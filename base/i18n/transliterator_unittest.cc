// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/transliterator.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/i18n/transliterator_buildflags.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::i18n {

TEST(TransliteratorTest, LowerCorrect) {
  std::unique_ptr<Transliterator> transliterator(CreateTransliterator("Lower"));
  ASSERT_TRUE(transliterator);
  std::u16string text(u"ÎÑŢÉRÑÅŢÎÖÑÅĻÎŽÅŢÎÖÑ");
  std::u16string result = transliterator->Transliterate(text);
  EXPECT_EQ(result, u"îñţérñåţîöñåļîžåţîöñ");
}

TEST(TransliteratorTest, LatinASCIICorrect) {
  std::unique_ptr<Transliterator> transliterator(
      CreateTransliterator("Latin-ASCII"));
  ASSERT_TRUE(transliterator);
  std::u16string text(u"ÎÑŢÉRÑÅŢÎÖÑÅĻÎŽÅŢÎÖÑ");
  std::u16string result = transliterator->Transliterate(text);
  EXPECT_EQ(result, u"INTERNATIONALIZATION");
}

TEST(TransliteratorTest, LowerLatinASCIICorrect) {
  std::unique_ptr<Transliterator> transliterator(
      CreateTransliterator("Lower;Latin-ASCII"));
  ASSERT_TRUE(transliterator);
  std::u16string text(u"ÎÑŢÉRÑÅŢÎÖÑÅĻÎŽÅŢÎÖÑ");
  std::u16string result = transliterator->Transliterate(text);
  EXPECT_EQ(result, u"internationalization");
}

// Used in components/autofill/core/browser/data_model/transliterator.cc
TEST(TransliteratorTest, KatakanaHiraganaCorrect) {
  for (std::string_view id : {"Katakana-Hiragana", "Kana-Hira"}) {
    std::unique_ptr<Transliterator> transliterator(CreateTransliterator(id));
    ASSERT_TRUE(transliterator) << id;
    std::u16string text(
        u"アメリカとウクライナの高官が来週、会談する見通しとなりました。");
    std::u16string result = transliterator->Transliterate(text);
    EXPECT_EQ(result,
              u"あめりかとうくらいなの高官が来週、会談する見通しとなりました。")
        << id;
  }
}

// Used in components/autofill/core/browser/data_model/transliterator.cc
TEST(TransliteratorTest, HiraganaKatakanaCorrect) {
  for (std::string_view id : {"Hiragana-Katakana", "Hira-Kana"}) {
    std::unique_ptr<Transliterator> transliterator(CreateTransliterator(id));
    ASSERT_TRUE(transliterator) << id;
    std::u16string text(
        u"アメリカとウクライナの高官が来週、会談する見通しとなりました。");
    std::u16string result = transliterator->Transliterate(text);
    EXPECT_EQ(result,
              u"アメリカトウクライナノ高官ガ来週、会談スル見通シトナリマシタ。")
        << id;
  }
}

// Used in components/autofill/core/browser/data_model/transliterator.cc
TEST(TransliteratorTest, NFCCorrect) {
  std::unique_ptr<Transliterator> transliterator(CreateTransliterator("NFC"));
  ASSERT_TRUE(transliterator);
  std::u16string text(u"Ää Öö Üü éàçñ Ẁẃ ff fi ffl Dj dz ṡ ᴯ");
  std::u16string result = transliterator->Transliterate(text);
  EXPECT_EQ(result, u"Ää Öö Üü éàçñ Ẁẃ ff fi ffl Dj dz ṡ ᴯ");
}

// Used in components/autofill/core/browser/data_model/transliterator.cc
TEST(TransliteratorTest, NFDCorrect) {
  std::unique_ptr<Transliterator> transliterator(CreateTransliterator("NFD"));
  ASSERT_TRUE(transliterator);
  std::u16string text(u"Ää Öö Üü éàçñ Ẁẃ ﬀ ﬁ ﬄ ǅǆ ẛ ᴯ");
  std::u16string result = transliterator->Transliterate(text);
  EXPECT_EQ(result, u"Ää Öö Üü éàçñ Ẁẃ ﬀ ﬁ ﬄ ǅǆ ẛ ᴯ");
}

TEST(TransliteratorTest, NonNullTerminatedStringViewSlices) {
  constexpr std::string_view kIdWithGarbage = "Katakana-Hiragana_TRAILING";
  std::unique_ptr<Transliterator> kana_translit =
      CreateTransliterator(kIdWithGarbage.substr(0, 17));
  ASSERT_TRUE(kana_translit);
  EXPECT_EQ(kana_translit->Transliterate(u"テスト"), u"てすと");

  constexpr std::string_view kCompoundWithGarbage =
      "Lower;Latin-ASCII;INVALID_ID";
  std::unique_ptr<Transliterator> compound_translit =
      CreateTransliterator(kCompoundWithGarbage.substr(0, 17));
  ASSERT_TRUE(compound_translit);
  EXPECT_EQ(compound_translit->Transliterate(u"ÜBER"), u"uber");

  constexpr std::string_view kCustomIdWithGarbage = "und-t-und-x0-afill_EXTRA";
  std::unique_ptr<Transliterator> custom_translit =
      CreateTransliterator(kCustomIdWithGarbage.substr(0, 18));
  ASSERT_TRUE(custom_translit);
  EXPECT_EQ(custom_translit->Transliterate(u"Straße"), u"strasse");
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

TEST(TransliteratorTest, UnpairedSurrogatesHandledSafely) {
  std::unique_ptr<Transliterator> transliterator(
      CreateTransliterator("Lower;Latin-ASCII"));
  ASSERT_TRUE(transliterator);
  const std::u16string unpaired = {u'Ä', 0xd800, u'Ö'};
  std::u16string result = transliterator->Transliterate(unpaired);
#if BUILDFLAG(BUILD_RUST_TRANSLIT)
  EXPECT_EQ(result, (std::u16string{u'a', 0xfffd, u'o'}));
#else
  EXPECT_EQ(result, (std::u16string{u'a', 0xd800, u'o'}));
#endif
}

TEST(TransliteratorTest, InvalidIdDeath) {
  EXPECT_DCHECK_DEATH(CreateTransliterator(""));
  EXPECT_DCHECK_DEATH(CreateTransliterator("Unknown-Translit"));
  EXPECT_DCHECK_DEATH(CreateTransliterator("und-t-und-x0-unknown"));
  EXPECT_DCHECK_DEATH(CreateTransliterator("und_t_und_x0_afill"));
  EXPECT_DCHECK_DEATH(CreateTransliterator("de-t-de-d0-ascii"));
#if BUILDFLAG(BUILD_RUST_TRANSLIT)
  EXPECT_DCHECK_DEATH(CreateTransliterator(";;;"));
#endif
}

}  // namespace base::i18n
