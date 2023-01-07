// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "base/i18n/unicodestring.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/ustring.h"
#include "third_party/icu/source/i18n/unicode/translit.h"

namespace base {
namespace i18n {

TEST(TransliteratorTest, LowerCorrect) {
  UParseError parseErr;
  UErrorCode err = U_ZERO_ERROR;
  std::unique_ptr<icu::Transliterator> transliterator(
      icu::Transliterator::createInstance("Lower", UTRANS_FORWARD,
                                          parseErr, err));
  ASSERT_TRUE(U_SUCCESS(err));
  icu::UnicodeString text(u"ÎÑŢÉRÑÅŢÎÖÑÅĻÎŽÅŢÎÖÑ");
  transliterator->transliterate(text);
  EXPECT_EQ(base::i18n::UnicodeStringToString16(text), u"îñţérñåţîöñåļîžåţîöñ");
}

TEST(TransliteratorTest, LatinASCIICorrect) {
  UParseError parseErr;
  UErrorCode err = U_ZERO_ERROR;
  std::unique_ptr<icu::Transliterator> transliterator(
      icu::Transliterator::createInstance("Latin-ASCII", UTRANS_FORWARD,
                                          parseErr, err));
  ASSERT_TRUE(U_SUCCESS(err));
  icu::UnicodeString text(u"ÎÑŢÉRÑÅŢÎÖÑÅĻÎŽÅŢÎÖÑ");
  transliterator->transliterate(text);
  EXPECT_EQ(base::i18n::UnicodeStringToString16(text), u"INTERNATIONALIZATION");
}

TEST(TransliteratorTest, LowerLatinASCIICorrect) {
  UParseError parseErr;
  UErrorCode err = U_ZERO_ERROR;
  std::unique_ptr<icu::Transliterator> transliterator(
      icu::Transliterator::createInstance("Lower;Latin-ASCII", UTRANS_FORWARD,
                                          parseErr, err));
  ASSERT_TRUE(U_SUCCESS(err));
  icu::UnicodeString text(u"ÎÑŢÉRÑÅŢÎÖÑÅĻÎŽÅŢÎÖÑ");
  transliterator->transliterate(text);
  EXPECT_EQ(base::i18n::UnicodeStringToString16(text), u"internationalization");
}

}  // namespace i18n
}  // namespace base
