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
  kMaxValue = kJa,
};

// Converts a SupportedLanguage to a language code.
std::string_view ToLanguageCode(SupportedLanguage supported_language);

// Converts a language code to a SupportedLanguage.
std::optional<SupportedLanguage> ToSupportedLanguage(
    std::string_view language_code);

// The key for language pack components.
enum class LanguagePackKey {
  kEn_Es = 0,
  kEn_Ja = 1,
  kMaxValue = kEn_Ja,
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

// The config for each language pack.
constexpr auto kLanguagePackComponentConfigMap =
    base::MakeFixedFlatMap<LanguagePackKey, const LanguagePackComponentConfig*>(
        {{LanguagePackKey::kEn_Es, &kTranslateKitEnEsConfig},
         {LanguagePackKey::kEn_Ja, &kTranslateKitEnJaConfig}});
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
