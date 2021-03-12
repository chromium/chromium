// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/escape.h"

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/third_party/icu/icu_utf.h"

namespace base {

namespace {

// Contains nonzero when the corresponding character is unescapable for normal
// URLs. These characters are the ones that may change the parsing of a URL, so
// we don't want to unescape them sometimes. In many case we won't want to
// unescape spaces, but that is controlled by parameters to Unescape*.
//
// The basic rule is that we can't unescape anything that would changing parsing
// like # or ?. We also can't unescape &, =, or + since that could be part of a
// query and that could change the server's parsing of the query. Nor can we
// unescape \ since src/url/ will convert it to a /.
//
// Lastly, we can't unescape anything that doesn't have a canonical
// representation in a URL. This means that unescaping will change the URL, and
// you could get different behavior if you copy and paste the URL, or press
// enter in the URL bar. The list of characters that fall into this category
// are the ones labeled PASS (allow either escaped or unescaped) in the big
// lookup table at the top of url/url_canon_path.cc.  Also, characters
// that have CHAR_QUERY set in url/url_canon_internal.cc but are not
// allowed in query strings according to http://www.ietf.org/rfc/rfc3261.txt are
// not unescaped, to avoid turning a valid url according to spec into an
// invalid one.
// clang-format off
const char kUrlUnescape[128] = {
//   Null, control chars...
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//  ' ' !  "  #  $  %  &  '  (  )  *  +  ,  -  .  /
     0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
//   0  1  2  3  4  5  6  7  8  9  :  ;  <  =  >  ?
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1, 0,
//   @  A  B  C  D  E  F  G  H  I  J  K  L  M  N  O
     0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
//   P  Q  R  S  T  U  V  W  X  Y  Z  [  \  ]  ^  _
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,
//   `  a  b  c  d  e  f  g  h  i  j  k  l  m  n  o
     0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
//   p  q  r  s  t  u  v  w  x  y  z  {  |  }  ~  <NBSP>
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0,
};
// clang-format on

// Attempts to unescape the sequence at |index| within |escaped_text|.  If
// successful, sets |value| to the unescaped value.  Returns whether
// unescaping succeeded.
bool UnescapeUnsignedByteAtIndex(StringPiece escaped_text,
                                 size_t index,
                                 unsigned char* value) {
  if ((index + 2) >= escaped_text.size())
    return false;
  if (escaped_text[index] != '%')
    return false;
  char most_sig_digit(escaped_text[index + 1]);
  char least_sig_digit(escaped_text[index + 2]);
  if (IsHexDigit(most_sig_digit) && IsHexDigit(least_sig_digit)) {
    *value =
        HexDigitToInt(most_sig_digit) * 16 + HexDigitToInt(least_sig_digit);
    return true;
  }
  return false;
}

// Attempts to unescape and decode a UTF-8-encoded percent-escaped character at
// the specified index. On success, returns true, sets |code_point_out| to be
// the character's code point and |unescaped_out| to be the unescaped UTF-8
// string. |unescaped_out| will always be 1/3rd the length of the substring of
// |escaped_text| that corresponds to the unescaped character.
bool UnescapeUTF8CharacterAtIndex(StringPiece escaped_text,
                                  size_t index,
                                  uint32_t* code_point_out,
                                  std::string* unescaped_out) {
  DCHECK(unescaped_out->empty());

  unsigned char bytes[CBU8_MAX_LENGTH];
  if (!UnescapeUnsignedByteAtIndex(escaped_text, index, &bytes[0]))
    return false;

  size_t num_bytes = 1;

  // If this is a lead byte, need to collect trail bytes as well.
  if (CBU8_IS_LEAD(bytes[0])) {
    // Look for the last trail byte of the UTF-8 character.  Give up once
    // reach max character length number of bytes, or hit an unescaped
    // character. No need to check length of escaped_text, as
    // UnescapeUnsignedByteAtIndex checks lengths.
    while (num_bytes < size(bytes) &&
           UnescapeUnsignedByteAtIndex(escaped_text, index + num_bytes * 3,
                                       &bytes[num_bytes]) &&
           CBU8_IS_TRAIL(bytes[num_bytes])) {
      ++num_bytes;
    }
  }

  int32_t char_index = 0;
  // Check if the unicode "character" that was just unescaped is valid.
  if (!ReadUnicodeCharacter(reinterpret_cast<char*>(bytes), num_bytes,
                            &char_index, code_point_out)) {
    return false;
  }

  // It's possible that a prefix of |bytes| forms a valid UTF-8 character,
  // and the rest are not valid UTF-8, so need to update |num_bytes| based
  // on the result of ReadUnicodeCharacter().
  num_bytes = char_index + 1;
  *unescaped_out = std::string(reinterpret_cast<char*>(bytes), num_bytes);
  return true;
}

// This method takes a Unicode code point and returns true if it should be
// unescaped, based on |rules|.
bool ShouldUnescapeCodePoint(UnescapeRule::Type rules, uint32_t code_point) {
  // If this is an ASCII character, use the lookup table.
  if (code_point < 0x80) {
    return kUrlUnescape[code_point] ||
           // Allow some additional unescaping when flags are set.
           (code_point == ' ' && (rules & UnescapeRule::SPACES)) ||
           // Allow any of the prohibited but non-control characters when doing
           // "special" chars.
           ((code_point == '/' || code_point == '\\') &&
            (rules & UnescapeRule::PATH_SEPARATORS)) ||
           (code_point > ' ' && code_point != '/' && code_point != '\\' &&
            (rules & UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS));
  }

  // Compare the code point against a list of characters that can be used
  // to spoof other URLs.
  //
  // Can't use icu to make this cleaner, because Cronet cannot depend on
  // icu, and currently uses this file.
  // TODO(https://crbug.com/829873): Try to make this use icu, both to
  // protect against regressions as the Unicode standard is updated and to
  // reduce the number of long lists of characters.
  return !(
      // Per http://tools.ietf.org/html/rfc3987#section-4.1, certain BiDi
      // control characters are not allowed to appear unescaped in URLs.
      code_point == 0x200E ||  // LEFT-TO-RIGHT MARK         (%E2%80%8E)
      code_point == 0x200F ||  // RIGHT-TO-LEFT MARK         (%E2%80%8F)
      code_point == 0x202A ||  // LEFT-TO-RIGHT EMBEDDING    (%E2%80%AA)
      code_point == 0x202B ||  // RIGHT-TO-LEFT EMBEDDING    (%E2%80%AB)
      code_point == 0x202C ||  // POP DIRECTIONAL FORMATTING (%E2%80%AC)
      code_point == 0x202D ||  // LEFT-TO-RIGHT OVERRIDE     (%E2%80%AD)
      code_point == 0x202E ||  // RIGHT-TO-LEFT OVERRIDE     (%E2%80%AE)

      // The Unicode Technical Report (TR9) as referenced by RFC 3987 above has
      // since added some new BiDi control characters that are not safe to
      // unescape. http://www.unicode.org/reports/tr9
      code_point == 0x061C ||  // ARABIC LETTER MARK         (%D8%9C)
      code_point == 0x2066 ||  // LEFT-TO-RIGHT ISOLATE      (%E2%81%A6)
      code_point == 0x2067 ||  // RIGHT-TO-LEFT ISOLATE      (%E2%81%A7)
      code_point == 0x2068 ||  // FIRST STRONG ISOLATE       (%E2%81%A8)
      code_point == 0x2069 ||  // POP DIRECTIONAL ISOLATE    (%E2%81%A9)

      // The following spoofable characters are also banned in unescaped URLs,
      // because they could be used to imitate parts of a web browser's UI.
      code_point == 0x1F50F ||  // LOCK WITH INK PEN    (%F0%9F%94%8F)
      code_point == 0x1F510 ||  // CLOSED LOCK WITH KEY (%F0%9F%94%90)
      code_point == 0x1F512 ||  // LOCK                 (%F0%9F%94%92)
      code_point == 0x1F513 ||  // OPEN LOCK            (%F0%9F%94%93)

      // Spaces are also banned, as they can be used to scroll text out of view.
      code_point == 0x0085 ||  // NEXT LINE                  (%C2%85)
      code_point == 0x00A0 ||  // NO-BREAK SPACE             (%C2%A0)
      code_point == 0x1680 ||  // OGHAM SPACE MARK           (%E1%9A%80)
      code_point == 0x2000 ||  // EN QUAD                    (%E2%80%80)
      code_point == 0x2001 ||  // EM QUAD                    (%E2%80%81)
      code_point == 0x2002 ||  // EN SPACE                   (%E2%80%82)
      code_point == 0x2003 ||  // EM SPACE                   (%E2%80%83)
      code_point == 0x2004 ||  // THREE-PER-EM SPACE         (%E2%80%84)
      code_point == 0x2005 ||  // FOUR-PER-EM SPACE          (%E2%80%85)
      code_point == 0x2006 ||  // SIX-PER-EM SPACE           (%E2%80%86)
      code_point == 0x2007 ||  // FIGURE SPACE               (%E2%80%87)
      code_point == 0x2008 ||  // PUNCTUATION SPACE          (%E2%80%88)
      code_point == 0x2009 ||  // THIN SPACE                 (%E2%80%89)
      code_point == 0x200A ||  // HAIR SPACE                 (%E2%80%8A)
      code_point == 0x2028 ||  // LINE SEPARATOR             (%E2%80%A8)
      code_point == 0x2029 ||  // PARAGRAPH SEPARATOR        (%E2%80%A9)
      code_point == 0x202F ||  // NARROW NO-BREAK SPACE      (%E2%80%AF)
      code_point == 0x205F ||  // MEDIUM MATHEMATICAL SPACE  (%E2%81%9F)
      code_point == 0x3000 ||  // IDEOGRAPHIC SPACE          (%E3%80%80)
      // U+2800 is rendered as a space, but is not considered whitespace (see
      // crbug.com/1068531).
      code_point == 0x2800 ||  // BRAILLE PATTERN BLANK      (%E2%A0%80)

      // Default Ignorable ([:Default_Ignorable_Code_Point=Yes:]) and Format
      // characters ([:Cf:]) are also banned (see crbug.com/824715).
      code_point == 0x00AD ||  // SOFT HYPHEN               (%C2%AD)
      code_point == 0x034F ||  // COMBINING GRAPHEME JOINER (%CD%8F)
      // Arabic number formatting
      (code_point >= 0x0600 && code_point <= 0x0605) ||
      // U+061C is already banned as a BiDi control character.
      code_point == 0x06DD ||  // ARABIC END OF AYAH          (%DB%9D)
      code_point == 0x070F ||  // SYRIAC ABBREVIATION MARK    (%DC%8F)
      code_point == 0x08E2 ||  // ARABIC DISPUTED END OF AYAH (%E0%A3%A2)
      code_point == 0x115F ||  // HANGUL CHOSEONG FILLER      (%E1%85%9F)
      code_point == 0x1160 ||  // HANGUL JUNGSEONG FILLER     (%E1%85%A0)
      code_point == 0x17B4 ||  // KHMER VOWEL INHERENT AQ     (%E1%9E%B4)
      code_point == 0x17B5 ||  // KHMER VOWEL INHERENT AA     (%E1%9E%B5)
      code_point == 0x180B ||  // MONGOLIAN FREE VARIATION SELECTOR ONE
                               // (%E1%A0%8B)
      code_point == 0x180C ||  // MONGOLIAN FREE VARIATION SELECTOR TWO
                               // (%E1%A0%8C)
      code_point == 0x180D ||  // MONGOLIAN FREE VARIATION SELECTOR THREE
                               // (%E1%A0%8D)
      code_point == 0x180E ||  // MONGOLIAN VOWEL SEPARATOR   (%E1%A0%8E)
      code_point == 0x200B ||  // ZERO WIDTH SPACE            (%E2%80%8B)
      code_point == 0x200C ||  // ZERO WIDTH SPACE NON-JOINER (%E2%80%8C)
      code_point == 0x200D ||  // ZERO WIDTH JOINER           (%E2%80%8D)
      // U+200E, U+200F, U+202A--202E, and U+2066--2069 are already banned as
      // BiDi control characters.
      code_point == 0x2060 ||  // WORD JOINER          (%E2%81%A0)
      code_point == 0x2061 ||  // FUNCTION APPLICATION (%E2%81%A1)
      code_point == 0x2062 ||  // INVISIBLE TIMES      (%E2%81%A2)
      code_point == 0x2063 ||  // INVISIBLE SEPARATOR  (%E2%81%A3)
      code_point == 0x2064 ||  // INVISIBLE PLUS       (%E2%81%A4)
      code_point == 0x2065 ||  // null (%E2%81%A5)
      // 0x2066--0x2069 are already banned as a BiDi control characters.
      // General Punctuation - Deprecated (U+206A--206F)
      (code_point >= 0x206A && code_point <= 0x206F) ||
      code_point == 0x3164 ||  // HANGUL FILLER (%E3%85%A4)
      (code_point >= 0xFFF0 && code_point <= 0xFFF8) ||  // null
      // Variation selectors (%EF%B8%80 -- %EF%B8%8F)
      (code_point >= 0xFE00 && code_point <= 0xFE0F) ||
      code_point == 0xFEFF ||   // ZERO WIDTH NO-BREAK SPACE (%EF%BB%BF)
      code_point == 0xFFA0 ||   // HALFWIDTH HANGUL FILLER (%EF%BE%A0)
      code_point == 0xFFF9 ||   // INTERLINEAR ANNOTATION ANCHOR     (%EF%BF%B9)
      code_point == 0xFFFA ||   // INTERLINEAR ANNOTATION SEPARATOR  (%EF%BF%BA)
      code_point == 0xFFFB ||   // INTERLINEAR ANNOTATION TERMINATOR (%EF%BF%BB)
      code_point == 0x110BD ||  // KAITHI NUMBER SIGN       (%F0%91%82%BD)
      code_point == 0x110CD ||  // KAITHI NUMBER SIGN ABOVE (%F0%91%83%8D)
      // Egyptian hieroglyph formatting (%F0%93%90%B0 -- %F0%93%90%B8)
      (code_point >= 0x13430 && code_point <= 0x13438) ||
      // Shorthand format controls (%F0%9B%B2%A0 -- %F0%9B%B2%A3)
      (code_point >= 0x1BCA0 && code_point <= 0x1BCA3) ||
      // Beams and slurs (%F0%9D%85%B3 -- %F0%9D%85%BA)
      (code_point >= 0x1D173 && code_point <= 0x1D17A) ||
      // Tags, Variation Selectors, nulls
      (code_point >= 0xE0000 && code_point <= 0xE0FFF));
}

// Unescapes |escaped_text| according to |rules|, returning the resulting
// string.  Fills in an |adjustments| parameter, if non-nullptr, so it reflects
// the alterations done to the string that are not one-character-to-one-
// character.  The resulting |adjustments| will always be sorted by increasing
// offset.
std::string UnescapeURLWithAdjustmentsImpl(
    StringPiece escaped_text,
    UnescapeRule::Type rules,
    OffsetAdjuster::Adjustments* adjustments) {
  if (adjustments)
    adjustments->clear();
  // Do not unescape anything, return the |escaped_text| text.
  if (rules == UnescapeRule::NONE)
    return std::string(escaped_text);

  // The output of the unescaping is always smaller than the input, so we can
  // reserve the input size to make sure we have enough buffer and don't have
  // to allocate in the loop below.
  std::string result;
  result.reserve(escaped_text.length());

  // Locations of adjusted text.
  for (size_t i = 0, max = escaped_text.size(); i < max;) {
    // Try to unescape the character.
    uint32_t code_point;
    std::string unescaped;
    if (!UnescapeUTF8CharacterAtIndex(escaped_text, i, &code_point,
                                      &unescaped)) {
      // Check if the next character can be unescaped, but not as a valid UTF-8
      // character. In that case, just unescaped and write the non-sense
      // character.
      //
      // TODO(https://crbug.com/829868): Do not unescape illegal UTF-8
      // sequences.
      unsigned char non_utf8_byte;
      if (UnescapeUnsignedByteAtIndex(escaped_text, i, &non_utf8_byte)) {
        result.push_back(non_utf8_byte);
        if (adjustments)
          adjustments->push_back(OffsetAdjuster::Adjustment(i, 3, 1));
        i += 3;
        continue;
      }

      // Character is not escaped, so append as is, unless it's a '+' and
      // REPLACE_PLUS_WITH_SPACE is being applied.
      if (escaped_text[i] == '+' &&
          (rules & UnescapeRule::REPLACE_PLUS_WITH_SPACE)) {
        result.push_back(' ');
      } else {
        result.push_back(escaped_text[i]);
      }
      ++i;
      continue;
    }

    DCHECK(!unescaped.empty());

    if (!ShouldUnescapeCodePoint(rules, code_point)) {
      // If it's a valid UTF-8 character, but not safe to unescape, copy all
      // bytes directly.
      result.append(escaped_text.begin() + i,
                    escaped_text.begin() + i + 3 * unescaped.length());
      i += unescaped.length() * 3;
      continue;
    }

    // If the code point is allowed, and append the entire unescaped character.
    result.append(unescaped);
    if (adjustments) {
      for (size_t j = 0; j < unescaped.length(); ++j) {
        adjustments->push_back(OffsetAdjuster::Adjustment(i + j * 3, 3, 1));
      }
    }
    i += 3 * unescaped.length();
  }

  return result;
}

}  // namespace

std::string UnescapeURLComponent(StringPiece escaped_text,
                                 UnescapeRule::Type rules) {
  return UnescapeURLWithAdjustmentsImpl(escaped_text, rules, nullptr);
}

std::u16string UnescapeAndDecodeUTF8URLComponentWithAdjustments(
    StringPiece text,
    UnescapeRule::Type rules,
    OffsetAdjuster::Adjustments* adjustments) {
  std::u16string result;
  OffsetAdjuster::Adjustments unescape_adjustments;
  std::string unescaped_url(
      UnescapeURLWithAdjustmentsImpl(text, rules, &unescape_adjustments));
  if (UTF8ToUTF16WithAdjustments(unescaped_url.data(), unescaped_url.length(),
                                 &result, adjustments)) {
    // Character set looks like it's valid.
    if (adjustments) {
      OffsetAdjuster::MergeSequentialAdjustments(unescape_adjustments,
                                                 adjustments);
    }
    return result;
  }
  // Character set is not valid.  Return the escaped version.
  return UTF8ToUTF16WithAdjustments(text, adjustments);
}

std::string UnescapeBinaryURLComponent(StringPiece escaped_text,
                                       UnescapeRule::Type rules) {
  // Only NORMAL and REPLACE_PLUS_WITH_SPACE are supported.
  DCHECK(rules != UnescapeRule::NONE);
  DCHECK(!(rules &
           ~(UnescapeRule::NORMAL | UnescapeRule::REPLACE_PLUS_WITH_SPACE)));

  std::string unescaped_text;

  // The output of the unescaping is always smaller than the input, so we can
  // reserve the input size to make sure we have enough buffer and don't have
  // to allocate in the loop below.
  // Increase capacity before size, as just resizing can grow capacity
  // needlessly beyond our requested size.
  unescaped_text.reserve(escaped_text.size());
  unescaped_text.resize(escaped_text.size());

  size_t output_index = 0;

  for (size_t i = 0, max = escaped_text.size(); i < max;) {
    unsigned char byte;
    // UnescapeUnsignedByteAtIndex does bounds checking, so this is always safe
    // to call.
    if (UnescapeUnsignedByteAtIndex(escaped_text, i, &byte)) {
      unescaped_text[output_index++] = byte;
      i += 3;
      continue;
    }

    if ((rules & UnescapeRule::REPLACE_PLUS_WITH_SPACE) &&
        escaped_text[i] == '+') {
      unescaped_text[output_index++] = ' ';
      ++i;
      continue;
    }

    unescaped_text[output_index++] = escaped_text[i++];
  }

  DCHECK_LE(output_index, unescaped_text.size());
  unescaped_text.resize(output_index);
  return unescaped_text;
}

bool UnescapeBinaryURLComponentSafe(StringPiece escaped_text,
                                    bool fail_on_path_separators,
                                    std::string* unescaped_text) {
  unescaped_text->clear();

  std::set<unsigned char> illegal_encoded_bytes;
  for (char c = '\x00'; c < '\x20'; ++c) {
    illegal_encoded_bytes.insert(c);
  }
  if (fail_on_path_separators) {
    illegal_encoded_bytes.insert('/');
    illegal_encoded_bytes.insert('\\');
  }
  if (ContainsEncodedBytes(escaped_text, illegal_encoded_bytes))
    return false;

  *unescaped_text = UnescapeBinaryURLComponent(escaped_text);
  return true;
}

bool ContainsEncodedBytes(StringPiece escaped_text,
                          const std::set<unsigned char>& bytes) {
  for (size_t i = 0, max = escaped_text.size(); i < max;) {
    unsigned char byte;
    // UnescapeUnsignedByteAtIndex does bounds checking, so this is always safe
    // to call.
    if (UnescapeUnsignedByteAtIndex(escaped_text, i, &byte)) {
      if (bytes.find(byte) != bytes.end())
        return true;

      i += 3;
      continue;
    }

    ++i;
  }

  return false;
}

}  // namespace base