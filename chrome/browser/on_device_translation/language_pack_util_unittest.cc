// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/language_pack_util.h"

#include <string>

#include "base/logging.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_translation {
namespace {

TEST(LanguagePackUtilTest, ToLanguageCode) {
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kEn), "en");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kEs), "es");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kJa), "ja");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kAr), "ar");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kBn), "bn");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kDe), "de");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kFr), "fr");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kHi), "hi");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kIt), "it");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kKo), "ko");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kNl), "nl");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kPl), "pl");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kPt), "pt");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kRu), "ru");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kTh), "th");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kTr), "tr");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kVi), "vi");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kZh), "zh");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kZhHant), "zh-Hant");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kBg), "bg");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kCs), "cs");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kDa), "da");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kEl), "el");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kFi), "fi");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kHr), "hr");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kHu), "hu");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kId), "id");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kIw), "iw");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kLt), "lt");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kNo), "no");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kRo), "ro");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kSk), "sk");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kSl), "sl");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kSv), "sv");
  EXPECT_EQ(ToLanguageCode(SupportedLanguage::kUk), "uk");
}

TEST(LanguagePackUtilTest, ToSupportedLanguage) {
  EXPECT_EQ(ToSupportedLanguage("en"), SupportedLanguage::kEn);
  EXPECT_EQ(ToSupportedLanguage("es"), SupportedLanguage::kEs);
  EXPECT_EQ(ToSupportedLanguage("ja"), SupportedLanguage::kJa);
  EXPECT_EQ(ToSupportedLanguage("es"), SupportedLanguage::kEs);
  EXPECT_EQ(ToSupportedLanguage("ja"), SupportedLanguage::kJa);
  EXPECT_EQ(ToSupportedLanguage("ar"), SupportedLanguage::kAr);
  EXPECT_EQ(ToSupportedLanguage("bn"), SupportedLanguage::kBn);
  EXPECT_EQ(ToSupportedLanguage("de"), SupportedLanguage::kDe);
  EXPECT_EQ(ToSupportedLanguage("fr"), SupportedLanguage::kFr);
  EXPECT_EQ(ToSupportedLanguage("hi"), SupportedLanguage::kHi);
  EXPECT_EQ(ToSupportedLanguage("it"), SupportedLanguage::kIt);
  EXPECT_EQ(ToSupportedLanguage("ko"), SupportedLanguage::kKo);
  EXPECT_EQ(ToSupportedLanguage("nl"), SupportedLanguage::kNl);
  EXPECT_EQ(ToSupportedLanguage("pl"), SupportedLanguage::kPl);
  EXPECT_EQ(ToSupportedLanguage("pt"), SupportedLanguage::kPt);
  EXPECT_EQ(ToSupportedLanguage("ru"), SupportedLanguage::kRu);
  EXPECT_EQ(ToSupportedLanguage("th"), SupportedLanguage::kTh);
  EXPECT_EQ(ToSupportedLanguage("tr"), SupportedLanguage::kTr);
  EXPECT_EQ(ToSupportedLanguage("vi"), SupportedLanguage::kVi);
  EXPECT_EQ(ToSupportedLanguage("zh"), SupportedLanguage::kZh);
  EXPECT_EQ(ToSupportedLanguage("zh-Hant"), SupportedLanguage::kZhHant);
  EXPECT_EQ("bg", ToLanguageCode(SupportedLanguage::kBg));
  EXPECT_EQ("cs", ToLanguageCode(SupportedLanguage::kCs));
  EXPECT_EQ("da", ToLanguageCode(SupportedLanguage::kDa));
  EXPECT_EQ("el", ToLanguageCode(SupportedLanguage::kEl));
  EXPECT_EQ("fi", ToLanguageCode(SupportedLanguage::kFi));
  EXPECT_EQ("hr", ToLanguageCode(SupportedLanguage::kHr));
  EXPECT_EQ("hu", ToLanguageCode(SupportedLanguage::kHu));
  EXPECT_EQ("id", ToLanguageCode(SupportedLanguage::kId));
  EXPECT_EQ("iw", ToLanguageCode(SupportedLanguage::kIw));
  EXPECT_EQ("lt", ToLanguageCode(SupportedLanguage::kLt));
  EXPECT_EQ("no", ToLanguageCode(SupportedLanguage::kNo));
  EXPECT_EQ("ro", ToLanguageCode(SupportedLanguage::kRo));
  EXPECT_EQ("sk", ToLanguageCode(SupportedLanguage::kSk));
  EXPECT_EQ("sl", ToLanguageCode(SupportedLanguage::kSl));
  EXPECT_EQ("sv", ToLanguageCode(SupportedLanguage::kSv));
  EXPECT_EQ("uk", ToLanguageCode(SupportedLanguage::kUk));

  // TODO(crbug.com/358030919): Currently we are checking case-sensitive
  // language codes. This may be changed in the future.
  EXPECT_EQ(ToSupportedLanguage("En"), std::nullopt);
  // Check that the empty string is not a valid language code.
  EXPECT_EQ(ToSupportedLanguage(""), std::nullopt);
}

TEST(LanguagePackUtilTest, IsPopularLanguage) {
  EXPECT_TRUE(IsPopularLanguage(SupportedLanguage::kEn));
  EXPECT_TRUE(IsPopularLanguage(SupportedLanguage::kEn));
  EXPECT_TRUE(IsPopularLanguage(SupportedLanguage::kEs));
  EXPECT_TRUE(IsPopularLanguage(SupportedLanguage::kJa));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kAr));
  EXPECT_TRUE(IsPopularLanguage(SupportedLanguage::kBn));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kDe));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kFr));
  EXPECT_TRUE(IsPopularLanguage(SupportedLanguage::kHi));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kIt));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kKo));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kNl));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kPl));
  EXPECT_TRUE(IsPopularLanguage(SupportedLanguage::kPt));
  EXPECT_TRUE(IsPopularLanguage(SupportedLanguage::kRu));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kTh));
  EXPECT_TRUE(IsPopularLanguage(SupportedLanguage::kTr));
  EXPECT_TRUE(IsPopularLanguage(SupportedLanguage::kVi));
  EXPECT_TRUE(IsPopularLanguage(SupportedLanguage::kZh));
  EXPECT_TRUE(IsPopularLanguage(SupportedLanguage::kZhHant));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kBg));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kCs));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kDa));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kEl));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kFi));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kHr));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kHu));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kId));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kIw));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kLt));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kNo));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kRo));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kSk));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kSl));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kSv));
  EXPECT_FALSE(IsPopularLanguage(SupportedLanguage::kUk));
}

TEST(LanguagePackUtilTest, GetLanguagePackComponentConfig) {
  // En to Es.
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Es).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Es).language2,
            SupportedLanguage::kEs);

  // En to Ja.
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ja).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ja).language2,
            SupportedLanguage::kJa);

  // Ar to En.
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kAr_En).language1,
            SupportedLanguage::kAr);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kAr_En).language2,
            SupportedLanguage::kEn);

  // Bn to En.
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kBn_En).language1,
            SupportedLanguage::kBn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kBn_En).language2,
            SupportedLanguage::kEn);

  // De to En.
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kDe_En).language1,
            SupportedLanguage::kDe);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kDe_En).language2,
            SupportedLanguage::kEn);

  // En to Fr.
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Fr).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Fr).language2,
            SupportedLanguage::kFr);

  // En to Hi.
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Hi).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Hi).language2,
            SupportedLanguage::kHi);

  // En to It.
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_It).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_It).language2,
            SupportedLanguage::kIt);

  // En to Ko.
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ko).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ko).language2,
            SupportedLanguage::kKo);

  // En to Nl.
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Nl).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Nl).language2,
            SupportedLanguage::kNl);

  // En to Pl.
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Pl).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Pl).language2,
            SupportedLanguage::kPl);

  // En to Pt.
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Pt).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Pt).language2,
            SupportedLanguage::kPt);

  // En to Ru.
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ru).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ru).language2,
            SupportedLanguage::kRu);

  // En to Th.
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Th).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Th).language2,
            SupportedLanguage::kTh);

  // En to Tr.
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Tr).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Tr).language2,
            SupportedLanguage::kTr);

  // En to Vi.
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Vi).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Vi).language2,
            SupportedLanguage::kVi);

  // En to Zh.
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Zh).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Zh).language2,
            SupportedLanguage::kZh);

  // En to ZhHant.
  EXPECT_EQ(
      GetLanguagePackComponentConfig(LanguagePackKey::kEn_ZhHant).language1,
      SupportedLanguage::kEn);
  EXPECT_EQ(
      GetLanguagePackComponentConfig(LanguagePackKey::kEn_ZhHant).language2,
      SupportedLanguage::kZhHant);

  // En to Bg
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kBg_En).language1,
            SupportedLanguage::kBg);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kBg_En).language2,
            SupportedLanguage::kEn);

  // En to Cs
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kCs_En).language1,
            SupportedLanguage::kCs);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kCs_En).language2,
            SupportedLanguage::kEn);

  // En to Da
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kDa_En).language1,
            SupportedLanguage::kDa);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kDa_En).language2,
            SupportedLanguage::kEn);

  // En to El
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEl_En).language1,
            SupportedLanguage::kEl);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEl_En).language2,
            SupportedLanguage::kEn);

  // En to Fi
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Fi).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Fi).language2,
            SupportedLanguage::kFi);

  // En to Hr
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Hr).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Hr).language2,
            SupportedLanguage::kHr);

  // En to Hu
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Hu).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Hu).language2,
            SupportedLanguage::kHu);

  // En to Id
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Id).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Id).language2,
            SupportedLanguage::kId);

  // En to Iw
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Iw).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Iw).language2,
            SupportedLanguage::kIw);

  // En to Lt
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Lt).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Lt).language2,
            SupportedLanguage::kLt);

  // En to No
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_No).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_No).language2,
            SupportedLanguage::kNo);

  // En to Ro
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ro).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ro).language2,
            SupportedLanguage::kRo);

  // En to Sk
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Sk).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Sk).language2,
            SupportedLanguage::kSk);

  // En to Sl
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Sl).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Sl).language2,
            SupportedLanguage::kSl);

  // En to Sv
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Sv).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Sv).language2,
            SupportedLanguage::kSv);

  // En to Uk
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Uk).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Uk).language2,
            SupportedLanguage::kUk);
}

TEST(LanguagePackUtilTest, GetComponentPathPrefName) {
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Es)),
              "on_device_translation.translate_kit_packages.en_es_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ja)),
              "on_device_translation.translate_kit_packages.en_ja_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kAr_En)),
              "on_device_translation.translate_kit_packages.ar_en_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kBn_En)),
              "on_device_translation.translate_kit_packages.bn_en_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kDe_En)),
              "on_device_translation.translate_kit_packages.de_en_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Fr)),
              "on_device_translation.translate_kit_packages.en_fr_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Hi)),
              "on_device_translation.translate_kit_packages.en_hi_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_It)),
              "on_device_translation.translate_kit_packages.en_it_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ko)),
              "on_device_translation.translate_kit_packages.en_ko_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Nl)),
              "on_device_translation.translate_kit_packages.en_nl_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Pl)),
              "on_device_translation.translate_kit_packages.en_pl_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Pt)),
              "on_device_translation.translate_kit_packages.en_pt_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ru)),
              "on_device_translation.translate_kit_packages.en_ru_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Th)),
              "on_device_translation.translate_kit_packages.en_th_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Tr)),
              "on_device_translation.translate_kit_packages.en_tr_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Vi)),
              "on_device_translation.translate_kit_packages.en_vi_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Zh)),
              "on_device_translation.translate_kit_packages.en_zh_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_ZhHant)),
              "on_device_translation.translate_kit_packages.en_zh-Hant_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kBg_En)),
              "on_device_translation.translate_kit_packages.bg_en_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kCs_En)),
              "on_device_translation.translate_kit_packages.cs_en_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kDa_En)),
              "on_device_translation.translate_kit_packages.da_en_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEl_En)),
              "on_device_translation.translate_kit_packages.el_en_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Fi)),
              "on_device_translation.translate_kit_packages.en_fi_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Hr)),
              "on_device_translation.translate_kit_packages.en_hr_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Hu)),
              "on_device_translation.translate_kit_packages.en_hu_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Id)),
              "on_device_translation.translate_kit_packages.en_id_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Iw)),
              "on_device_translation.translate_kit_packages.en_iw_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Lt)),
              "on_device_translation.translate_kit_packages.en_lt_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_No)),
              "on_device_translation.translate_kit_packages.en_no_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ro)),
              "on_device_translation.translate_kit_packages.en_ro_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Sk)),
              "on_device_translation.translate_kit_packages.en_sk_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Sl)),
              "on_device_translation.translate_kit_packages.en_sl_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Sv)),
              "on_device_translation.translate_kit_packages.en_sv_path");
  EXPECT_THAT(GetComponentPathPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Uk)),
              "on_device_translation.translate_kit_packages.en_uk_path");
}

TEST(LanguagePackUtilTest, GetRegisteredFlagPrefName) {
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Es)),
              "on_device_translation.translate_kit_packages.en_es_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ja)),
              "on_device_translation.translate_kit_packages.en_ja_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kAr_En)),
              "on_device_translation.translate_kit_packages.ar_en_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kBn_En)),
              "on_device_translation.translate_kit_packages.bn_en_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kDe_En)),
              "on_device_translation.translate_kit_packages.de_en_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Fr)),
              "on_device_translation.translate_kit_packages.en_fr_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Hi)),
              "on_device_translation.translate_kit_packages.en_hi_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_It)),
              "on_device_translation.translate_kit_packages.en_it_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ko)),
              "on_device_translation.translate_kit_packages.en_ko_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Nl)),
              "on_device_translation.translate_kit_packages.en_nl_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Pl)),
              "on_device_translation.translate_kit_packages.en_pl_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Pt)),
              "on_device_translation.translate_kit_packages.en_pt_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ru)),
              "on_device_translation.translate_kit_packages.en_ru_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Th)),
              "on_device_translation.translate_kit_packages.en_th_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Tr)),
              "on_device_translation.translate_kit_packages.en_tr_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Vi)),
              "on_device_translation.translate_kit_packages.en_vi_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Zh)),
              "on_device_translation.translate_kit_packages.en_zh_registered");
  EXPECT_THAT(
      GetRegisteredFlagPrefName(
          GetLanguagePackComponentConfig(LanguagePackKey::kEn_ZhHant)),
      "on_device_translation.translate_kit_packages.en_zh-Hant_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kBg_En)),
              "on_device_translation.translate_kit_packages.bg_en_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kCs_En)),
              "on_device_translation.translate_kit_packages.cs_en_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kDa_En)),
              "on_device_translation.translate_kit_packages.da_en_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEl_En)),
              "on_device_translation.translate_kit_packages.el_en_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Fi)),
              "on_device_translation.translate_kit_packages.en_fi_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Hr)),
              "on_device_translation.translate_kit_packages.en_hr_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Hu)),
              "on_device_translation.translate_kit_packages.en_hu_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Id)),
              "on_device_translation.translate_kit_packages.en_id_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Iw)),
              "on_device_translation.translate_kit_packages.en_iw_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Lt)),
              "on_device_translation.translate_kit_packages.en_lt_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_No)),
              "on_device_translation.translate_kit_packages.en_no_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ro)),
              "on_device_translation.translate_kit_packages.en_ro_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Sk)),
              "on_device_translation.translate_kit_packages.en_sk_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Sl)),
              "on_device_translation.translate_kit_packages.en_sl_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Sv)),
              "on_device_translation.translate_kit_packages.en_sv_registered");
  EXPECT_THAT(GetRegisteredFlagPrefName(
                  GetLanguagePackComponentConfig(LanguagePackKey::kEn_Uk)),
              "on_device_translation.translate_kit_packages.en_uk_registered");
}

TEST(LanguagePackUtilTest, CalculateRequiredLanguagePacks) {
  // Check that invalid language codes are not supported.
  EXPECT_THAT(CalculateRequiredLanguagePacks("en", "invalid"),
              std::set<LanguagePackKey>());
  EXPECT_THAT(CalculateRequiredLanguagePacks("invalid", "en"),
              std::set<LanguagePackKey>());

  // Check that the same language is not supported.
  EXPECT_THAT(CalculateRequiredLanguagePacks("en", "en"),
              std::set<LanguagePackKey>());
  EXPECT_THAT(CalculateRequiredLanguagePacks("es", "es"),
              std::set<LanguagePackKey>());
  EXPECT_THAT(CalculateRequiredLanguagePacks("ja", "ja"),
              std::set<LanguagePackKey>());

  // One of the languages is English.
  EXPECT_THAT(CalculateRequiredLanguagePacks("en", "es"),
              std::set<LanguagePackKey>({LanguagePackKey::kEn_Es}));
  EXPECT_THAT(CalculateRequiredLanguagePacks("es", "en"),
              std::set<LanguagePackKey>({LanguagePackKey::kEn_Es}));
  EXPECT_THAT(CalculateRequiredLanguagePacks("en", "ja"),
              std::set<LanguagePackKey>({LanguagePackKey::kEn_Ja}));
  EXPECT_THAT(CalculateRequiredLanguagePacks("ja", "en"),
              std::set<LanguagePackKey>({LanguagePackKey::kEn_Ja}));

  // Both languages are non-English.
  EXPECT_THAT(CalculateRequiredLanguagePacks("es", "ja"),
              std::set<LanguagePackKey>(
                  {LanguagePackKey::kEn_Es, LanguagePackKey::kEn_Ja}));
  EXPECT_THAT(CalculateRequiredLanguagePacks("ja", "es"),
              std::set<LanguagePackKey>(
                  {LanguagePackKey::kEn_Es, LanguagePackKey::kEn_Ja}));
}

TEST(LanguagePackUtilTest, GetPackageInstallDirName) {
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Es), "en_es");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Ja), "en_ja");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kAr_En), "ar_en");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kBn_En), "bn_en");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kDe_En), "de_en");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Fr), "en_fr");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Hi), "en_hi");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_It), "en_it");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Ko), "en_ko");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Nl), "en_nl");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Pl), "en_pl");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Pt), "en_pt");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Ru), "en_ru");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Th), "en_th");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Tr), "en_tr");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Vi), "en_vi");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Zh), "en_zh");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_ZhHant),
            "en_zh-Hant");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kBg_En), "bg_en");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kCs_En), "cs_en");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kDa_En), "da_en");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEl_En), "el_en");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Fi), "en_fi");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Hr), "en_hr");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Hu), "en_hu");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Id), "en_id");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Iw), "en_iw");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Lt), "en_lt");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_No), "en_no");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Ro), "en_ro");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Sk), "en_sk");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Sl), "en_sl");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Sv), "en_sv");
  EXPECT_EQ(GetPackageInstallDirName(LanguagePackKey::kEn_Uk), "en_uk");
}

TEST(LanguagePackUtilTest, GetPackageNameSuffix) {
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Es), "en-es");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Ja), "en-ja");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kAr_En), "ar-en");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kBn_En), "bn-en");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kDe_En), "de-en");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Fr), "en-fr");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Hi), "en-hi");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_It), "en-it");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Ko), "en-ko");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Nl), "en-nl");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Pl), "en-pl");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Pt), "en-pt");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Ru), "en-ru");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Th), "en-th");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Tr), "en-tr");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Vi), "en-vi");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Zh), "en-zh");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_ZhHant), "en-zh-Hant");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kBg_En), "bg-en");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kCs_En), "cs-en");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kDa_En), "da-en");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEl_En), "el-en");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Fi), "en-fi");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Hr), "en-hr");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Hu), "en-hu");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Id), "en-id");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Iw), "en-iw");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Lt), "en-lt");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_No), "en-no");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Ro), "en-ro");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Sk), "en-sk");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Sl), "en-sl");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Sv), "en-sv");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Uk), "en-uk");
}

TEST(LanguagePackUtilTest, GetPackageInstallSubDirNamesForVerification) {
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Es),
      std::vector<std::string>({"en_es_dictionary", "en_es_nmt", "es_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Ja),
      std::vector<std::string>({"en_ja_dictionary", "en_ja_nmt", "ja_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kAr_En),
      std::vector<std::string>({"ar_en_dictionary", "ar_en_nmt", "en_ar_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kBn_En),
      std::vector<std::string>({"bn_en_dictionary", "bn_en_nmt", "en_bn_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kDe_En),
      std::vector<std::string>({"de_en_dictionary", "de_en_nmt", "en_de_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Fr),
      std::vector<std::string>({"en_fr_dictionary", "en_fr_nmt", "fr_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Hi),
      std::vector<std::string>({"en_hi_dictionary", "en_hi_nmt", "hi_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_It),
      std::vector<std::string>({"en_it_dictionary", "en_it_nmt", "it_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Ko),
      std::vector<std::string>({"en_ko_dictionary", "en_ko_nmt", "ko_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Nl),
      std::vector<std::string>({"en_nl_dictionary", "en_nl_nmt", "nl_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Pl),
      std::vector<std::string>({"en_pl_dictionary", "en_pl_nmt", "pl_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Pt),
      std::vector<std::string>({"en_pt_dictionary", "en_pt_nmt", "pt_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Ru),
      std::vector<std::string>({"en_ru_dictionary", "en_ru_nmt", "ru_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Th),
      std::vector<std::string>({"en_th_dictionary", "en_th_nmt", "th_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Tr),
      std::vector<std::string>({"en_tr_dictionary", "en_tr_nmt", "tr_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Vi),
      std::vector<std::string>({"en_vi_dictionary", "en_vi_nmt", "vi_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Zh),
      std::vector<std::string>({"en_zh_dictionary", "en_zh_nmt", "zh_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_ZhHant),
      std::vector<std::string>(
          {"en_zh-Hant_dictionary", "en_zh-Hant_nmt", "zh-Hant_en_nmt"}));

  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kBg_En),
      std::vector<std::string>({"bg_en_dictionary", "bg_en_nmt", "en_bg_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kCs_En),
      std::vector<std::string>({"cs_en_dictionary", "cs_en_nmt", "en_cs_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kDa_En),
      std::vector<std::string>({"da_en_dictionary", "da_en_nmt", "en_da_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEl_En),
      std::vector<std::string>({"el_en_dictionary", "el_en_nmt", "en_el_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Fi),
      std::vector<std::string>({"en_fi_dictionary", "en_fi_nmt", "fi_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Hr),
      std::vector<std::string>({"en_hr_dictionary", "en_hr_nmt", "hr_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Hu),
      std::vector<std::string>({"en_hu_dictionary", "en_hu_nmt", "hu_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Id),
      std::vector<std::string>({"en_id_dictionary", "en_id_nmt", "id_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Iw),
      std::vector<std::string>({"en_iw_dictionary", "en_iw_nmt", "iw_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Lt),
      std::vector<std::string>({"en_lt_dictionary", "en_lt_nmt", "lt_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_No),
      std::vector<std::string>({"en_no_dictionary", "en_no_nmt", "no_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Ro),
      std::vector<std::string>({"en_ro_dictionary", "en_ro_nmt", "ro_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Sk),
      std::vector<std::string>({"en_sk_dictionary", "en_sk_nmt", "sk_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Sl),
      std::vector<std::string>({"en_sl_dictionary", "en_sl_nmt", "sl_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Sv),
      std::vector<std::string>({"en_sv_dictionary", "en_sv_nmt", "sv_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Uk),
      std::vector<std::string>({"en_uk_dictionary", "en_uk_nmt", "uk_en_nmt"}));
}

TEST(LanguagePackUtilTest, GetSourceLanguageCode) {
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Es), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Ja), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kAr_En), "ar");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kBn_En), "bn");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kDe_En), "de");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Fr), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Hi), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_It), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Ko), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Nl), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Pl), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Pt), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Ru), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Th), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Tr), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Vi), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Zh), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_ZhHant), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kBg_En), "bg");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kCs_En), "cs");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kDa_En), "da");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEl_En), "el");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Fi), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Hr), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Hu), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Id), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Iw), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Lt), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_No), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Ro), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Sk), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Sl), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Sv), "en");
  EXPECT_EQ(GetSourceLanguageCode(LanguagePackKey::kEn_Uk), "en");
}

TEST(LanguagePackUtilTest, GetTargetLanguageCode) {
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Es), "es");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Ja), "ja");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kAr_En), "en");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kBn_En), "en");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kDe_En), "en");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Fr), "fr");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Hi), "hi");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_It), "it");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Ko), "ko");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Nl), "nl");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Pl), "pl");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Pt), "pt");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Ru), "ru");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Th), "th");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Tr), "tr");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Vi), "vi");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Zh), "zh");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_ZhHant), "zh-Hant");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kBg_En), "en");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kCs_En), "en");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kDa_En), "en");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEl_En), "en");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Fi), "fi");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Hr), "hr");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Hu), "hu");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Id), "id");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Iw), "iw");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Lt), "lt");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_No), "no");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Ro), "ro");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Sk), "sk");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Sl), "sl");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Sv), "sv");
  EXPECT_EQ(GetTargetLanguageCode(LanguagePackKey::kEn_Uk), "uk");
}

}  // namespace
}  // namespace on_device_translation
