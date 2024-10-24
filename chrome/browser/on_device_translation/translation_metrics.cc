// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/translation_metrics.h"

#include <sstream>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/on_device_translation/language_pack_util.h"

namespace on_device_translation {
namespace {

constexpr int kLanguagePairOffset = 1000;
static_assert(static_cast<int>(SupportedLanguage::kMaxValue) <
              kLanguagePairOffset);

std::string GetSourceUMANameForAPI(std::string_view api_name) {
  return base::StrCat(
      {"Translate.OnDeviceTranslation.", api_name, ".SourceLanguage"});
}

std::string GetTargetUMANameForAPI(std::string_view api_name) {
  return base::StrCat(
      {"Translate.OnDeviceTranslation.", api_name, ".TargetLanguage"});
}

std::string GetPairUMANameForAPI(std::string_view api_name) {
  return base::StrCat(
      {"Translate.OnDeviceTranslation.", api_name, ".LanguagePair"});
}

std::string GetCharacterCountUMAForSourceLanguage(
    std::string_view source_lang) {
  return base::StrCat({"Translate.OnDeviceTranslation.SourceLanguage.",
                       source_lang, ".CharacterCount"});
}

std::string GetCharacterCountUMAForTargetLanguage(
    std::string_view target_lang) {
  return base::StrCat({"Translate.OnDeviceTranslation.TargetLanguage.",
                       target_lang, ".CharacterCount"});
}
}  // namespace

void RecordLanguageUma(std::string_view uma_name,
                       std::string_view language_code) {
  std::optional<SupportedLanguage> code = ToSupportedLanguage(language_code);
  if (code.has_value()) {
    base::UmaHistogramEnumeration(uma_name, code.value());
  }
}

void RecordLanguagePairUma(std::string_view uma_name,
                           std::string_view source_lang,
                           std::string_view target_lang) {
  std::optional<SupportedLanguage> source_code =
      ToSupportedLanguage(source_lang);
  std::optional<SupportedLanguage> target_code =
      ToSupportedLanguage(target_lang);
  if (source_code.has_value() && target_code.has_value()) {
    base::UmaHistogramSparse(
        uma_name, static_cast<int>(source_code.value()) * kLanguagePairOffset +
                      static_cast<int>(target_code.value()));
  }
}

void RecordTranslationAPICallForLanguagePair(std::string_view api_name,
                                             std::string_view source_lang,
                                             std::string_view target_lang) {
  if (!ToSupportedLanguage(source_lang).has_value() ||
      !ToSupportedLanguage(target_lang).has_value()) {
    return;
  }
  RecordLanguageUma(GetSourceUMANameForAPI(api_name), source_lang);
  RecordLanguageUma(GetTargetUMANameForAPI(api_name), target_lang);
  RecordLanguagePairUma(GetPairUMANameForAPI(api_name), source_lang,
                        target_lang);
}

void RecordTranslationCharacterCount(std::string_view source_lang,
                                     std::string_view target_lang,
                                     int character_count) {
  // The translate() API call requires the source and target language to be
  // supported.
  CHECK(ToSupportedLanguage(source_lang).has_value());
  CHECK(ToSupportedLanguage(target_lang).has_value());
  base::UmaHistogramCounts1M("Translate.OnDeviceTranslation.CharacterCount",
                             character_count);
  base::UmaHistogramCounts1M(GetCharacterCountUMAForSourceLanguage(source_lang),
                             character_count);
  base::UmaHistogramCounts1M(GetCharacterCountUMAForTargetLanguage(target_lang),
                             character_count);
}

}  // namespace on_device_translation
