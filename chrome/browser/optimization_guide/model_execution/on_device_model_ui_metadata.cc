// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/model_execution/on_device_model_ui_metadata.h"

#include <string_view>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/ai/features.h"
#include "chrome/grit/generated_resources.h"
#include "media/base/media_switches.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/base/l10n/l10n_util.h"

namespace optimization_guide {

namespace {

bool IsGemma4VersionEnabled(std::string_view expected_version) {
  return base::FeatureList::IsEnabled(kAIApiFoundationalModel) &&
         base::GetFieldTrialParamValueByFeature(
             kAIApiFoundationalModel, "model_version") == expected_version;
}

bool IsModelEnabled(OnDeviceBaseModel base_model,
                    OnDeviceModelCategory category) {
  if (base_model == OnDeviceBaseModel::kGemma4) {
    return IsGemma4VersionEnabled("v4");
  }

  switch (category) {
    case OnDeviceModelCategory::kGeneralPurpose:
      return true;
    case OnDeviceModelCategory::kProofreading:
      // kAIProofreadingAPI is FEATURE_ENABLED_BY_DEFAULT with
      // copied_from_base_feature_if: "overridden" for Origin Trial support, so
      // check for an explicit override (e.g. chrome://flags or Finch).
      return base::FeatureList::GetStateIfOverridden(
                 blink::features::kAIProofreadingAPI) == true;
    case OnDeviceModelCategory::kSummarizing:
      return base::FeatureList::IsEnabled(
                 blink::features::kAISummarizationAPI) &&
             base::FeatureList::IsEnabled(
                 blink::features::kAISummarizationPerformancePreference);
    case OnDeviceModelCategory::kLiveTranscription:
      return base::FeatureList::IsEnabled(
          media::kOnDeviceWebSpeechSmallExpertModel);
  }
}

std::u16string GetCategoryTitle(OnDeviceModelCategory category) {
  switch (category) {
    case OnDeviceModelCategory::kGeneralPurpose:
      return l10n_util::GetStringUTF16(
          IDS_ON_DEVICE_MODEL_CATEGORY_GENERAL_PURPOSE_TITLE);
    case OnDeviceModelCategory::kProofreading:
      return l10n_util::GetStringUTF16(
          IDS_ON_DEVICE_MODEL_CATEGORY_PROOFREADING_TITLE);
    case OnDeviceModelCategory::kSummarizing:
      return l10n_util::GetStringUTF16(
          IDS_ON_DEVICE_MODEL_CATEGORY_SUMMARIZING_TITLE);
    case OnDeviceModelCategory::kLiveTranscription:
      return l10n_util::GetStringUTF16(
          IDS_ON_DEVICE_MODEL_CATEGORY_LIVE_TRANSCRIPTION_TITLE);
  }
}

std::u16string GetCategoryDescription(OnDeviceModelCategory category) {
  switch (category) {
    case OnDeviceModelCategory::kGeneralPurpose:
      return l10n_util::GetStringUTF16(
          IDS_ON_DEVICE_MODEL_CATEGORY_GENERAL_PURPOSE_DESCRIPTION);
    case OnDeviceModelCategory::kProofreading:
      return l10n_util::GetStringUTF16(
          IDS_ON_DEVICE_MODEL_CATEGORY_PROOFREADING_DESCRIPTION);
    case OnDeviceModelCategory::kSummarizing:
      return l10n_util::GetStringUTF16(
          IDS_ON_DEVICE_MODEL_CATEGORY_SUMMARIZING_DESCRIPTION);
    case OnDeviceModelCategory::kLiveTranscription:
      return l10n_util::GetStringUTF16(
          IDS_ON_DEVICE_MODEL_CATEGORY_LIVE_TRANSCRIPTION_DESCRIPTION);
  }
}

std::vector<std::u16string> GetCategoryDetails(OnDeviceModelCategory category) {
  switch (category) {
    case OnDeviceModelCategory::kGeneralPurpose:
      return {
          l10n_util::GetStringUTF16(
              IDS_ON_DEVICE_MODEL_CATEGORY_GENERAL_PURPOSE_DETAIL_1),
          l10n_util::GetStringUTF16(
              IDS_ON_DEVICE_MODEL_CATEGORY_GENERAL_PURPOSE_DETAIL_2),
      };
    case OnDeviceModelCategory::kProofreading:
      return {
          l10n_util::GetStringUTF16(
              IDS_ON_DEVICE_MODEL_CATEGORY_PROOFREADING_DETAIL_1),
          l10n_util::GetStringUTF16(
              IDS_ON_DEVICE_MODEL_CATEGORY_PROOFREADING_DETAIL_2),
      };
    case OnDeviceModelCategory::kSummarizing:
      return {
          l10n_util::GetStringUTF16(
              IDS_ON_DEVICE_MODEL_CATEGORY_SUMMARIZING_DETAIL_1),
          l10n_util::GetStringUTF16(
              IDS_ON_DEVICE_MODEL_CATEGORY_SUMMARIZING_DETAIL_2),
      };
    case OnDeviceModelCategory::kLiveTranscription:
      return {
          l10n_util::GetStringUTF16(
              IDS_ON_DEVICE_MODEL_CATEGORY_LIVE_TRANSCRIPTION_DETAIL_1),
          l10n_util::GetStringUTF16(
              IDS_ON_DEVICE_MODEL_CATEGORY_LIVE_TRANSCRIPTION_DETAIL_2),
      };
  }
}

// TODO(crbug.com/582309438): Move model display names into the
// manifest as explicit data.
std::u16string GetBaseModelDisplayName(OnDeviceBaseModel base_model) {
  switch (base_model) {
    case OnDeviceBaseModel::kGemma4:
      return u"Gemma 4";
    case OnDeviceBaseModel::kV3NanoGpu:
    case OnDeviceBaseModel::kV3NanoCpu:
      return u"Gemini Nano";
    case OnDeviceBaseModel::kProofreaderSmallExpert:
    case OnDeviceBaseModel::kSummarizerSmallExpert:
      return u"Gemma 197M";
    case OnDeviceBaseModel::kSpeechRecognitionSmallExpert:
      return u"Gemma 500M";
    case OnDeviceBaseModel::kUnknown:
    case OnDeviceBaseModel::kClassifier:
    case OnDeviceBaseModel::kLanguageDetection:
    case OnDeviceBaseModel::kLayaEnWint8:
    case OnDeviceBaseModel::kLayaMlWint8:
      return std::u16string();
  }
}

}  // namespace

OnDeviceModelUiMetadata::OnDeviceModelUiMetadata() = default;

OnDeviceModelUiMetadata::OnDeviceModelUiMetadata(
    OnDeviceModelCategory category,
    std::u16string category_title,
    std::u16string category_description,
    std::u16string model_display_name,
    std::vector<std::u16string> details)
    : category(category),
      category_title(std::move(category_title)),
      category_description(std::move(category_description)),
      model_display_name(std::move(model_display_name)),
      details(std::move(details)) {}

OnDeviceModelUiMetadata::OnDeviceModelUiMetadata(
    const OnDeviceModelUiMetadata&) = default;
OnDeviceModelUiMetadata& OnDeviceModelUiMetadata::operator=(
    const OnDeviceModelUiMetadata&) = default;
OnDeviceModelUiMetadata::OnDeviceModelUiMetadata(OnDeviceModelUiMetadata&&) =
    default;
OnDeviceModelUiMetadata& OnDeviceModelUiMetadata::operator=(
    OnDeviceModelUiMetadata&&) = default;
OnDeviceModelUiMetadata::~OnDeviceModelUiMetadata() = default;

std::optional<OnDeviceModelUiMetadata> GetOnDeviceModelUiMetadata(
    OnDeviceBaseModel base_model) {
  std::optional<OnDeviceModelCategory> category =
      GetOnDeviceModelCategory(base_model);
  if (!category || !IsModelEnabled(base_model, *category)) {
    return std::nullopt;
  }
  return OnDeviceModelUiMetadata(
      *category, GetCategoryTitle(*category), GetCategoryDescription(*category),
      GetBaseModelDisplayName(base_model), GetCategoryDetails(*category));
}

}  // namespace optimization_guide
