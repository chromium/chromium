// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_metrics_recorder.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_context.h"
#include "chrome/browser/ash/input_method/editor_metrics_enums.h"
#include "chrome/browser/ash/input_method/input_methods_by_language.h"
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace ash::input_method {

namespace {

constexpr int kMaxNumResponsesFromServer = 20;

std::string GetToneStringFromEnum(EditorTone tone) {
  switch (tone) {
    case EditorTone::kRephrase:
      return "Rephrase";
    case EditorTone::kEmojify:
      return "Emojify";
    case EditorTone::kShorten:
      return "Shorten";
    case EditorTone::kElaborate:
      return "Elaborate";
    case EditorTone::kFormalize:
      return "Formalize";
    case EditorTone::kProofread:
      return "Proofread";
    case EditorTone::kFreeformRewrite:
      return "FreeformRewrite";
    case EditorTone::kUnset:
      return "Unset";
    case EditorTone::kUnknown:
      return "Unknown";
  }
}

EditorTone GetEditorToneFromString(std::string_view tone) {
  if (tone == "REPHRASE") {
    return EditorTone::kRephrase;
  }
  if (tone == "EMOJIFY") {
    return EditorTone::kEmojify;
  }
  if (tone == "SHORTEN") {
    return EditorTone::kShorten;
  }
  if (tone == "ELABORATE") {
    return EditorTone::kElaborate;
  }
  if (tone == "FORMALIZE") {
    return EditorTone::kFormalize;
  }
  if (tone == "PROOFREAD") {
    return EditorTone::kProofread;
  }
  return EditorTone::kUnknown;
}

std::string_view AsString(const EditorOpportunityMode& mode) {
  switch (mode) {
    case EditorOpportunityMode::kWrite:
      return "Write";
    case EditorOpportunityMode::kRewrite:
      return "Rewrite";
    case EditorOpportunityMode::kNotAllowedForUse:
      return "NotAllowed";
    case EditorOpportunityMode::kInvalidInput:
      return "InvalidInput";
  }
}

std::string_view AsString(const LanguageCategory& category) {
  switch (category) {
    case LanguageCategory::kAfrikaans:
      return "Afrikaans";
    case LanguageCategory::kDanish:
      return "Danish";
    case LanguageCategory::kDutch:
      return "Dutch";
    case LanguageCategory::kFinnish:
      return "Finnish";
    case LanguageCategory::kEnglish:
      return "English";
    case LanguageCategory::kFrench:
      return "French";
    case LanguageCategory::kGerman:
      return "German";
    case LanguageCategory::kItalian:
      return "Italian";
    case LanguageCategory::kJapanese:
      return "Japanese";
    case LanguageCategory::kNorwegian:
      return "Norwegian";
    case LanguageCategory::kPolish:
      return "Polish";
    case LanguageCategory::kPortugese:
      return "Portugese";
    case LanguageCategory::kSpanish:
      return "Spanish";
    case LanguageCategory::kSwedish:
      return "Swedish";
    default:
      return "Other";
  }
}

std::optional<EditorCriticalStates> AsCriticalState(const EditorStates& state) {
  switch (state) {
    case EditorStates::kNativeUIShown:
    case EditorStates::kPromoCardImpression:
      return EditorCriticalStates::kShowUI;
    case EditorStates::kNativeRequest:
    case EditorStates::kWebUIRequest:
      return EditorCriticalStates::kRequestTriggered;
    case EditorStates::kInsert:
      return EditorCriticalStates::kTextInserted;
    default:
      return std::nullopt;
  }
}

std::string_view AsEnglishOrOther(const LanguageCategory& category) {
  switch (category) {
    case LanguageCategory::kEnglish:
      return "English";
    default:
      return "Other";
  }
}

bool IsInternationalizedPathEnabled() {
  return base::FeatureList::IsEnabled(features::kOrcaAfrikaans) ||
         base::FeatureList::IsEnabled(features::kOrcaDanish) ||
         base::FeatureList::IsEnabled(features::kOrcaDutch) ||
         base::FeatureList::IsEnabled(features::kOrcaFinnish) ||
         base::FeatureList::IsEnabled(features::kOrcaFrench) ||
         base::FeatureList::IsEnabled(features::kOrcaGerman) ||
         base::FeatureList::IsEnabled(features::kOrcaItalian) ||
         base::FeatureList::IsEnabled(features::kOrcaJapanese) ||
         base::FeatureList::IsEnabled(features::kOrcaNorwegian) ||
         base::FeatureList::IsEnabled(features::kOrcaPolish) ||
         base::FeatureList::IsEnabled(features::kOrcaPortugese) ||
         base::FeatureList::IsEnabled(features::kOrcaSpanish) ||
         base::FeatureList::IsEnabled(features::kOrcaSwedish);
}

}  // namespace

EditorStates ToEditorStatesMetric(EditorBlockedReason reason) {
  switch (reason) {
    case EditorBlockedReason::kBlockedByConsent:
      return EditorStates::kBlockedByConsent;
    case EditorBlockedReason::kBlockedBySetting:
      return EditorStates::kBlockedBySetting;
    case EditorBlockedReason::kBlockedByTextLength:
      return EditorStates::kBlockedByTextLength;
    case EditorBlockedReason::kBlockedByUrl:
      return EditorStates::kBlockedByUrl;
    case EditorBlockedReason::kBlockedByApp:
      return EditorStates::kBlockedByApp;
    case EditorBlockedReason::kBlockedByInputMethod:
      return EditorStates::kBlockedByInputMethod;
    case EditorBlockedReason::kBlockedByInputType:
      return EditorStates::kBlockedByInputType;
    case EditorBlockedReason::kBlockedByAppType:
      return EditorStates::kBlockedByAppType;
    case EditorBlockedReason::kBlockedByInvalidFormFactor:
      return EditorStates::kBlockedByInvalidFormFactor;
    case EditorBlockedReason::kBlockedByNetworkStatus:
      return EditorStates::kBlockedByNetworkStatus;
    case EditorBlockedReason::kBlockedByUnsupportedRegion:
      return EditorStates::kBlockedByUnsupportedRegion;
    case EditorBlockedReason::kBlockedByPolicy:
      return EditorStates::kBlockedByPolicy;
    case EditorBlockedReason::kBlockedByUnknownCapability:
      return EditorStates::kBlockedByUnknownCapability;
    case EditorBlockedReason::kBlockedByUnsupportedCapability:
      return EditorStates::kBlockedByUnsupportedCapability;
  }
}

EditorStates ToEditorStatesMetric(orca::mojom::TextQueryErrorCode error_code) {
  switch (error_code) {
    case orca::mojom::TextQueryErrorCode::kUnknown:
      return EditorStates::kErrorUnknown;
    case orca::mojom::TextQueryErrorCode::kInvalidArgument:
      return EditorStates::kErrorInvalidArgument;
    case orca::mojom::TextQueryErrorCode::kResourceExhausted:
      return EditorStates::kErrorResourceExhausted;
    case orca::mojom::TextQueryErrorCode::kBackendFailure:
      return EditorStates::kErrorBackendFailure;
    case orca::mojom::TextQueryErrorCode::kNoInternetConnection:
      return EditorStates::kErrorNoInternetConnection;
    case orca::mojom::TextQueryErrorCode::kUnsupportedLanguage:
      return EditorStates::kErrorUnsupportedLanguage;
    case orca::mojom::TextQueryErrorCode::kBlockedOutputs:
      return EditorStates::kErrorBlockedOutputs;
    case orca::mojom::TextQueryErrorCode::kRestrictedRegion:
      return EditorStates::kErrorRestrictedRegion;
  }
}

std::optional<EditorStates> ToEditorStatesMetric(
    orca::mojom::MetricEvent metric_event) {
  switch (metric_event) {
    case orca::mojom::MetricEvent::kRefineRequest:
      return EditorStates::kRefineRequest;
    case orca::mojom::MetricEvent::kFeedbackThumbsUp:
      return EditorStates::kThumbsUp;
    case orca::mojom::MetricEvent::kFeedbackThumbsDown:
      return EditorStates::kThumbsDown;
    case orca::mojom::MetricEvent::kReturnToPreviousSuggestions:
      return EditorStates::kReturnToPreviousSuggestions;
    case orca::mojom::MetricEvent::kWebUIRequest:
      return EditorStates::kWebUIRequest;
    case orca::mojom::MetricEvent::kUnknown:
      return std::nullopt;
  }
}

EditorTone ToEditorMetricTone(orca::mojom::TriggerContextPtr trigger_context) {
  if (trigger_context->freeform_selected) {
    return EditorTone::kFreeformRewrite;
  }
  switch (trigger_context->preset_type_selected) {
    case orca::mojom::PresetTextQueryType::kShorten:
      return EditorTone::kShorten;
    case orca::mojom::PresetTextQueryType::kElaborate:
      return EditorTone::kElaborate;
    case orca::mojom::PresetTextQueryType::kRephrase:
      return EditorTone::kRephrase;
    case orca::mojom::PresetTextQueryType::kFormalize:
      return EditorTone::kFormalize;
    case orca::mojom::PresetTextQueryType::kEmojify:
      return EditorTone::kEmojify;
    case orca::mojom::PresetTextQueryType::kProofread:
      return EditorTone::kProofread;
    case orca::mojom::PresetTextQueryType::kUnknown:
      return EditorTone::kUnknown;
  }
}

EditorMetricsRecorder::EditorMetricsRecorder(EditorContext* context,
                                             EditorOpportunityMode mode)
    : context_(context), mode_(mode) {}

void EditorMetricsRecorder::SetMode(EditorOpportunityMode mode) {
  mode_ = mode;
}

void EditorMetricsRecorder::SetTone(
    std::optional<std::string_view> preset_query_id,
    std::optional<std::string_view> freeform_text) {
  if (freeform_text.has_value() && !freeform_text->empty()) {
    tone_ = EditorTone::kFreeformRewrite;
    return;
  }
  if (!preset_query_id.has_value()) {
    return;
  }

  tone_ = GetEditorToneFromString(*preset_query_id);
}

void EditorMetricsRecorder::SetTone(EditorTone tone) {
  tone_ = tone;
}

void EditorMetricsRecorder::LogEditorState(EditorStates state) {
  base::UmaHistogramEnumeration(
      base::StrCat({"InputMethod.Manta.Orca.States.", AsString(mode_)}), state);

  if (IsInternationalizedPathEnabled()) {
    base::UmaHistogramEnumeration(
        base::StrCat({"InputMethod.Manta.Orca.",
                      AsString(InputMethodToLanguageCategory(
                          context_->active_engine_id())),
                      ".States.", AsString(mode_)}),
        state);
  }

  if (std::optional<EditorCriticalStates> critical_state =
          AsCriticalState(state);
      critical_state != std::nullopt) {
    LogEditorCriticalState(*critical_state);
  }

  if (mode_ != EditorOpportunityMode::kRewrite) {
    return;
  }

  base::UmaHistogramEnumeration(base::StrCat({"InputMethod.Manta.Orca.States.",
                                              GetToneStringFromEnum(tone_)}),
                                state);
}

void EditorMetricsRecorder::LogNumberOfCharactersInserted(
    int number_of_characters) {
  if (mode_ == EditorOpportunityMode::kInvalidInput ||
      mode_ == EditorOpportunityMode::kNotAllowedForUse) {
    return;
  }

  base::UmaHistogramCounts100000(
      base::StrCat(
          {"InputMethod.Manta.Orca.CharactersInserted.", AsString(mode_)}),
      number_of_characters);

  if (IsInternationalizedPathEnabled()) {
    base::UmaHistogramCounts100000(
        base::StrCat({"InputMethod.Manta.Orca.",
                      AsEnglishOrOther(InputMethodToLanguageCategory(
                          context_->active_engine_id())),
                      ".CharactersInserted.", AsString(mode_)}),
        number_of_characters);
  }

  if (mode_ != EditorOpportunityMode::kRewrite) {
    return;
  }

  base::UmaHistogramCounts100000("InputMethod.Manta.Orca.CharactersInserted." +
                                     GetToneStringFromEnum(tone_),
                                 number_of_characters);
}

void EditorMetricsRecorder::LogNumberOfCharactersSelectedForInsert(
    int number_of_characters) {
  if (mode_ == EditorOpportunityMode::kInvalidInput ||
      mode_ == EditorOpportunityMode::kNotAllowedForUse) {
    return;
  }

  base::UmaHistogramCounts100000(
      base::StrCat({"InputMethod.Manta.Orca.CharactersSelectedForInsert.",
                    AsString(mode_)}),
      number_of_characters);

  if (IsInternationalizedPathEnabled()) {
    base::UmaHistogramCounts100000(
        base::StrCat({"InputMethod.Manta.Orca.",
                      AsEnglishOrOther(InputMethodToLanguageCategory(
                          context_->active_engine_id())),
                      ".CharactersSelectedForInsert.", AsString(mode_)}),
        number_of_characters);
  }

  if (mode_ != EditorOpportunityMode::kRewrite) {
    return;
  }

  base::UmaHistogramCounts100000(
      "InputMethod.Manta.Orca.CharactersSelectedForInsert." +
          GetToneStringFromEnum(tone_),
      number_of_characters);
}

void EditorMetricsRecorder::LogNumberOfResponsesFromServer(
    int number_of_responses) {
  if (mode_ == EditorOpportunityMode::kInvalidInput ||
      mode_ == EditorOpportunityMode::kNotAllowedForUse) {
    return;
  }

  base::UmaHistogramExactLinear(
      base::StrCat({"InputMethod.Manta.Orca.NumResponses.", AsString(mode_)}),
      number_of_responses, kMaxNumResponsesFromServer);

  if (IsInternationalizedPathEnabled()) {
    base::UmaHistogramExactLinear(
        base::StrCat({"InputMethod.Manta.Orca.",
                      AsEnglishOrOther(InputMethodToLanguageCategory(
                          context_->active_engine_id())),
                      ".NumResponses.", AsString(mode_)}),
        number_of_responses, kMaxNumResponsesFromServer);
  }

  if (mode_ != EditorOpportunityMode::kRewrite) {
    return;
  }

  base::UmaHistogramExactLinear(
      base::StrCat({"InputMethod.Manta.Orca.NumResponses.",
                    GetToneStringFromEnum(tone_)}),
      number_of_responses, kMaxNumResponsesFromServer);
}

void EditorMetricsRecorder::LogLengthOfLongestResponseFromServer(
    int number_of_characters) {
  if (mode_ == EditorOpportunityMode::kInvalidInput ||
      mode_ == EditorOpportunityMode::kNotAllowedForUse) {
    return;
  }

  base::UmaHistogramCounts100000(
      base::StrCat(
          {"InputMethod.Manta.Orca.LengthOfLongestResponse.", AsString(mode_)}),
      number_of_characters);

  if (IsInternationalizedPathEnabled()) {
    base::UmaHistogramCounts100000(
        base::StrCat({"InputMethod.Manta.Orca.",
                      AsEnglishOrOther(InputMethodToLanguageCategory(
                          context_->active_engine_id())),
                      ".LengthOfLongestResponse.", AsString(mode_)}),
        number_of_characters);
  }

  if (mode_ != EditorOpportunityMode::kRewrite) {
    return;
  }

  base::UmaHistogramCounts100000(
      base::StrCat({"InputMethod.Manta.Orca.LengthOfLongestResponse.",
                    GetToneStringFromEnum(tone_)}),
      number_of_characters);
}

void EditorMetricsRecorder::LogEditorCriticalState(
    const EditorCriticalStates& critical_state) {
  if (std::optional<ukm::SourceId> source_id = context_->GetUkmSourceId();
      source_id.has_value()) {
    ukm::builders::InputMethod_Manta_Orca(*source_id)
        .SetEditorCriticalStates(static_cast<int>(critical_state))
        .Record(ukm::UkmRecorder::Get());
  }
}

}  // namespace ash::input_method
