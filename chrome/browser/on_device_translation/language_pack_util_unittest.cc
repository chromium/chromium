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
}

TEST(LanguagePackUtilTest, ToSupportedLanguage) {
  EXPECT_EQ(ToSupportedLanguage("en"), SupportedLanguage::kEn);
  EXPECT_EQ(ToSupportedLanguage("es"), SupportedLanguage::kEs);
  EXPECT_EQ(ToSupportedLanguage("ja"), SupportedLanguage::kJa);

  // TODO(crbug.com/358030919): Currently we are checking case-sensitive
  // language codes. This may be changed in the future.
  EXPECT_EQ(ToSupportedLanguage("En"), std::nullopt);
  // Check that the empty string is not a valid language code.
  EXPECT_EQ(ToSupportedLanguage(""), std::nullopt);
}

TEST(LanguagePackUtilTest, GetLanguagePackComponentConfig) {
  // En to Es.
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Es).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Es).language2,
            SupportedLanguage::kEs);
  EXPECT_THAT(
      std::string(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Es)
                      .config_path_pref),
      std::string(kTranslateKitEnEsPath));

  // En to Ja.
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ja).language1,
            SupportedLanguage::kEn);
  EXPECT_EQ(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ja).language2,
            SupportedLanguage::kJa);
  EXPECT_THAT(
      std::string(GetLanguagePackComponentConfig(LanguagePackKey::kEn_Ja)
                      .config_path_pref),
      std::string(kTranslateKitEnJaPath));
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
}

TEST(LanguagePackUtilTest, GetPackageNameSuffix) {
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Es), "en-es");
  EXPECT_EQ(GetPackageNameSuffix(LanguagePackKey::kEn_Ja), "en-ja");
}

TEST(LanguagePackUtilTest, GetPackageInstallSubDirNamesForVerification) {
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Es),
      std::vector<std::string>({"en_es_dictionary", "en_es_nmt", "es_en_nmt"}));
  EXPECT_THAT(
      GetPackageInstallSubDirNamesForVerification(LanguagePackKey::kEn_Ja),
      std::vector<std::string>({"en_ja_dictionary", "en_ja_nmt", "ja_en_nmt"}));
}

}  // namespace
}  // namespace on_device_translation
