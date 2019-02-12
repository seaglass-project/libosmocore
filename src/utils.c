/*
 * (C) 2011 by Harald Welte <laforge@gnumonks.org>
 * (C) 2011 by Sylvain Munaut <tnt@246tNt.com>
 * (C) 2014 by Nils O. Selåsdal <noselasd@fiane.dyndns.org>
 *
 * All Rights Reserved
 *
 * SPDX-License-Identifier: GPL-2.0+
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */


#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <inttypes.h>

#include <osmocom/core/utils.h>
#include <osmocom/core/bit64gen.h>


/*! \addtogroup utils
 * @{
 * various utility routines
 *
 * \file utils.c */

static char namebuf[255];

/*! get human-readable string for given value
 *  \param[in] vs Array of value_string tuples
 *  \param[in] val Value to be converted
 *  \returns pointer to human-readable string
 *
 * If val is found in vs, the array's string entry is returned. Otherwise, an
 * "unknown" string containing the actual value is composed in a static buffer
 * that is reused across invocations.
 */
const char *get_value_string(const struct value_string *vs, uint32_t val)
{
	const char *str = get_value_string_or_null(vs, val);
	if (str)
		return str;

	snprintf(namebuf, sizeof(namebuf), "unknown 0x%"PRIx32, val);
	namebuf[sizeof(namebuf) - 1] = '\0';
	return namebuf;
}

/*! get human-readable string or NULL for given value
 *  \param[in] vs Array of value_string tuples
 *  \param[in] val Value to be converted
 *  \returns pointer to human-readable string or NULL if val is not found
 */
const char *get_value_string_or_null(const struct value_string *vs,
				     uint32_t val)
{
	int i;

	for (i = 0;; i++) {
		if (vs[i].value == 0 && vs[i].str == NULL)
			break;
		if (vs[i].value == val)
			return vs[i].str;
	}

	return NULL;
}

/*! get numeric value for given human-readable string
 *  \param[in] vs Array of value_string tuples
 *  \param[in] str human-readable string
 *  \returns numeric value (>0) or negative numer in case of error
 */
int get_string_value(const struct value_string *vs, const char *str)
{
	int i;

	for (i = 0;; i++) {
		if (vs[i].value == 0 && vs[i].str == NULL)
			break;
		if (!strcasecmp(vs[i].str, str))
			return vs[i].value;
	}
	return -EINVAL;
}

/*! Convert BCD-encoded digit into printable character
 *  \param[in] bcd A single BCD-encoded digit
 *  \returns single printable character
 */
char osmo_bcd2char(uint8_t bcd)
{
	if (bcd < 0xa)
		return '0' + bcd;
	else
		return 'A' + (bcd - 0xa);
}

/*! Convert number in ASCII to BCD value
 *  \param[in] c ASCII character
 *  \returns BCD encoded value of character
 */
uint8_t osmo_char2bcd(char c)
{
	if (c >= '0' && c <= '9')
		return c - 0x30;
	else if (c >= 'A' && c <= 'F')
		return 0xa + (c - 'A');
	else if (c >= 'a' && c <= 'f')
		return 0xa + (c - 'a');
	else
		return 0;
}

/*! Convert BCD to string.
 * The given nibble offsets are interpreted in BCD order, i.e. nibble 0 is bcd[0] & 0xf, nibble 1 is bcd[0] >> 4, nibble
 * 3 is bcd[1] & 0xf, etc..
 *  \param[out] dst  Output string buffer, is always nul terminated when dst_size > 0.
 *  \param[in] dst_size  sizeof() the output string buffer.
 *  \param[in] bcd  Binary coded data buffer.
 *  \param[in] start_nibble  Offset to start from, in nibbles, typically 1 to skip the first nibble.
 *  \param[in] end_nibble  Offset to stop before, in nibbles, e.g. sizeof(bcd)*2 - (bcd[0] & GSM_MI_ODD? 0:1).
 *  \param[in] allow_hex  If false, return error if there are digits other than 0-9. If true, return those as [A-F].
 *  \returns The strlen that would be written if the output buffer is large enough, excluding nul byte (like
 *           snprintf()), or -EINVAL if allow_hex is false and a digit > 9 is encountered. On -EINVAL, the conversion is
 *           still completed as if allow_hex were passed as true. Return -ENOMEM if dst is NULL or dst_size is zero.
 *           If end_nibble <= start_nibble, write an empty string to dst and return 0.
 */
int osmo_bcd2str(char *dst, size_t dst_size, const uint8_t *bcd, int start_nibble, int end_nibble, bool allow_hex)
{
	char *dst_end = dst + dst_size - 1;
	int nibble_i;
	int rc = 0;

	if (!dst || dst_size < 1)
		return -ENOMEM;

	for (nibble_i = start_nibble; nibble_i < end_nibble && dst < dst_end; nibble_i++, dst++) {
		uint8_t nibble = bcd[nibble_i >> 1];
		if ((nibble_i & 1))
			nibble >>= 4;
		nibble &= 0xf;

		if (!allow_hex && nibble > 9)
			rc = -EINVAL;

		*dst = osmo_bcd2char(nibble);
	}
	*dst = '\0';

	if (rc < 0)
		return rc;
	return OSMO_MAX(0, end_nibble - start_nibble);
}

/*! Parse a string containing hexadecimal digits
 *  \param[in] str string containing ASCII encoded hexadecimal digits
 *  \param[out] b output buffer
 *  \param[in] max_len maximum space in output buffer
 *  \returns number of parsed octets, or -1 on error
 */
int osmo_hexparse(const char *str, uint8_t *b, int max_len)

{
	char c;
	uint8_t v;
	const char *strpos;
	unsigned int nibblepos = 0;

	memset(b, 0x00, max_len);

	for (strpos = str; (c = *strpos); strpos++) {
		/* skip whitespace */
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
			continue;

		/* If the buffer is too small, error out */
		if (nibblepos >= (max_len << 1))
			return -1;

		if (c >= '0' && c <= '9')
			v = c - '0';
		else if (c >= 'a' && c <= 'f')
			v = 10 + (c - 'a');
		else if (c >= 'A' && c <= 'F')
			v = 10 + (c - 'A');
		else
			return -1;

		b[nibblepos >> 1] |= v << (nibblepos & 1 ? 0 : 4);
		nibblepos ++;
	}

	/* In case of uneven amount of digits, the last byte is not complete
	 * and that's an error. */
	if (nibblepos & 1)
		return -1;

	return nibblepos >> 1;
}

static char hexd_buff[4096];
static const char hex_chars[] = "0123456789abcdef";

/*! Convert binary sequence to hexadecimal ASCII string.
 *  \param[out] out_buf  Output buffer to write the resulting string to.
 *  \param[in] out_buf_size  sizeof(out_buf).
 *  \param[in] buf  Input buffer, pointer to sequence of bytes.
 *  \param[in] len  Length of input buf in number of bytes.
 *  \param[in] delim  String to separate each byte; NULL or "" for no delim.
 *  \param[in] delim_after_last  If true, end the string in delim (true: "1a:ef:d9:", false: "1a:ef:d9");
 *                               if out_buf has insufficient space, the string will always end in a delim.
 *  \returns out_buf, containing a zero-terminated string, or "" (empty string) if out_buf == NULL or out_buf_size < 1.
 *
 * This function will print a sequence of bytes as hexadecimal numbers, adding one delim between each byte (e.g. for
 * delim passed as ":", return a string like "1a:ef:d9").
 *
 * The delim_after_last argument exists to be able to exactly show the original osmo_hexdump() behavior, which always
 * ends the string with a delimiter.
 */
const char *osmo_hexdump_buf(char *out_buf, size_t out_buf_size, const unsigned char *buf, int len, const char *delim,
			     bool delim_after_last)
{
	int i;
	char *cur = out_buf;
	size_t delim_len;

	if (!out_buf || !out_buf_size)
		return "";

	delim = delim ? : "";
	delim_len = strlen(delim);

	for (i = 0; i < len; i++) {
		const char *delimp = delim;
		int len_remain = out_buf_size - (cur - out_buf) - 1;
		if (len_remain < (2 + delim_len)
		    && !(!delim_after_last && i == (len - 1) && len_remain >= 2))
			break;

		*cur++ = hex_chars[buf[i] >> 4];
		*cur++ = hex_chars[buf[i] & 0xf];

		if (i == (len - 1) && !delim_after_last)
			break;

		while (len_remain > 1 && *delimp) {
			*cur++ = *delimp++;
			len_remain--;
		}
	}
	*cur = '\0';
	return out_buf;
}

/*! Convert a sequence of unpacked bits to ASCII string
 * \param[in] bits A sequence of unpacked bits
 * \param[in] len Length of bits
 */
char *osmo_ubit_dump(const uint8_t *bits, unsigned int len)
{
	int i;

	if (len > sizeof(hexd_buff)-1)
		len = sizeof(hexd_buff)-1;
	memset(hexd_buff, 0, sizeof(hexd_buff));

	for (i = 0; i < len; i++) {
		char outch;
		switch (bits[i]) {
		case 0:
			outch = '0';
			break;
		case 0xff:
			outch = '?';
			break;
		case 1:
			outch = '1';
			break;
		default:
			outch = 'E';
			break;
		}
		hexd_buff[i] = outch;
	}
	hexd_buff[sizeof(hexd_buff)-1] = 0;
	return hexd_buff;
}

/*! Convert binary sequence to hexadecimal ASCII string
 *  \param[in] buf pointer to sequence of bytes
 *  \param[in] len length of buf in number of bytes
 *  \returns pointer to zero-terminated string
 *
 * This function will print a sequence of bytes as hexadecimal numbers,
 * adding one space character between each byte (e.g. "1a ef d9")
 *
 * The maximum size of the output buffer is 4096 bytes, i.e. the maximum
 * number of input bytes that can be printed in one call is 1365!
 */
char *osmo_hexdump(const unsigned char *buf, int len)
{
	osmo_hexdump_buf(hexd_buff, sizeof(hexd_buff), buf, len, " ", true);
	return hexd_buff;
}

/*! Convert binary sequence to hexadecimal ASCII string
 *  \param[in] buf pointer to sequence of bytes
 *  \param[in] len length of buf in number of bytes
 *  \returns pointer to zero-terminated string
 *
 * This function will print a sequence of bytes as hexadecimal numbers,
 * without any space character between each byte (e.g. "1aefd9")
 *
 * The maximum size of the output buffer is 4096 bytes, i.e. the maximum
 * number of input bytes that can be printed in one call is 2048!
 */
char *osmo_hexdump_nospc(const unsigned char *buf, int len)
{
	osmo_hexdump_buf(hexd_buff, sizeof(hexd_buff), buf, len, "", true);
	return hexd_buff;
}

/* Compat with previous typo to preserve abi */
char *osmo_osmo_hexdump_nospc(const unsigned char *buf, int len)
#if defined(__MACH__) && defined(__APPLE__)
	;
#else
	__attribute__((weak, alias("osmo_hexdump_nospc")));
#endif

#include "../config.h"
#ifdef HAVE_CTYPE_H
#include <ctype.h>
/*! Convert an entire string to lower case
 *  \param[out] out output string, caller-allocated
 *  \param[in] in input string
 */
void osmo_str2lower(char *out, const char *in)
{
	unsigned int i;

	for (i = 0; i < strlen(in); i++)
		out[i] = tolower((const unsigned char)in[i]);
	out[strlen(in)] = '\0';
}

/*! Convert an entire string to upper case
 *  \param[out] out output string, caller-allocated
 *  \param[in] in input string
 */
void osmo_str2upper(char *out, const char *in)
{
	unsigned int i;

	for (i = 0; i < strlen(in); i++)
		out[i] = toupper((const unsigned char)in[i]);
	out[strlen(in)] = '\0';
}
#endif /* HAVE_CTYPE_H */

/*! Wishful thinking to generate a constant time compare
 *  \param[in] exp Expected data
 *  \param[in] rel Comparison value
 *  \param[in] count Number of bytes to compare
 *  \returns 1 in case \a exp equals \a rel; zero otherwise
 *
 * Compare count bytes of exp to rel. Return 0 if they are identical, 1
 * otherwise. Do not return a mismatch on the first mismatching byte,
 * but always compare all bytes, regardless. The idea is that the amount of
 * matching bytes cannot be inferred from the time the comparison took. */
int osmo_constant_time_cmp(const uint8_t *exp, const uint8_t *rel, const int count)
{
	int x = 0, i;

	for (i = 0; i < count; ++i)
		x |= exp[i] ^ rel[i];

	/* if x is zero, all data was identical */
	return x? 1 : 0;
}

/*! Generic retrieval of 1..8 bytes as big-endian uint64_t
 *  \param[in] data Input data as byte-array
 *  \param[in] data_len Length of \a data in octets
 *  \returns uint64_t of \a data interpreted as big-endian
 *
 * This is like osmo_load64be_ext, except that if data_len is less than
 * sizeof(uint64_t), the data is interpreted as the least significant bytes
 * (osmo_load64be_ext loads them as the most significant bytes into the
 * returned uint64_t). In this way, any integer size up to 64 bits can be
 * decoded conveniently by using sizeof(), without the need to call specific
 * numbered functions (osmo_load16, 32, ...). */
uint64_t osmo_decode_big_endian(const uint8_t *data, size_t data_len)
{
	uint64_t value = 0;

	while (data_len > 0) {
		value = (value << 8) + *data;
		data += 1;
		data_len -= 1;
	}

	return value;
}

/*! Generic big-endian encoding of big endian number up to 64bit
 *  \param[in] value unsigned integer value to be stored
 *  \param[in] data_len number of octets 
 *  \returns static buffer containing big-endian stored value
 *
 * This is like osmo_store64be_ext, except that this returns a static buffer of
 * the result (for convenience, but not threadsafe). If data_len is less than
 * sizeof(uint64_t), only the least significant bytes of value are encoded. */
uint8_t *osmo_encode_big_endian(uint64_t value, size_t data_len)
{
	static uint8_t buf[sizeof(uint64_t)];
	OSMO_ASSERT(data_len <= ARRAY_SIZE(buf));
	osmo_store64be_ext(value, buf, data_len);
	return buf;
}

/*! Copy a C-string into a sized buffer
 *  \param[in] src source string
 *  \param[out] dst destination string
 *  \param[in] siz size of the \a dst buffer
 *  \returns length of \a src
 *
 * Copy at most \a siz bytes from \a src to \a dst, ensuring that the result is
 * NUL terminated. The NUL character is included in \a siz, i.e. passing the
 * actual sizeof(*dst) is correct.
 */
size_t osmo_strlcpy(char *dst, const char *src, size_t siz)
{
	size_t ret = src ? strlen(src) : 0;

	if (siz) {
		size_t len = (ret >= siz) ? siz - 1 : ret;
		if (src)
			memcpy(dst, src, len);
		dst[len] = '\0';
	}
	return ret;
}

/*! Validate that a given string is a hex string within given size limits.
 * Note that each hex digit amounts to a nibble, so if checking for a hex
 * string to result in N bytes, pass amount of digits as 2*N.
 * \param str  A nul-terminated string to validate, or NULL.
 * \param min_digits  least permitted amount of digits.
 * \param max_digits  most permitted amount of digits.
 * \param require_even  if true, require an even amount of digits.
 * \returns true when the hex_str contains only hexadecimal digits (no
 *          whitespace) and matches the requested length; also true
 *          when min_digits <= 0 and str is NULL.
 */
bool osmo_is_hexstr(const char *str, int min_digits, int max_digits,
		    bool require_even)
{
	int len;
	/* Use unsigned char * to avoid a compiler warning of
	 * "error: array subscript has type 'char' [-Werror=char-subscripts]" */
	const unsigned char *pos = (const unsigned char*)str;
	if (!pos)
		return min_digits < 1;
	for (len = 0; *pos && len < max_digits; len++, pos++)
		if (!isxdigit(*pos))
			return false;
	if (len < min_digits)
		return false;
	/* With not too many digits, we should have reached *str == nul */
	if (*pos)
		return false;
	if (require_even && (len & 1))
		return false;

	return true;
}

/*! Determine if a given identifier is valid, i.e. doesn't contain illegal chars
 *  \param[in] str String to validate
 *  \param[in] sep_chars Permitted separation characters between identifiers.
 *  \returns true in case \a str contains only valid identifiers and sep_chars, false otherwise
 */
bool osmo_separated_identifiers_valid(const char *str, const char *sep_chars)
{
	/* characters that are illegal in names */
	static const char illegal_chars[] = "., {}[]()<>|~\\^`'\"?=;/+*&%$#!";
	unsigned int i;
	size_t len;

	/* an empty string is not a valid identifier */
	if (!str || (len = strlen(str)) == 0)
		return false;

	for (i = 0; i < len; i++) {
		if (sep_chars && strchr(sep_chars, str[i]))
			continue;
		/* check for 7-bit ASCII */
		if (str[i] & 0x80)
			return false;
		if (!isprint((int)str[i]))
			return false;
		/* check for some explicit reserved control characters */
		if (strchr(illegal_chars, str[i]))
			return false;
	}

	return true;
}

/*! Determine if a given identifier is valid, i.e. doesn't contain illegal chars
 *  \param[in] str String to validate
 *  \returns true in case \a str contains valid identifier, false otherwise
 */
bool osmo_identifier_valid(const char *str)
{
	return osmo_separated_identifiers_valid(str, NULL);
}

/*! Return the string with all non-printable characters escaped.
 * \param[in] str  A string that may contain any characters.
 * \param[in] len  Pass -1 to print until nul char, or >= 0 to force a length.
 * \param[inout] buf  string buffer to write escaped characters to.
 * \param[in] bufsize  size of \a buf.
 * \returns buf containing an escaped representation, possibly truncated, or str itself.
 */
const char *osmo_escape_str_buf(const char *str, int in_len, char *buf, size_t bufsize)
{
	int in_pos = 0;
	int next_unprintable = 0;
	int out_pos = 0;
	char *out = buf;
	/* -1 to leave space for a final \0 */
	int out_len = bufsize-1;

	if (!str)
		return "(null)";

	if (in_len < 0)
		in_len = strlen(str);

	while (in_pos < in_len) {
		for (next_unprintable = in_pos;
		     next_unprintable < in_len && isprint((int)str[next_unprintable])
		     && str[next_unprintable] != '"'
		     && str[next_unprintable] != '\\';
		     next_unprintable++);

		if (next_unprintable == in_len
		    && in_pos == 0)
			return str;

		while (in_pos < next_unprintable && out_pos < out_len)
			out[out_pos++] = str[in_pos++];

		if (out_pos == out_len || in_pos == in_len)
			goto done;

		switch (str[next_unprintable]) {
#define BACKSLASH_CASE(c, repr) \
		case c: \
			if (out_pos > out_len-2) \
				goto done; \
			out[out_pos++] = '\\'; \
			out[out_pos++] = repr; \
			break

		BACKSLASH_CASE('\n', 'n');
		BACKSLASH_CASE('\r', 'r');
		BACKSLASH_CASE('\t', 't');
		BACKSLASH_CASE('\0', '0');
		BACKSLASH_CASE('\a', 'a');
		BACKSLASH_CASE('\b', 'b');
		BACKSLASH_CASE('\v', 'v');
		BACKSLASH_CASE('\f', 'f');
		BACKSLASH_CASE('\\', '\\');
		BACKSLASH_CASE('"', '"');
#undef BACKSLASH_CASE

		default:
			out_pos += snprintf(&out[out_pos], out_len - out_pos, "\\%u", (unsigned char)str[in_pos]);
			if (out_pos > out_len) {
				out_pos = out_len;
				goto done;
			}
			break;
		}
		in_pos ++;
	}

done:
	out[out_pos] = '\0';
	return out;
}

/*! Return the string with all non-printable characters escaped.
 * Call osmo_escape_str_buf() with a static buffer.
 * \param[in] str  A string that may contain any characters.
 * \param[in] len  Pass -1 to print until nul char, or >= 0 to force a length.
 * \returns buf containing an escaped representation, possibly truncated, or str itself.
 */
const char *osmo_escape_str(const char *str, int in_len)
{
	return osmo_escape_str_buf(str, in_len, namebuf, sizeof(namebuf));
}

/*! Like osmo_escape_str(), but returns double-quotes around a string, or "NULL" for a NULL string.
 * This allows passing any char* value and get its C representation as string.
 * \param[in] str  A string that may contain any characters.
 * \param[in] in_len  Pass -1 to print until nul char, or >= 0 to force a length.
 * \returns buf containing a quoted and escaped representation, possibly truncated.
 */
const char *osmo_quote_str_buf(const char *str, int in_len, char *buf, size_t bufsize)
{
	const char *res;
	int l;
	if (!str)
		return "NULL";
	if (bufsize < 3)
		return "<buf-too-small>";
	buf[0] = '"';
	res = osmo_escape_str_buf(str, in_len, buf + 1, bufsize - 2);
	/* if osmo_escape_str_buf() returned the str itself, we need to copy it to buf to be able to
	 * quote it. */
	if (res == str) {
		/* max_len = bufsize - two quotes - nul term */
		int max_len = bufsize - 2 - 1;
		if (in_len >= 0)
			max_len = OSMO_MIN(in_len, max_len);
		/* It is not allowed to pass unterminated strings into osmo_strlcpy() :/ */
		strncpy(buf + 1, str, max_len);
		buf[1 + max_len] = '\0';
	}
	l = strlen(buf);
	buf[l] = '"';
	buf[l+1] = '\0'; /* both osmo_escape_str_buf() and max_len above ensure room for '\0' */
	return buf;
}

/*! Like osmo_quote_str_buf() but returns the result in a static buffer.
 * The static buffer is shared with get_value_string() and osmo_escape_str().
 * \param[in] str  A string that may contain any characters.
 * \param[in] in_len  Pass -1 to print until nul char, or >= 0 to force a length.
 * \returns static buffer containing a quoted and escaped representation, possibly truncated.
 */
const char *osmo_quote_str(const char *str, int in_len)
{
	return osmo_quote_str_buf(str, in_len, namebuf, sizeof(namebuf));
}

/*! perform an integer square root operation on unsigned 32bit integer.
 *  This implementation is taken from "Hacker's Delight" Figure 11-1 "Integer square root, Newton's
 *  method", which can also be found at http://www.hackersdelight.org/hdcodetxt/isqrt.c.txt */
uint32_t osmo_isqrt32(uint32_t x)
{
	uint32_t x1;
	int s, g0, g1;

	if (x <= 1)
		return x;

	s = 1;
	x1 = x - 1;
	if (x1 > 0xffff) {
		s = s + 8;
		x1 = x1 >> 16;
	}
	if (x1 > 0xff) {
		s = s + 4;
		x1 = x1 >> 8;
	}
	if (x1 > 0xf) {
		s = s + 2;
		x1 = x1 >> 4;
	}
	if (x1 > 0x3) {
		s = s + 1;
	}

	g0 = 1 << s;			/* g0 = 2**s */
	g1 = (g0 + (x >> s)) >> 1;	/* g1 = (g0 + x/g0)/2 */

	/* converges after four to five divisions for arguments up to 16,785,407 */
	while (g1 < g0) {
		g0 = g1;
		g1 = (g0 + (x/g0)) >> 1;
	}
	return g0;
}

/*! Convert a string to lowercase, while checking buffer size boundaries.
 * The result written to \a dest is guaranteed to be nul terminated if \a dest_len > 0.
 * If dest == src, the string is converted in-place, if necessary truncated at dest_len - 1 characters
 * length as well as nul terminated.
 * Note: similar osmo_str2lower(), but safe to use for src strings of arbitrary length.
 *  \param[out] dest  Target buffer to write lowercase string.
 *  \param[in] dest_len  Maximum buffer size of dest (e.g. sizeof(dest)).
 *  \param[in] src  String to convert to lowercase.
 *  \returns Length of \a src, like osmo_strlcpy(), but if \a dest == \a src at most \a dest_len - 1.
 */
size_t osmo_str_tolower_buf(char *dest, size_t dest_len, const char *src)
{
	size_t rc;
	if (dest == src) {
		if (dest_len < 1)
			return 0;
		dest[dest_len - 1] = '\0';
		rc = strlen(dest);
	} else {
		if (dest_len < 1)
			return strlen(src);
		rc = osmo_strlcpy(dest, src, dest_len);
	}
	for (; *dest; dest++)
		*dest = tolower(*dest);
	return rc;
}

/*! Convert a string to lowercase, using a static buffer.
 * The resulting string may be truncated if the internally used static buffer is shorter than src.
 * The internal buffer is at least 128 bytes long, i.e. guaranteed to hold at least 127 characters and a
 * terminating nul.
 * See also osmo_str_tolower_buf().
 * \param[in] src  String to convert to lowercase.
 * \returns Resulting lowercase string in a static buffer, always nul terminated.
 */
const char *osmo_str_tolower(const char *src)
{
	static char buf[128];
	osmo_str_tolower_buf(buf, sizeof(buf), src);
	return buf;
}

/*! Convert a string to uppercase, while checking buffer size boundaries.
 * The result written to \a dest is guaranteed to be nul terminated if \a dest_len > 0.
 * If dest == src, the string is converted in-place, if necessary truncated at dest_len - 1 characters
 * length as well as nul terminated.
 * Note: similar osmo_str2upper(), but safe to use for src strings of arbitrary length.
 *  \param[out] dest  Target buffer to write uppercase string.
 *  \param[in] dest_len  Maximum buffer size of dest (e.g. sizeof(dest)).
 *  \param[in] src  String to convert to uppercase.
 *  \returns Length of \a src, like osmo_strlcpy(), but if \a dest == \a src at most \a dest_len - 1.
 */
size_t osmo_str_toupper_buf(char *dest, size_t dest_len, const char *src)
{
	size_t rc;
	if (dest == src) {
		if (dest_len < 1)
			return 0;
		dest[dest_len - 1] = '\0';
		rc = strlen(dest);
	} else {
		if (dest_len < 1)
			return strlen(src);
		rc = osmo_strlcpy(dest, src, dest_len);
	}
	for (; *dest; dest++)
		*dest = toupper(*dest);
	return rc;
}

/*! Convert a string to uppercase, using a static buffer.
 * The resulting string may be truncated if the internally used static buffer is shorter than src.
 * The internal buffer is at least 128 bytes long, i.e. guaranteed to hold at least 127 characters and a
 * terminating nul.
 * See also osmo_str_toupper_buf().
 * \param[in] src  String to convert to uppercase.
 * \returns Resulting uppercase string in a static buffer, always nul terminated.
 */
const char *osmo_str_toupper(const char *src)
{
	static char buf[128];
	osmo_str_toupper_buf(buf, sizeof(buf), src);
	return buf;
}

/*! Calculate the Luhn checksum (as used for IMEIs).
 * \param[in] in  Input digits in ASCII string representation.
 * \param[in] in_len  Count of digits to use for the input (14 for IMEI).
 * \returns checksum char (e.g. '3'); negative on error
 */
const char osmo_luhn(const char* in, int in_len)
{
	int i, sum = 0;

	/* All input must be numbers */
	for (i = 0; i < in_len; i++) {
		if (!isdigit(in[i]))
			return -EINVAL;
	}

	/* Double every second digit and add it to sum */
	for (i = in_len - 1; i >= 0; i -= 2) {
		int dbl = (in[i] - '0') * 2;
		if (dbl > 9)
			dbl -= 9;
		sum += dbl;
	}

	/* Add other digits to sum */
	for (i = in_len - 2; i >= 0; i -= 2)
		sum += in[i] - '0';

	/* Final checksum */
	return (sum * 9) % 10 + '0';
}

/*! @} */
