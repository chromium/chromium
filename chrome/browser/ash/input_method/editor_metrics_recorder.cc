// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_metrics_enums.h"
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"

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
  return EditorTone::kUnknown;
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
    case EditorBlockedReason::kBlockedByManagedStatus:
      return EditorStates::kBlockedByManagedStatus;
  }
}

EditorStates ToEditorStatesMetric(orca::mojom::TextQueryErrorCode error_code) {
  switch (error_code) {
    case orca::mojom::TextQueryErrorCode::kUnknown:
      return EditorStates::ErrorUnknown;
    case orca::mojom::TextQueryErrorCode::kInvalidArgument:
      return EditorStates::ErrorInvalidArgument;
    case orca::mojom::TextQueryErrorCode::kResourceExhausted:
      return EditorStates::ErrorResourceExhausted;
    case orca::mojom::TextQueryErrorCode::kBackendFailure:
      return EditorStates::ErrorBackendFailure;
    case orca::mojom::TextQueryErrorCode::kNoInternetConnection:
      return EditorStates::ErrorNoInternetConnection;
    case orca::mojom::TextQueryErrorCode::kUnsupportedLanguage:
      return EditorStates::ErrorUnsupportedLanguage;
    case orca::mojom::TextQueryErrorCode::kBlockedOutputs:
      return EditorStates::ErrorBlockedOutputs;
    case orca::mojom::TextQueryErrorCode::kRestrictedRegion:
      return EditorStates::ErrorRestrictedRegion;
  }
}

EditorMetricsRecorder::EditorMetricsRecorder(EditorOpportunityMode mode)
    : mode_(mode) {}

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
  std::string histogram_name;
  switch (mode_) {
    case EditorOpportunityMode::kWrite:
      histogram_name = "InputMethod.Manta.Orca.States.Write";
      break;
    case EditorOpportunityMode::kRewrite:
      histogram_name = "InputMethod.Manta.Orca.States.Rewrite";
      break;
    case EditorOpportunityMode::kNone:
      return;
  }

  base::UmaHistogramEnumeration(histogram_name, state);
  if (mode_ != EditorOpportunityMode::kRewrite) {
    return;
  }

  base::UmaHistogramEnumeration(base::StrCat({"InputMethod.Manta.Orca.States.",
                                              GetToneStringFromEnum(tone_)}),
                                state);
}

void EditorMetricsRecorder::LogNumberOfCharactersInserted(
    int number_of_characters) {
  std::string histogram_name;
  switch (mode_) {
    case EditorOpportunityMode::kWrite:
      histogram_name = "InputMethod.Manta.Orca.CharactersInserted.Write";
      break;
    case EditorOpportunityMode::kRewrite:
      histogram_name = "InputMethod.Manta.Orca.CharactersInserted.Rewrite";
      break;
    case EditorOpportunityMode::kNone:
      return;
  }

  base::UmaHistogramCounts100000(histogram_name, number_of_characters);
  if (mode_ != EditorOpportunityMode::kRewrite) {
    return;
  }

  base::UmaHistogramCounts100000("InputMethod.Manta.Orca.CharactersInserted." +
                                     GetToneStringFromEnum(tone_),
                                 number_of_characters);
}

void EditorMetricsRecorder::LogNumberOfCharactersSelectedForInsert(
    int number_of_characters) {
  std::string histogram_name;
  switch (mode_) {
    case EditorOpportunityMode::kWrite:
      histogram_name =
          "InputMethod.Manta.Orca.CharactersSelectedForInsert.Write";
      break;
    case EditorOpportunityMode::kRewrite:
      histogram_name =
          "InputMethod.Manta.Orca.CharactersSelectedForInsert.Rewrite";
      break;
    case EditorOpportunityMode::kNone:
      return;
  }

  base::UmaHistogramCounts100000(histogram_name, number_of_characters);
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
  switch (mode_) {
    case EditorOpportunityMode::kWrite:
      base::UmaHistogramExactLinear("InputMethod.Manta.Orca.NumResponses.Write",
                                    number_of_responses,
                                    kMaxNumResponsesFromServer);
      return;
    case EditorOpportunityMode::kRewrite:
      base::UmaHistogramExactLinear(
          "InputMethod.Manta.Orca.NumResponses.Rewrite", number_of_responses,
          kMaxNumResponsesFromServer);
      base::UmaHistogramExactLinear(
          base::StrCat({"InputMethod.Manta.Orca.NumResponses.",
                        GetToneStringFromEnum(tone_)}),
          number_of_responses, kMaxNumResponsesFromServer);
      return;
    case EditorOpportunityMode::kNone:
      return;
  }
}

void EditorMetricsRecorder::LogLengthOfLongestResponseFromServer(
    int number_of_characters) {
  switch (mode_) {
    case EditorOpportunityMode::kWrite:
      base::UmaHistogramCounts100000(
          "InputMethod.Manta.Orca.LengthOfLongestResponse.Write",
          number_of_characters);
      return;
    case EditorOpportunityMode::kRewrite:
      base::UmaHistogramCounts100000(
          "InputMethod.Manta.Orca.LengthOfLongestResponse.Rewrite",
          number_of_characters);
      base::UmaHistogramCounts100000(
          base::StrCat({"InputMethod.Manta.Orca.LengthOfLongestResponse.",
                        GetToneStringFromEnum(tone_)}),
          number_of_characters);
      return;
    case EditorOpportunityMode::kNone:
      return;
  }
}

}  // namespace ash::input_method
