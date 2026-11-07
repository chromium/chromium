// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/model_execution/on_device_model_ui_metadata.h"

#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace optimization_guide {

namespace {

std::u16string GetCategoryTitle(OnDeviceModelCategory category) {
  switch (category) {
    case OnDeviceModelCategory::kGeneralPurpose:
      return l10n_util::GetStringUTF16(
          IDS_ON_DEVICE_MODEL_CATEGORY_GENERAL_PURPOSE_TITLE);
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
    case OnDeviceModelCategory::kLiveTranscription:
      return l10n_util::GetStringUTF16(
          IDS_ON_DEVICE_MODEL_CATEGORY_LIVE_TRANSCRIPTION_DESCRIPTION);
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
    case OnDeviceBaseModel::kSpeechRecognitionSmallExpert:
      return u"Live Caption and Transcription";
    case OnDeviceBaseModel::kUnknown:
    case OnDeviceBaseModel::kXxs:
    case OnDeviceBaseModel::kXs:
    case OnDeviceBaseModel::kV2Nano:
    case OnDeviceBaseModel::kV3Nano:
    case OnDeviceBaseModel::kClassifier:
    case OnDeviceBaseModel::kLanguageDetection:
    case OnDeviceBaseModel::kProofreaderSmallExpert:
    case OnDeviceBaseModel::kSummarizerSmallExpert:
    case OnDeviceBaseModel::kLayaEnWint8:
    case OnDeviceBaseModel::kLayaMlWint8:
      return std::u16string();
  }
}

}  // namespace

std::optional<OnDeviceModelUiMetadata> GetOnDeviceModelUiMetadata(
    OnDeviceBaseModel base_model) {
  std::optional<OnDeviceModelCategory> category =
      GetOnDeviceModelCategory(base_model);
  if (!category) {
    return std::nullopt;
  }
  return OnDeviceModelUiMetadata{
      .category = *category,
      .category_title = GetCategoryTitle(*category),
      .category_description = GetCategoryDescription(*category),
      .model_display_name = GetBaseModelDisplayName(base_model),
  };
}

}  // namespace optimization_guide
