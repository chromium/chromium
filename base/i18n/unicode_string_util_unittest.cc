// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/unicode_string_util.h"

#include <stdint.h>

#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::i18n {

TEST(UnicodeStringUtilTest, ContainsUnicodeControlOrFormat_UTF8) {
  // Empty and normal UTF-8 strings (including multibyte UTF-8 whose
  // continuation bytes fall in 0x80..0x9F, such as U+20AC '€' = E2 82 AC).
  EXPECT_FALSE(ContainsUnicodeControlOrFormat(""));
  EXPECT_FALSE(ContainsUnicodeControlOrFormat("hello 1"));
  EXPECT_FALSE(ContainsUnicodeControlOrFormat("\u3042"));        // Hiragana A
  EXPECT_FALSE(ContainsUnicodeControlOrFormat("\u20AC"));        // Euro sign
  EXPECT_FALSE(ContainsUnicodeControlOrFormat("a\U0002070Eb"));  // Han char
  EXPECT_FALSE(ContainsUnicodeControlOrFormat("\U000E0100"));    // Mn, not Cf

  // Control characters (Cc)
  EXPECT_TRUE(ContainsUnicodeControlOrFormat(
      base::MakeStringViewWithNulChars("hello\0world")));
  EXPECT_TRUE(ContainsUnicodeControlOrFormat("a\x{1F}b"));  // US
  EXPECT_TRUE(ContainsUnicodeControlOrFormat("a\x{7F}b"));  // DEL
  EXPECT_TRUE(ContainsUnicodeControlOrFormat("a\u0085b"));  // NEL (C2 85)
  EXPECT_TRUE(ContainsUnicodeControlOrFormat("a\u009Fb"));  // APC (C2 9F)

  // Format characters (Cf) - BMP and Supplementary
  EXPECT_TRUE(ContainsUnicodeControlOrFormat("a\u00ADb"));      // Soft Hyphen
  EXPECT_TRUE(ContainsUnicodeControlOrFormat("a\u200Bb"));      // ZWSP
  EXPECT_TRUE(ContainsUnicodeControlOrFormat("a\u200Db"));      // ZWJ
  EXPECT_TRUE(ContainsUnicodeControlOrFormat("a\u200Eb"));      // LRM
  EXPECT_TRUE(ContainsUnicodeControlOrFormat("a\u202Db"));      // LRO
  EXPECT_TRUE(ContainsUnicodeControlOrFormat("a\uFEFFb"));      // BOM
  EXPECT_TRUE(ContainsUnicodeControlOrFormat("a\U000E0001b"));  // Language Tag
  EXPECT_TRUE(ContainsUnicodeControlOrFormat("a\U000E0020b"));  // Tag Space
  EXPECT_TRUE(ContainsUnicodeControlOrFormat("a\U000E007Fb"));  // Cancel Tag

  // Ill-formed UTF-8 byte sequences (including raw Latin-1 0x85 / 0xAD bytes)
  // are ignored rather than misinterpreted as code points.
  EXPECT_FALSE(ContainsUnicodeControlOrFormat("a\x{85}b"));
  EXPECT_FALSE(ContainsUnicodeControlOrFormat("a\x{AD}b"));
  EXPECT_FALSE(ContainsUnicodeControlOrFormat("a\x{FF}b"));
  EXPECT_FALSE(ContainsUnicodeControlOrFormat("a\x{ED}\x{A0}\x{80}b"));
}

TEST(UnicodeStringUtilTest, ContainsUnicodeControlOrFormat_UTF16) {
  // Empty and normal UTF-16 strings
  EXPECT_FALSE(ContainsUnicodeControlOrFormat(u""));
  EXPECT_FALSE(ContainsUnicodeControlOrFormat(u"hello 1 \u3042"));
  EXPECT_FALSE(ContainsUnicodeControlOrFormat(u"a\U0002070Eb"));
  EXPECT_FALSE(ContainsUnicodeControlOrFormat(u"\U000E0100"));  // Mn, not Cf

  // Unpaired surrogates are Cs, not Cc or Cf.
  const std::u16string unpaired_lead = {u'a', 0xD800, u'b'};
  const std::u16string unpaired_trail = {u'a', 0xDC00, u'b'};
  EXPECT_FALSE(ContainsUnicodeControlOrFormat(unpaired_lead));
  EXPECT_FALSE(ContainsUnicodeControlOrFormat(unpaired_trail));

  // Control characters (Cc)
  EXPECT_TRUE(ContainsUnicodeControlOrFormat(
      base::MakeStringViewWithNulChars(u"hello\0world")));
  EXPECT_TRUE(ContainsUnicodeControlOrFormat(u"a\u001Fb"));
  EXPECT_TRUE(ContainsUnicodeControlOrFormat(u"a\u007Fb"));
  EXPECT_TRUE(ContainsUnicodeControlOrFormat(u"a\u0085b"));
  EXPECT_TRUE(ContainsUnicodeControlOrFormat(u"a\u009Fb"));

  // Format characters (Cf) - BMP and Supplementary
  EXPECT_TRUE(ContainsUnicodeControlOrFormat(u"a\u00ADb"));
  EXPECT_TRUE(ContainsUnicodeControlOrFormat(u"a\u200Bb"));
  EXPECT_TRUE(ContainsUnicodeControlOrFormat(u"a\u200Db"));
  EXPECT_TRUE(ContainsUnicodeControlOrFormat(u"hello\u200Eworld"));
  EXPECT_TRUE(ContainsUnicodeControlOrFormat(u"a\u202Db"));
  EXPECT_TRUE(ContainsUnicodeControlOrFormat(u"a\uFEFFb"));
  EXPECT_TRUE(ContainsUnicodeControlOrFormat(u"a\U000E0001b"));
  EXPECT_TRUE(ContainsUnicodeControlOrFormat(u"a\U000E0020b"));
  EXPECT_TRUE(ContainsUnicodeControlOrFormat(u"a\U000E007Fb"));
}

TEST(UnicodeStringUtilTest, ContainsUnicodeControlOrFormatLatin1) {
  EXPECT_FALSE(ContainsUnicodeControlOrFormatLatin1({}));

  const uint8_t kNormalLatin1[] = {'h', 'i', '1', 0xA0, 0xA9, 0xFF};
  EXPECT_FALSE(ContainsUnicodeControlOrFormatLatin1(kNormalLatin1));

  // Control characters (Cc)
  const uint8_t kWithNul[] = {'a', 0x00, 'b'};  // NUL
  const uint8_t kWithUs[] = {'a', 0x1F, 'b'};   // US
  const uint8_t kWithDel[] = {'a', 0x7F, 'b'};  // DEL
  const uint8_t kWithNel[] = {'a', 0x85, 'b'};  // NEL
  const uint8_t kWithApc[] = {'a', 0x9F, 'b'};  // APC
  EXPECT_TRUE(ContainsUnicodeControlOrFormatLatin1(kWithNul));
  EXPECT_TRUE(ContainsUnicodeControlOrFormatLatin1(kWithUs));
  EXPECT_TRUE(ContainsUnicodeControlOrFormatLatin1(kWithDel));
  EXPECT_TRUE(ContainsUnicodeControlOrFormatLatin1(kWithNel));
  EXPECT_TRUE(ContainsUnicodeControlOrFormatLatin1(kWithApc));

  // Format characters (Cf)
  const uint8_t kWithSoftHyphen[] = {'a', 0xAD, 'b'};  // Soft Hyphen
  EXPECT_TRUE(ContainsUnicodeControlOrFormatLatin1(kWithSoftHyphen));
}

TEST(UnicodeStringUtilTest, RemoveUnicodeControlAndFormat_U16) {
  // Empty string
  std::u16string str;
  EXPECT_FALSE(RemoveUnicodeControlAndFormat(str));
  EXPECT_TRUE(str.empty());

  // No change
  str = u"hello";
  EXPECT_FALSE(RemoveUnicodeControlAndFormat(str));
  EXPECT_EQ(str, u"hello");

  // Only control
  str = base::MakeStringViewWithNulChars(u"\0\x1F\x7F");
  EXPECT_TRUE(RemoveUnicodeControlAndFormat(str));
  EXPECT_TRUE(str.empty());

  // Only format
  str = u"\u00AD\u200B\u200D\u200E";
  EXPECT_TRUE(RemoveUnicodeControlAndFormat(str));
  EXPECT_TRUE(str.empty());

  // Mixed (use MakeStringViewWithNulChars to preserve NUL)
  str = base::MakeStringViewWithNulChars(u"a\0b\u200Ec");
  EXPECT_TRUE(RemoveUnicodeControlAndFormat(str));
  EXPECT_EQ(str, u"abc");

  // Supplementary format (U+E0001 Language Tag: H 0xDB40, L 0xDC01)
  str = u"a\U000E0001b";
  ASSERT_EQ(str.length(), 4u);
  EXPECT_TRUE(RemoveUnicodeControlAndFormat(str));
  EXPECT_EQ(str, u"ab");

  // Supplementary normal character (U+2070E Han character) is preserved.
  str = u"a\U0002070Eb";
  std::u16string expected = str;
  EXPECT_FALSE(RemoveUnicodeControlAndFormat(str));
  EXPECT_EQ(str, expected);

  // Unpaired surrogate without any control/format characters: early return
  // leaves the string untouched.
  str = {u'a', 0xD841, u'b'};
  expected = str;
  EXPECT_FALSE(RemoveUnicodeControlAndFormat(str));
  EXPECT_EQ(str, expected);

  // Case 1: Split surrogates that would coalesce into a supplementary Format
  // character (U+E0001: 0xDB40, 0xDC01) when an intervening control/format
  // character is removed are each replaced with U+FFFD.
  str = {u'a', 0xDB40, 0x200B, 0xDC01, u'b'};
  EXPECT_TRUE(RemoveUnicodeControlAndFormat(str));
  EXPECT_EQ(str, u"a\uFFFD\uFFFDb");
  EXPECT_FALSE(ContainsUnicodeControlOrFormat(str));

  // Case 2: Single unpaired lead or trail surrogate alongside a removed
  // control/format character is replaced with U+FFFD.
  str = {u'a', 0xD841, 0x200B, u'b'};
  EXPECT_TRUE(RemoveUnicodeControlAndFormat(str));
  EXPECT_EQ(str, u"a\uFFFDb");

  str = {u'a', 0x200B, 0xDF0E, u'b'};
  EXPECT_TRUE(RemoveUnicodeControlAndFormat(str));
  EXPECT_EQ(str, u"a\uFFFDb");

  // Case 3: Split surrogates that would coalesce into a normal supplementary
  // character (U+2070E: 0xD841, 0xDF0E) when an intervening control/format
  // character is removed are replaced with U+FFFD rather than forming U+2070E.
  str = {u'a', 0xD841, 0x200B, 0xDF0E, u'b'};
  EXPECT_TRUE(RemoveUnicodeControlAndFormat(str));
  EXPECT_EQ(str, u"a\uFFFD\uFFFDb");
}

}  // namespace base::i18n
