// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/language_pack_util.h"

#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/strings/strcat.h"

namespace on_device_translation {
namespace {

constexpr char kPrefNamePrefix[] =
    "on_device_translation.translate_kit_packages.";
constexpr char kComponentPathPrefNameSuffix[] = "_path";
constexpr char kRegisteredFlagPrefNameSuffix[] = "_registered";

// Currently we always translate via English, so the number of
// SupportedLanguages needs to include English in addition to all the
// LanguagePackKeys.
static_assert(static_cast<unsigned>(SupportedLanguage::kMaxValue) ==
                  static_cast<unsigned>(LanguagePackKey::kMaxValue) + 1,
              "Missmatching SupportedLanguage size and LanguagePackKey size");

// The supported languages for on-device translation.
inline constexpr auto kSupportedLanguageCodeMap = base::MakeFixedFlatMap<
    SupportedLanguage,
    std::string_view>(
    {{SupportedLanguage::kEn, "en"},          {SupportedLanguage::kEs, "es"},
     {SupportedLanguage::kJa, "ja"},          {SupportedLanguage::kAr, "ar"},
     {SupportedLanguage::kBn, "bn"},          {SupportedLanguage::kDe, "de"},
     {SupportedLanguage::kFr, "fr"},          {SupportedLanguage::kHi, "hi"},
     {SupportedLanguage::kIt, "it"},          {SupportedLanguage::kKo, "ko"},
     {SupportedLanguage::kNl, "nl"},          {SupportedLanguage::kPl, "pl"},
     {SupportedLanguage::kPt, "pt"},          {SupportedLanguage::kRu, "ru"},
     {SupportedLanguage::kTh, "th"},          {SupportedLanguage::kTr, "tr"},
     {SupportedLanguage::kVi, "vi"},          {SupportedLanguage::kZh, "zh"},
     {SupportedLanguage::kZhHant, "zh-Hant"}, {SupportedLanguage::kBg, "bg"},
     {SupportedLanguage::kCs, "cs"},          {SupportedLanguage::kDa, "da"},
     {SupportedLanguage::kEl, "el"},          {SupportedLanguage::kFi, "fi"},
     {SupportedLanguage::kHr, "hr"},          {SupportedLanguage::kHu, "hu"},
     {SupportedLanguage::kId, "id"},          {SupportedLanguage::kIw, "iw"},
     {SupportedLanguage::kLt, "lt"},          {SupportedLanguage::kNo, "no"},
     {SupportedLanguage::kRo, "ro"},          {SupportedLanguage::kSk, "sk"},
     {SupportedLanguage::kSl, "sl"},          {SupportedLanguage::kSv, "sv"},
     {SupportedLanguage::kUk, "uk"}});
static_assert(std::size(kSupportedLanguageCodeMap) ==
                  static_cast<unsigned>(SupportedLanguage::kMaxValue) + 1,
              "All languages must be in kSupportedLanguageCodeMap.");

// Returns the language pair for a non-English supported language. The order of
// the language pair is determined by the language code's alphabetical order.
inline std::pair<SupportedLanguage, SupportedLanguage>
SupportedLanguagePairFromNonEnglishSupportedLanguage(
    SupportedLanguage supported_language) {
  CHECK_NE(supported_language, SupportedLanguage::kEn);
  if (kSupportedLanguageCodeMap.at(supported_language) < "en") {
    return std::make_pair(supported_language, SupportedLanguage::kEn);
  } else {
    return std::make_pair(SupportedLanguage::kEn, supported_language);
  }
}

// The inverse of kSupportedLanguageCodeMap.
inline constexpr auto kSupportedLanguageCodeInverseMap = base::MakeFixedFlatMap<
    std::string_view,
    SupportedLanguage>(
    {{"en", SupportedLanguage::kEn},          {"es", SupportedLanguage::kEs},
     {"ja", SupportedLanguage::kJa},          {"ar", SupportedLanguage::kAr},
     {"bn", SupportedLanguage::kBn},          {"de", SupportedLanguage::kDe},
     {"fr", SupportedLanguage::kFr},          {"hi", SupportedLanguage::kHi},
     {"it", SupportedLanguage::kIt},          {"ko", SupportedLanguage::kKo},
     {"nl", SupportedLanguage::kNl},          {"pl", SupportedLanguage::kPl},
     {"pt", SupportedLanguage::kPt},          {"ru", SupportedLanguage::kRu},
     {"th", SupportedLanguage::kTh},          {"tr", SupportedLanguage::kTr},
     {"vi", SupportedLanguage::kVi},          {"zh", SupportedLanguage::kZh},
     {"zh-Hant", SupportedLanguage::kZhHant}, {"bg", SupportedLanguage::kBg},
     {"cs", SupportedLanguage::kCs},          {"da", SupportedLanguage::kDa},
     {"el", SupportedLanguage::kEl},          {"fi", SupportedLanguage::kFi},
     {"hr", SupportedLanguage::kHr},          {"hu", SupportedLanguage::kHu},
     {"id", SupportedLanguage::kId},          {"iw", SupportedLanguage::kIw},
     {"lt", SupportedLanguage::kLt},          {"no", SupportedLanguage::kNo},
     {"ro", SupportedLanguage::kRo},          {"sk", SupportedLanguage::kSk},
     {"sl", SupportedLanguage::kSl},          {"sv", SupportedLanguage::kSv},
     {"uk", SupportedLanguage::kUk}});
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

bool IsPopularLanguage(SupportedLanguage supported_language) {
  return supported_language == SupportedLanguage::kEn ||
         supported_language == SupportedLanguage::kZh ||
         supported_language == SupportedLanguage::kZhHant ||
         supported_language == SupportedLanguage::kJa ||
         supported_language == SupportedLanguage::kPt ||
         supported_language == SupportedLanguage::kRu ||
         supported_language == SupportedLanguage::kEs ||
         supported_language == SupportedLanguage::kTr ||
         supported_language == SupportedLanguage::kHi ||
         supported_language == SupportedLanguage::kVi ||
         supported_language == SupportedLanguage::kBn;
}

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

std::string GetComponentPathPrefName(
    const LanguagePackComponentConfig& config) {
  return base::StrCat({kPrefNamePrefix, ToLanguageCode(config.language1), "_",
                       ToLanguageCode(config.language2),
                       kComponentPathPrefNameSuffix});
}

std::string GetRegisteredFlagPrefName(
    const LanguagePackComponentConfig& config) {
  return base::StrCat({kPrefNamePrefix, ToLanguageCode(config.language1), "_",
                       ToLanguageCode(config.language2),
                       kRegisteredFlagPrefNameSuffix});
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
  const auto supported_language =
      NonEnglishSupportedLanguageFromLanguagePackKey(language_pack_key);
  const auto lang_pair =
      SupportedLanguagePairFromNonEnglishSupportedLanguage(supported_language);
  return base::StrCat(
      {ToLanguageCode(lang_pair.first), "_", ToLanguageCode(lang_pair.second)});
}

std::string GetPackageNameSuffix(LanguagePackKey language_pack_key) {
  const auto supported_language =
      NonEnglishSupportedLanguageFromLanguagePackKey(language_pack_key);
  const auto lang_pair =
      SupportedLanguagePairFromNonEnglishSupportedLanguage(supported_language);
  return base::StrCat(
      {ToLanguageCode(lang_pair.first), "-", ToLanguageCode(lang_pair.second)});
}

std::vector<std::string> GetPackageInstallSubDirNamesForVerification(
    LanguagePackKey language_pack_key) {
  const auto supported_language =
      NonEnglishSupportedLanguageFromLanguagePackKey(language_pack_key);
  const auto lang_pair =
      SupportedLanguagePairFromNonEnglishSupportedLanguage(supported_language);
  return {
      base::StrCat({ToLanguageCode(lang_pair.first), "_",
                    ToLanguageCode(lang_pair.second), "_dictionary"}),
      base::StrCat({ToLanguageCode(lang_pair.first), "_",
                    ToLanguageCode(lang_pair.second), "_nmt"}),
      base::StrCat({ToLanguageCode(lang_pair.second), "_",
                    ToLanguageCode(lang_pair.first), "_nmt"}),
  };
}

std::string_view GetSourceLanguageCode(LanguagePackKey language_pack_key) {
  const SupportedLanguage supported_language =
      NonEnglishSupportedLanguageFromLanguagePackKey(language_pack_key);
  const auto [source_lang, _] =
      SupportedLanguagePairFromNonEnglishSupportedLanguage(supported_language);
  return ToLanguageCode(source_lang);
}

std::string_view GetTargetLanguageCode(LanguagePackKey language_pack_key) {
  const SupportedLanguage supported_language =
      NonEnglishSupportedLanguageFromLanguagePackKey(language_pack_key);
  const auto [_, target_lang] =
      SupportedLanguagePairFromNonEnglishSupportedLanguage(supported_language);
  return ToLanguageCode(target_lang);
}

}  // namespace on_device_translation
