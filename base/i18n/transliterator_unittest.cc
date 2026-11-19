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
  ASSERT_TRUE(transliterator);
  std::u16string text(u"ÎÑŢÉRÑÅŢÎÖÑÅĻÎŽÅŢÎÖÑ");
  std::u16string result = transliterator->Transliterate(text);
  EXPECT_EQ(result, u"îñţérñåţîöñåļîžåţîöñ");
}

TEST(TransliteratorTest, LatinASCIICorrect) {
  std::unique_ptr<base::i18n::Transliterator> transliterator(
      base::i18n::CreateTransliterator("Latin-ASCII"));
  ASSERT_TRUE(transliterator);
  std::u16string text(u"ÎÑŢÉRÑÅŢÎÖÑÅĻÎŽÅŢÎÖÑ");
  std::u16string result = transliterator->Transliterate(text);
  EXPECT_EQ(result, u"INTERNATIONALIZATION");
}

TEST(TransliteratorTest, LowerLatinASCIICorrect) {
  std::unique_ptr<base::i18n::Transliterator> transliterator(
      base::i18n::CreateTransliterator("Lower;Latin-ASCII"));
  ASSERT_TRUE(transliterator);
  std::u16string text(u"ÎÑŢÉRÑÅŢÎÖÑÅĻÎŽÅŢÎÖÑ");
  std::u16string result = transliterator->Transliterate(text);
  EXPECT_EQ(result, u"internationalization");
}

// Used in components/autofill/core/browser/data_model/transliterator.cc
TEST(TransliteratorTest, KatakanaHiraganaCorrect) {
  std::unique_ptr<base::i18n::Transliterator> transliterator(
      base::i18n::CreateTransliterator("Katakana-Hiragana"));
  ASSERT_TRUE(transliterator);
  std::u16string text(
      u"アメリカとウクライナの高官が来週、会談する見通しとなりました。");
  std::u16string result = transliterator->Transliterate(text);
  EXPECT_EQ(result,
            u"あめりかとうくらいなの高官が来週、会談する見通しとなりました。");
}

// Used in components/autofill/core/browser/data_model/transliterator.cc
TEST(TransliteratorTest, HiraganaKatakanaCorrect) {
  std::unique_ptr<base::i18n::Transliterator> transliterator(
      base::i18n::CreateTransliterator("Hiragana-Katakana"));
  ASSERT_TRUE(transliterator);
  std::u16string text(
      u"アメリカとウクライナの高官が来週、会談する見通しとなりました。");
  std::u16string result = transliterator->Transliterate(text);
  EXPECT_EQ(result,
            u"アメリカトウクライナノ高官ガ来週、会談スル見通シトナリマシタ。");
}

// Used in components/autofill/core/browser/data_model/transliterator.cc
TEST(TransliteratorTest, NFCCorrect) {
  std::unique_ptr<base::i18n::Transliterator> transliterator(
      base::i18n::CreateTransliterator("NFC"));
  ASSERT_TRUE(transliterator);
  std::u16string text(u"Ää Öö Üü éàçñ Ẁẃ ff fi ffl Dj dz ṡ ᴯ");
  std::u16string result = transliterator->Transliterate(text);
  EXPECT_EQ(result, u"Ää Öö Üü éàçñ Ẁẃ ff fi ffl Dj dz ṡ ᴯ");
}

// Used in components/autofill/core/browser/data_model/transliterator.cc
TEST(TransliteratorTest, NFDCorrect) {
  std::unique_ptr<base::i18n::Transliterator> transliterator(
      base::i18n::CreateTransliterator("NFD"));
  ASSERT_TRUE(transliterator);
  std::u16string text(u"Ää Öö Üü éàçñ Ẁẃ ﬀ ﬁ ﬄ ǅǆ ẛ ᴯ");
  std::u16string result = transliterator->Transliterate(text);
  EXPECT_EQ(result, u"Ää Öö Üü éàçñ Ẁẃ ﬀ ﬁ ﬄ ǅǆ ẛ ᴯ");
}

TEST(TransliteratorTest, CustomBcp47RuleSets) {
  std::unique_ptr<Transliterator> autofill_default =
      CreateTransliterator("und-t-und-x0-afill");
  ASSERT_TRUE(autofill_default);
  EXPECT_EQ(autofill_default->Transliterate(u"Ä_Ö_Ü_ß_Ł"), u"a_o_u_ss_l");

  std::unique_ptr<Transliterator> autofill_de =
      CreateTransliterator("und-t-und-x0-afillde");
  ASSERT_TRUE(autofill_de);
  EXPECT_EQ(autofill_de->Transliterate(u"ä_o\u0308_Ü_ß"), u"ae_oe_ue_ss");

  std::unique_ptr<Transliterator> drop_acc =
      CreateTransliterator("und-t-und-x0-dropacc");
  ASSERT_TRUE(drop_acc);
  EXPECT_EQ(drop_acc->Transliterate(u"łódź øresund đ"), u"lodz oresund d");
  EXPECT_EQ(drop_acc->Transliterate(u"các dấu đã được"), u"cac dau da duoc");

  std::unique_ptr<Transliterator> extra_conf =
      CreateTransliterator("und-t-und-x0-xconf");
  ASSERT_TRUE(extra_conf);
  EXPECT_EQ(extra_conf->Transliterate(u"ӕ七ßџθ—"), u"aetbu0-");

  std::unique_ptr<Transliterator> rm_hyphen =
      CreateTransliterator("und-t-und-x0-rmhyph");
  ASSERT_TRUE(rm_hyphen);
  EXPECT_EQ(rm_hyphen->Transliterate(u"wi\u2015fi--\u058A~"), u"wifi");

  std::unique_ptr<Transliterator> diacritic_stripper =
      CreateTransliterator("und-t-und-x0-diacstr");
  ASSERT_TRUE(diacritic_stripper);
  EXPECT_EQ(diacritic_stripper->Transliterate(u"Français École cœur København"),
            u"Francais Ecole coeur Kobenhavn");
}

}  // namespace base::i18n
