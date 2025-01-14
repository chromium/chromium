// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/transliterator.h"

#include "base/i18n/unicodestring.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::i18n {

TEST(TransliteratorTest, LowerCorrect) {
  std::unique_ptr<base::i18n::Transliterator> transliterator(
      base::i18n::CreateTransliterator("Lower"));
  ASSERT_TRUE(transliterator != nullptr);
  std::u16string text(u"ÎÑŢÉRÑÅŢÎÖÑÅĻÎŽÅŢÎÖÑ");
  std::u16string result = transliterator->Transliterate(text);
  EXPECT_EQ(result, u"îñţérñåţîöñåļîžåţîöñ");
}

TEST(TransliteratorTest, LatinASCIICorrect) {
  std::unique_ptr<base::i18n::Transliterator> transliterator(
      base::i18n::CreateTransliterator("Latin-ASCII"));
  ASSERT_TRUE(transliterator != nullptr);
  std::u16string text(u"ÎÑŢÉRÑÅŢÎÖÑÅĻÎŽÅŢÎÖÑ");
  std::u16string result = transliterator->Transliterate(text);
  EXPECT_EQ(result, u"INTERNATIONALIZATION");
}

TEST(TransliteratorTest, LowerLatinASCIICorrect) {
  std::unique_ptr<base::i18n::Transliterator> transliterator(
      base::i18n::CreateTransliterator("Lower;Latin-ASCII"));
  ASSERT_TRUE(transliterator != nullptr);
  std::u16string text(u"ÎÑŢÉRÑÅŢÎÖÑÅĻÎŽÅŢÎÖÑ");
  std::u16string result = transliterator->Transliterate(text);
  EXPECT_EQ(result, u"internationalization");
}

}  // namespace base::i18n
