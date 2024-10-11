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
}

}  // namespace
}  // namespace on_device_translation
