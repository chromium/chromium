// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_config_factory.h"

#include <string>
#include <utility>

#include "base/i18n/language_tag.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/ash/input_method/input_methods_by_language.h"
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"

namespace ash::input_method {
namespace {

orca::mojom::EditorConfigPtr EnglishConfig(
    const base::i18n::LanguageTag& locale) {
  std::vector<orca::mojom::PresetTextQueryType> allowed = {
      orca::mojom::PresetTextQueryType::kElaborate,
      orca::mojom::PresetTextQueryType::kEmojify,
      orca::mojom::PresetTextQueryType::kFormalize,
      orca::mojom::PresetTextQueryType::kProofread,
      orca::mojom::PresetTextQueryType::kRephrase,
      orca::mojom::PresetTextQueryType::kShorten,
  };
  return orca::mojom::EditorConfig::New(
      /*allowed_types=*/std::move(allowed),
      /*language_code=*/std::string(locale.tag_string()));
}

orca::mojom::EditorConfigPtr InternationalizedConfig(
    const base::i18n::LanguageTag& locale) {
  std::vector<orca::mojom::PresetTextQueryType> allowed = {
      orca::mojom::PresetTextQueryType::kElaborate,
      orca::mojom::PresetTextQueryType::kEmojify,
      orca::mojom::PresetTextQueryType::kFormalize,
      orca::mojom::PresetTextQueryType::kProofread,
      orca::mojom::PresetTextQueryType::kRephrase,
      orca::mojom::PresetTextQueryType::kShorten,
  };
  return orca::mojom::EditorConfig::New(
      /*allowed_types=*/std::move(allowed),
      /*language_code=*/std::string(locale.tag_string()));
}

}  // namespace

orca::mojom::EditorConfigPtr BuildConfigFor(
    LanguageCategory language,
    const base::i18n::LanguageTag& locale) {
  switch (language) {
    case LanguageCategory::kDanish:
    case LanguageCategory::kDutch:
    case LanguageCategory::kFinnish:
    case LanguageCategory::kFrench:
    case LanguageCategory::kGerman:
    case LanguageCategory::kItalian:
    case LanguageCategory::kJapanese:
    case LanguageCategory::kNorwegian:
    case LanguageCategory::kPortugese:
    case LanguageCategory::kSpanish:
    case LanguageCategory::kSwedish:
      return InternationalizedConfig(locale);
    case LanguageCategory::kEnglish:
    default:
      return EnglishConfig(locale);
  }
}

}  // namespace ash::input_method
