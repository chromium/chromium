// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_config_factory.h"

#include "ash/constants/ash_features.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/ash/input_method/editor_helpers.h"
#include "chrome/browser/ash/input_method/input_methods_by_language.h"
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"

namespace ash::input_method {
namespace {

constexpr char kDefaultLanguageCode[] = "en";

orca::mojom::EditorConfigPtr EnglishConfig() {
  std::vector<orca::mojom::PresetTextQueryType> allowed;
  if (base::FeatureList::IsEnabled(features::kOrcaElaborate)) {
    allowed.push_back(orca::mojom::PresetTextQueryType::kElaborate);
  }
  if (base::FeatureList::IsEnabled(features::kOrcaEmojify)) {
    allowed.push_back(orca::mojom::PresetTextQueryType::kEmojify);
  }
  if (base::FeatureList::IsEnabled(features::kOrcaFormalize)) {
    allowed.push_back(orca::mojom::PresetTextQueryType::kFormalize);
  }
  if (base::FeatureList::IsEnabled(features::kOrcaProofread)) {
    allowed.push_back(orca::mojom::PresetTextQueryType::kProofread);
  }
  if (base::FeatureList::IsEnabled(features::kOrcaRephrase)) {
    allowed.push_back(orca::mojom::PresetTextQueryType::kRephrase);
  }
  if (base::FeatureList::IsEnabled(features::kOrcaShorten)) {
    allowed.push_back(orca::mojom::PresetTextQueryType::kShorten);
  }
  return orca::mojom::EditorConfig::New(
      /*allowed_types=*/std::move(allowed),
      /*language_code=*/ShouldUseL10nStrings()
          ? GetSystemLocale()
          : std::string(kDefaultLanguageCode));
}

orca::mojom::EditorConfigPtr InternationalizedConfig() {
  std::vector<orca::mojom::PresetTextQueryType> allowed;
  if (base::FeatureList::IsEnabled(features::kOrcaInternationalizeElaborate)) {
    allowed.push_back(orca::mojom::PresetTextQueryType::kElaborate);
  }
  if (base::FeatureList::IsEnabled(features::kOrcaInternationalizeEmojify)) {
    allowed.push_back(orca::mojom::PresetTextQueryType::kEmojify);
  }
  if (base::FeatureList::IsEnabled(features::kOrcaInternationalizeFormalize)) {
    allowed.push_back(orca::mojom::PresetTextQueryType::kFormalize);
  }
  if (base::FeatureList::IsEnabled(features::kOrcaInternationalizeProofread)) {
    allowed.push_back(orca::mojom::PresetTextQueryType::kProofread);
  }
  if (base::FeatureList::IsEnabled(features::kOrcaInternationalizeRephrase)) {
    allowed.push_back(orca::mojom::PresetTextQueryType::kRephrase);
  }
  if (base::FeatureList::IsEnabled(features::kOrcaInternationalizeShorten)) {
    allowed.push_back(orca::mojom::PresetTextQueryType::kShorten);
  }
  return orca::mojom::EditorConfig::New(
      /*allowed_types=*/std::move(allowed),
      /*language_code=*/ShouldUseL10nStrings()
          ? GetSystemLocale()
          : std::string(kDefaultLanguageCode));
}

}  // namespace

orca::mojom::EditorConfigPtr BuildConfigFor(const LanguageCategory& language) {
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
      return InternationalizedConfig();
    case LanguageCategory::kEnglish:
    default:
      return EnglishConfig();
  }
}

}  // namespace ash::input_method
