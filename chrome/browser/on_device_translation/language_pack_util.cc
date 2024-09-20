// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/language_pack_util.h"

#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/strings/strcat.h"

namespace on_device_translation {
namespace {

// Currently we always translate via English, so the number of
// SupportedLanguages needs to include English in addition to all the
// LanguagePackKeys.
static_assert(static_cast<unsigned>(SupportedLanguage::kMaxValue) ==
                  static_cast<unsigned>(LanguagePackKey::kMaxValue) + 1,
              "Missmatching SupportedLanguage size and LanguagePackKey size");

// The supported languages for on-device translation.
inline constexpr auto kSupportedLanguageCodeMap =
    base::MakeFixedFlatMap<SupportedLanguage, std::string_view>({
        {SupportedLanguage::kEn, "en"},
        {SupportedLanguage::kEs, "es"},
        {SupportedLanguage::kJa, "ja"},
    });
static_assert(std::size(kSupportedLanguageCodeMap) ==
                  static_cast<unsigned>(SupportedLanguage::kMaxValue) + 1,
              "All languages must be in kSupportedLanguageCodeMap.");

// The inverse of kSupportedLanguageCodeMap.
inline constexpr auto kSupportedLanguageCodeInverseMap =
    base::MakeFixedFlatMap<std::string_view, SupportedLanguage>({
        {"en", SupportedLanguage::kEn},
        {"es", SupportedLanguage::kEs},
        {"ja", SupportedLanguage::kJa},
    });
static_assert(std::size(kSupportedLanguageCodeInverseMap) ==
                  static_cast<unsigned>(SupportedLanguage::kMaxValue) + 1,
              "All languages must be in kSupportedLanguageCodeInverseMap.");

LanguagePackKey LanguagePackKeyFromNonEnglishSupportedLanguage(
    SupportedLanguage supported_language) {
  CHECK_NE(supported_language, SupportedLanguage::kEn);
  return static_cast<LanguagePackKey>(
      static_cast<unsigned>(supported_language) - 1);
}

SupportedLanguage NonEnglishSupportedLanguageFromLanguagePackKey(
    LanguagePackKey language_pack_key) {
  return static_cast<SupportedLanguage>(
      static_cast<unsigned>(language_pack_key) + 1);
}

}  // namespace

// Converts a SupportedLanguage to a language code.
std::string_view ToLanguageCode(SupportedLanguage supported_language) {
  return kSupportedLanguageCodeMap.at(supported_language);
}

// Converts a language code to a SupportedLanguage.
std::optional<SupportedLanguage> ToSupportedLanguage(
    std::string_view language_code) {
  auto it = kSupportedLanguageCodeInverseMap.find(language_code);
  if (it != kSupportedLanguageCodeInverseMap.end()) {
    return it->second;
  }
  return std::nullopt;
}

// Returns the config for a language pack component.
const LanguagePackComponentConfig& GetLanguagePackComponentConfig(
    LanguagePackKey key) {
  return *kLanguagePackComponentConfigMap.at(key);
}

// Calculates the required language packs for a translation from source_lang to
// target_lang.
// Note: Currently, this method is implemented assuming that translation between
// non-English languages is done by first translating to English. This logic
// needs to be updated when direct translation between non-English languages is
// supported by the library.
std::set<LanguagePackKey> CalculateRequiredLanguagePacks(
    const std::string& source_lang,
    const std::string& target_lang) {
  auto source_lang_code = ToSupportedLanguage(source_lang);
  auto target_lang_code = ToSupportedLanguage(target_lang);
  if (!source_lang_code.has_value() || !target_lang_code.has_value() ||
      source_lang_code == target_lang_code) {
    return {};
  }
  if (*source_lang_code == SupportedLanguage::kEn) {
    return {LanguagePackKeyFromNonEnglishSupportedLanguage(*target_lang_code)};
  }
  if (*target_lang_code == SupportedLanguage::kEn) {
    return {LanguagePackKeyFromNonEnglishSupportedLanguage(*source_lang_code)};
  }
  return {LanguagePackKeyFromNonEnglishSupportedLanguage(*source_lang_code),
          LanguagePackKeyFromNonEnglishSupportedLanguage(*target_lang_code)};
}

std::string GetPackageInstallDirName(LanguagePackKey language_pack_key) {
  return base::StrCat(
      {"en_", ToLanguageCode(NonEnglishSupportedLanguageFromLanguagePackKey(
                  language_pack_key))});
}

std::string GetPackageNameSuffix(LanguagePackKey language_pack_key) {
  return base::StrCat(
      {"en-", ToLanguageCode(NonEnglishSupportedLanguageFromLanguagePackKey(
                  language_pack_key))});
}

std::vector<std::string> GetPackageInstallSubDirNamesForVerification(
    LanguagePackKey language_pack_key) {
  const std::string_view non_english_language = ToLanguageCode(
      NonEnglishSupportedLanguageFromLanguagePackKey(language_pack_key));
  return {
      base::StrCat({"en_", non_english_language, "_dictionary"}),
      base::StrCat({"en_", non_english_language, "_nmt"}),
      base::StrCat({non_english_language, "_en_nmt"}),
  };
}

}  // namespace on_device_translation
