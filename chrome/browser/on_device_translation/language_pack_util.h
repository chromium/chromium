// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_LANGUAGE_PACK_UTIL_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_LANGUAGE_PACK_UTIL_H_

#include <array>
#include <set>
#include <string_view>

#include "base/containers/fixed_flat_map.h"

namespace on_device_translation {

// The supported languages for on-device translation.
enum class SupportedLanguage {
  kEn = 0,
  kEs = 1,
  kJa = 2,
  kAr = 3,
  kBn = 4,
  kDe = 5,
  kFr = 6,
  kHi = 7,
  kIt = 8,
  kKo = 9,
  kNl = 10,
  kPl = 11,
  kPt = 12,
  kRu = 13,
  kTh = 14,
  kTr = 15,
  kVi = 16,
  kZh = 17,
  kZhHant = 18,
  kMaxValue = kZhHant,
};

// Converts a SupportedLanguage to a language code.
std::string_view ToLanguageCode(SupportedLanguage supported_language);

// Converts a language code to a SupportedLanguage.
std::optional<SupportedLanguage> ToSupportedLanguage(
    std::string_view language_code);

// Returns whether the language is in the top 12 by number of native speakers.
// https://en.wikipedia.org/wiki/List_of_languages_by_number_of_native_speakers#Top_languages_by_population
bool IsPopularLanguage(SupportedLanguage supported_language);

// The key for language pack components.
enum class LanguagePackKey {
  kEn_Es = 0,
  kEn_Ja = 1,
  kAr_En = 2,
  kBn_En = 3,
  kDe_En = 4,
  kEn_Fr = 5,
  kEn_Hi = 6,
  kEn_It = 7,
  kEn_Ko = 8,
  kEn_Nl = 9,
  kEn_Pl = 10,
  kEn_Pt = 11,
  kEn_Ru = 12,
  kEn_Th = 13,
  kEn_Tr = 14,
  kEn_Vi = 15,
  kEn_Zh = 16,
  kEn_ZhHant = 17,
  kMaxValue = kEn_ZhHant,
};

// The config for a language pack component.
struct LanguagePackComponentConfig {
  const SupportedLanguage language1;
  const SupportedLanguage language2;
  const uint8_t public_key_sha[32];
};

// Returns the pref name of the fully-qualified path to the installed language
// pack. This pref is populated only when the language pack component is fully
// initialized and ready for use.
//   Eg: "on_device_translation.translate_kit_packages.en_es_path"
std::string GetComponentPathPrefName(const LanguagePackComponentConfig& config);

// Returns the pref name of the boolean value which indicates whether the
// language pack component has been registered.
// This pref is set regardless of whether the component is currently ready for
// use. For example, the component might be tried to install but not yet
// initialized.
//   Eg: "on_device_translation.translate_kit_packages.en_es_registered"
std::string GetRegisteredFlagPrefName(
    const LanguagePackComponentConfig& config);

// The config for the TranslateKit en-es language pack.
const LanguagePackComponentConfig kTranslateKitEnEsConfig = {
    .language1 = SupportedLanguage::kEn,
    .language2 = SupportedLanguage::kEs,
    .public_key_sha = {0x63, 0xbd, 0x10, 0x98, 0x4e, 0xaa, 0xc3, 0xbe,
                       0x3b, 0xe0, 0x87, 0xba, 0x03, 0x5d, 0x7d, 0x6e,
                       0x44, 0x7e, 0xaa, 0x02, 0xbb, 0x0c, 0xcc, 0x51,
                       0xb5, 0x74, 0x5d, 0xb8, 0x3c, 0x04, 0xe1, 0xbb},
};

// The config for the TranslateKit en-ja language pack.
const LanguagePackComponentConfig kTranslateKitEnJaConfig = {
    .language1 = SupportedLanguage::kEn,
    .language2 = SupportedLanguage::kJa,
    .public_key_sha = {0x7d, 0x22, 0x33, 0x74, 0x1c, 0xa8, 0x62, 0x58,
                       0x77, 0xdc, 0x88, 0x87, 0x2d, 0x0e, 0x6e, 0x4b,
                       0xad, 0xbf, 0x37, 0x29, 0x06, 0xff, 0xc7, 0x7b,
                       0xe4, 0x28, 0x83, 0x2f, 0xee, 0x7d, 0xd3, 0x72},
};

// The config for the TranslateKit ar-en language pack.
const LanguagePackComponentConfig kTranslateKitArEnConfig = {
    .language1 = SupportedLanguage::kAr,
    .language2 = SupportedLanguage::kEn,
    .public_key_sha = {0xa1, 0xb3, 0x67, 0xf5, 0x5a, 0x8a, 0xfc, 0x1b,
                       0xa3, 0x13, 0x8c, 0xab, 0xcc, 0x04, 0x0d, 0xd8,
                       0x57, 0xc8, 0xbd, 0xc6, 0x1d, 0xfe, 0xa3, 0xfa,
                       0x6c, 0x5b, 0x90, 0x57, 0x79, 0x03, 0x74, 0x93}};

// The config for the TranslateKit bn-en language pack.
const LanguagePackComponentConfig kTranslateKitBnEnConfig = {
    .language1 = SupportedLanguage::kBn,
    .language2 = SupportedLanguage::kEn,
    .public_key_sha = {0x62, 0x6f, 0x69, 0x31, 0x9c, 0x78, 0xf3, 0x92,
                       0x30, 0xcd, 0x8c, 0x6c, 0xe8, 0xff, 0xd7, 0x60,
                       0xb9, 0x35, 0xcf, 0x69, 0xb9, 0xc6, 0xae, 0x39,
                       0x77, 0xa8, 0x59, 0xab, 0x34, 0xe9, 0xad, 0xaf}};

// The config for the TranslateKit de-en language pack.
const LanguagePackComponentConfig kTranslateKitDeEnConfig = {
    .language1 = SupportedLanguage::kDe,
    .language2 = SupportedLanguage::kEn,
    .public_key_sha = {0xd7, 0x55, 0x73, 0xae, 0x4f, 0x45, 0x74, 0xab,
                       0xed, 0x8d, 0x31, 0xb9, 0x2a, 0xd5, 0xb5, 0x89,
                       0x16, 0x5d, 0x03, 0x0f, 0x18, 0x2c, 0x83, 0xea,
                       0x21, 0xbd, 0x81, 0x71, 0x91, 0x80, 0xdb, 0x13}};

// The config for the TranslateKit en-fr language pack.
const LanguagePackComponentConfig kTranslateKitEnFrConfig = {
    .language1 = SupportedLanguage::kEn,
    .language2 = SupportedLanguage::kFr,
    .public_key_sha = {0xe5, 0xa2, 0x55, 0xf1, 0x20, 0x0a, 0xb3, 0x0f,
                       0xac, 0x84, 0x2f, 0xc4, 0x0e, 0x3b, 0xbb, 0x09,
                       0x11, 0x88, 0xed, 0xd0, 0x47, 0x06, 0x80, 0x53,
                       0x73, 0x1d, 0x96, 0xe4, 0x99, 0x14, 0x14, 0x45}};

// The config for the TranslateKit en-hi language pack.
const LanguagePackComponentConfig kTranslateKitEnHiConfig = {
    .language1 = SupportedLanguage::kEn,
    .language2 = SupportedLanguage::kHi,
    .public_key_sha = {0xa9, 0x81, 0xd8, 0xbc, 0x0d, 0x1d, 0xdf, 0x28,
                       0xa4, 0x79, 0x36, 0x36, 0x9b, 0xf5, 0x4e, 0x1a,
                       0xcd, 0xa3, 0x4e, 0x8b, 0x0a, 0xac, 0xfe, 0x27,
                       0x90, 0xb5, 0x4a, 0xc7, 0x2f, 0xc9, 0xc2, 0xab}};

// The config for the TranslateKit en-it language pack.
const LanguagePackComponentConfig kTranslateKitEnItConfig = {
    .language1 = SupportedLanguage::kEn,
    .language2 = SupportedLanguage::kIt,
    .public_key_sha = {0xd7, 0xc3, 0x40, 0x59, 0x34, 0x55, 0x61, 0x1e,
                       0x89, 0x9e, 0x39, 0x37, 0x04, 0xb6, 0x49, 0x75,
                       0xce, 0x86, 0xeb, 0xd3, 0xff, 0xd8, 0xd8, 0xf9,
                       0x9e, 0x37, 0x55, 0xe8, 0x0e, 0xce, 0x42, 0x3e}};

// The config for the TranslateKit en-ko language pack.
const LanguagePackComponentConfig kTranslateKitEnKoConfig = {
    .language1 = SupportedLanguage::kEn,
    .language2 = SupportedLanguage::kKo,
    .public_key_sha = {0xe2, 0x0c, 0x8f, 0x9e, 0x6d, 0x33, 0x06, 0x3c,
                       0x6d, 0xdc, 0x1c, 0x9f, 0x8e, 0x57, 0x5b, 0xc6,
                       0x27, 0x90, 0xa0, 0x48, 0xea, 0x8e, 0x79, 0x93,
                       0xbd, 0xf4, 0x9b, 0x94, 0x02, 0x35, 0xcc, 0x7a}};

// The config for the TranslateKit en-nl language pack.
const LanguagePackComponentConfig kTranslateKitEnNlConfig = {
    .language1 = SupportedLanguage::kEn,
    .language2 = SupportedLanguage::kNl,
    .public_key_sha = {0x7d, 0xb7, 0x14, 0x60, 0x47, 0xda, 0xd9, 0xaa,
                       0x5d, 0xeb, 0xbf, 0x6d, 0x48, 0x51, 0x54, 0x26,
                       0x12, 0x36, 0x9e, 0x12, 0x45, 0x31, 0xf0, 0x38,
                       0xf0, 0x70, 0x41, 0xb0, 0x90, 0xc3, 0x89, 0x6b}};

// The config for the TranslateKit en-pl language pack.
const LanguagePackComponentConfig kTranslateKitEnPlConfig = {
    .language1 = SupportedLanguage::kEn,
    .language2 = SupportedLanguage::kPl,
    .public_key_sha = {0x10, 0x03, 0x30, 0xeb, 0xe4, 0x8f, 0x45, 0xf3,
                       0xac, 0x73, 0x17, 0x8c, 0x9b, 0x27, 0x43, 0x81,
                       0x26, 0x16, 0xdd, 0x2c, 0xf3, 0x76, 0x76, 0xb0,
                       0xb8, 0xf8, 0xa1, 0x6b, 0x61, 0xb9, 0x71, 0x91}};

// The config for the TranslateKit en-pt language pack.
const LanguagePackComponentConfig kTranslateKitEnPtConfig = {
    .language1 = SupportedLanguage::kEn,
    .language2 = SupportedLanguage::kPt,
    .public_key_sha = {0xcd, 0x31, 0x4d, 0x32, 0x6e, 0x09, 0xe9, 0x16,
                       0xca, 0x04, 0x4b, 0xc9, 0x9b, 0x76, 0x77, 0xc2,
                       0xb0, 0x4a, 0xa9, 0xa9, 0x07, 0x14, 0x79, 0x30,
                       0xbd, 0xd1, 0xe3, 0xa9, 0x38, 0x73, 0xd9, 0xee}};

// The config for the TranslateKit en-ru language pack.
const LanguagePackComponentConfig kTranslateKitEnRuConfig = {
    .language1 = SupportedLanguage::kEn,
    .language2 = SupportedLanguage::kRu,
    .public_key_sha = {0x31, 0xe6, 0xdd, 0x3f, 0x9d, 0x86, 0xd1, 0x3d,
                       0xbe, 0xa8, 0xfb, 0x9d, 0x93, 0x12, 0xd5, 0xc2,
                       0x9c, 0x6d, 0x49, 0x81, 0xe7, 0x4e, 0x56, 0x42,
                       0xba, 0x2c, 0x44, 0x62, 0x58, 0xa2, 0x33, 0xc3}};

// The config for the TranslateKit en-th language pack.
const LanguagePackComponentConfig kTranslateKitEnThConfig = {
    .language1 = SupportedLanguage::kEn,
    .language2 = SupportedLanguage::kTh,
    .public_key_sha = {0x73, 0xab, 0x92, 0x29, 0x91, 0xea, 0x31, 0x87,
                       0x4e, 0x06, 0xb5, 0x65, 0x64, 0x7c, 0x98, 0x28,
                       0x34, 0x38, 0x53, 0x58, 0x61, 0x49, 0xa7, 0xfa,
                       0xa1, 0x63, 0xa5, 0x8b, 0x3c, 0x97, 0x4e, 0xe2}};

// The config for the TranslateKit en-tr language pack.
const LanguagePackComponentConfig kTranslateKitEnTrConfig = {
    .language1 = SupportedLanguage::kEn,
    .language2 = SupportedLanguage::kTr,
    .public_key_sha = {0xc5, 0x75, 0x1d, 0x50, 0x3e, 0x87, 0xaf, 0xad,
                       0x2b, 0xa1, 0x1e, 0x41, 0x04, 0x84, 0x77, 0xae,
                       0xb1, 0x56, 0x82, 0xc2, 0x61, 0xa9, 0xa5, 0x5d,
                       0x60, 0x36, 0x57, 0xff, 0xb7, 0x2b, 0xd8, 0x6e}};

// The config for the TranslateKit en-vi language pack.
const LanguagePackComponentConfig kTranslateKitEnViConfig = {
    .language1 = SupportedLanguage::kEn,
    .language2 = SupportedLanguage::kVi,
    .public_key_sha = {0xb3, 0xff, 0x8e, 0xa7, 0x44, 0x40, 0xce, 0xc4,
                       0xa0, 0x4c, 0xcb, 0xa8, 0xc7, 0x92, 0xf2, 0x05,
                       0x42, 0x34, 0xf7, 0x35, 0x18, 0x15, 0x37, 0x1d,
                       0x66, 0x09, 0x36, 0x18, 0xe4, 0xd4, 0x39, 0x33}};

// The config for the TranslateKit en-zh language pack.
const LanguagePackComponentConfig kTranslateKitEnZhConfig = {
    .language1 = SupportedLanguage::kEn,
    .language2 = SupportedLanguage::kZh,
    .public_key_sha = {0x1e, 0x44, 0x3e, 0x1c, 0x99, 0x33, 0x56, 0xd2,
                       0x5d, 0xcd, 0x47, 0x24, 0xc9, 0x15, 0xf5, 0x1a,
                       0xa4, 0xfa, 0x2b, 0xc1, 0x84, 0xb7, 0x63, 0xb5,
                       0x1f, 0x9c, 0x1e, 0xe3, 0x9d, 0x7e, 0x55, 0xc5}};

// The config for the TranslateKit en-zh-Hant language pack.
const LanguagePackComponentConfig kTranslateKitEnZhHantConfig = {
    .language1 = SupportedLanguage::kEn,
    .language2 = SupportedLanguage::kZhHant,
    .public_key_sha = {0xe8, 0x50, 0xdf, 0x7d, 0x99, 0xef, 0x90, 0x2e,
                       0xf0, 0x4d, 0xa5, 0x6c, 0xc8, 0xb9, 0x40, 0x4b,
                       0xc3, 0x18, 0x64, 0x25, 0x95, 0x20, 0xdd, 0x4a,
                       0xcb, 0x70, 0x7e, 0x63, 0xcf, 0xaa, 0x5f, 0x3b}};

// The config for each language pack.
constexpr auto kLanguagePackComponentConfigMap =
    base::MakeFixedFlatMap<LanguagePackKey, const LanguagePackComponentConfig*>(
        {{LanguagePackKey::kEn_Es, &kTranslateKitEnEsConfig},
         {LanguagePackKey::kEn_Ja, &kTranslateKitEnJaConfig},
         {LanguagePackKey::kAr_En, &kTranslateKitArEnConfig},
         {LanguagePackKey::kBn_En, &kTranslateKitBnEnConfig},
         {LanguagePackKey::kDe_En, &kTranslateKitDeEnConfig},
         {LanguagePackKey::kEn_Fr, &kTranslateKitEnFrConfig},
         {LanguagePackKey::kEn_Hi, &kTranslateKitEnHiConfig},
         {LanguagePackKey::kEn_It, &kTranslateKitEnItConfig},
         {LanguagePackKey::kEn_Ko, &kTranslateKitEnKoConfig},
         {LanguagePackKey::kEn_Nl, &kTranslateKitEnNlConfig},
         {LanguagePackKey::kEn_Pl, &kTranslateKitEnPlConfig},
         {LanguagePackKey::kEn_Pt, &kTranslateKitEnPtConfig},
         {LanguagePackKey::kEn_Ru, &kTranslateKitEnRuConfig},
         {LanguagePackKey::kEn_Th, &kTranslateKitEnThConfig},
         {LanguagePackKey::kEn_Tr, &kTranslateKitEnTrConfig},
         {LanguagePackKey::kEn_Vi, &kTranslateKitEnViConfig},
         {LanguagePackKey::kEn_Zh, &kTranslateKitEnZhConfig},
         {LanguagePackKey::kEn_ZhHant, &kTranslateKitEnZhHantConfig}});
static_assert(std::size(kLanguagePackComponentConfigMap) ==
                  static_cast<unsigned>(LanguagePackKey::kMaxValue) + 1,
              "All language packs must be in kLanguagePackComponentConfigMap.");

// Returns the config for a language pack component.
const LanguagePackComponentConfig& GetLanguagePackComponentConfig(
    LanguagePackKey key);

// Calculates the required language packs for a translation from source_lang to
// target_lang.
std::set<LanguagePackKey> CalculateRequiredLanguagePacks(
    const std::string& source_lang,
    const std::string& target_lang);

// Returns the name for the install directory of a language pack.
// eg: "en_es".
std::string GetPackageInstallDirName(LanguagePackKey);

// Returns the suffix for the package name of a language pack.
// eg: "en-es".
std::string GetPackageNameSuffix(LanguagePackKey);

// Returns the names of sub-directories in the package install directory that
// need to be verified.
std::vector<std::string> GetPackageInstallSubDirNamesForVerification(
    LanguagePackKey);

}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_LANGUAGE_PACK_UTIL_H_
