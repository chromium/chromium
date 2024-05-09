// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/shortcut_icon_generator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace shortcuts {

TEST(ShortcutIconGeneratorTest, GenerateIconLetterFromUrl) {
  // ASCII:
  EXPECT_EQ(U'E', GenerateIconLetterFromUrl(GURL("http://example.com")));
  // Cyrillic capital letter ZHE for something like https://zhuk.rf:
  EXPECT_EQ(0x0416U,
            GenerateIconLetterFromUrl(GURL("https://xn--f1ai0a.xn--p1ai/")));
  // Arabic:
  EXPECT_EQ(0x0645U,
            GenerateIconLetterFromUrl(GURL("http://xn--mgbh0fb.example/")));
  // UTF-16 surrogate code units.
  // "𨭎𨥫𨋍" (U+28B4E, U+2896B, U+282CD)
  // (nonsensical sequence of non-BMP Chinese characters, IDNA-encoded)
  EXPECT_EQ(0x28b4eU,
            GenerateIconLetterFromUrl(GURL("http://xn--8h8k10hnsb.example/")));
  // "🌏👍" (U+1F30F, U+1F44D)
  // (sequence of non-BMP emoji characters, IDNA-encoded)
  // Emoji are not allowed in IDNA domains, so the first character of this
  // domain is simply 'X'.
  EXPECT_EQ(U'X',
            GenerateIconLetterFromUrl(GURL("http://xn--vg8h2t.example/")));
}

TEST(ShortcutIconGeneratorTest, GenerateIconLetterFromName) {
  // ASCII Encoding
  EXPECT_EQ(U'T', GenerateIconLetterFromName(u"test app name"));
  EXPECT_EQ(U'T', GenerateIconLetterFromName(u"Test app name"));
  // UTF-16 encoding (single code units):
  // "имя" (U+0438, U+043C, U+044F)
  const char16_t russian_name[] = {0x0438, 0x043c, 0x044f, 0x0};
  EXPECT_EQ(0x0418U, GenerateIconLetterFromName(russian_name));
  // UTF-16 surrogate code units.
  // "𨭎𨥫𨋍" (U+28B4E, U+2896B, U+282CD)
  // (nonsensical sequence of non-BMP Chinese characters, UTF-16-encoded)
  const char16_t chinese_name[] = {0xd862, 0xdf4e, 0xd862, 0xdd6b,
                                   0xd860, 0xdecd, 0x0};
  EXPECT_EQ(0x28b4eU, GenerateIconLetterFromName(chinese_name));
  // "🌏👍" (U+1F30F, U+1F44D)
  // (sequence of non-BMP emoji characters, UTF-16-encoded)
  const char16_t emoji_name[] = {0xd83c, 0xdf0f, 0xd83d, 0xdc4d, 0x0};
  EXPECT_EQ(0x1f30fU, GenerateIconLetterFromName(emoji_name));
}

TEST(ShortcutIconGeneratorTest, IconLetterToString) {
  // ASCII character
  EXPECT_EQ(u"T", IconLetterToString('T'));
  // BMP character 'И' (U+0418)
  EXPECT_EQ(std::u16string({0x0418}), IconLetterToString(0x0418));
  // Non-BMP character '𨭎' (U+28B4E)
  EXPECT_EQ(std::u16string({0xd862, 0xdf4e}), IconLetterToString(0x28b4e));
  // Non-BMP character '🌏' (U+1F30F)
  EXPECT_EQ(std::u16string({0xd83c, 0xdf0f}), IconLetterToString(0x1f30f));
}

}  // namespace shortcuts
