// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/model_execution/on_device_model_ui_metadata.h"

#include "components/optimization_guide/core/model_execution/on_device_model_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

TEST(OnDeviceModelUiMetadataTest, GeneralPurposeModels) {
  auto gemma4 = GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kGemma4);
  ASSERT_TRUE(gemma4.has_value());
  EXPECT_EQ(gemma4->category, OnDeviceModelCategory::kGeneralPurpose);
  EXPECT_EQ(gemma4->category_title, u"General purpose");
  EXPECT_EQ(gemma4->category_description,
            u"Helps to write, summarize, and more");
  EXPECT_EQ(gemma4->model_display_name, u"Gemma 4");

  auto nano_gpu = GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kV3NanoGpu);
  ASSERT_TRUE(nano_gpu.has_value());
  EXPECT_EQ(nano_gpu->category, OnDeviceModelCategory::kGeneralPurpose);
  EXPECT_EQ(nano_gpu->category_title, u"General purpose");
  EXPECT_EQ(nano_gpu->category_description,
            u"Helps to write, summarize, and more");
  EXPECT_EQ(nano_gpu->model_display_name, u"Gemini Nano");

  auto nano_cpu = GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kV3NanoCpu);
  ASSERT_TRUE(nano_cpu.has_value());
  EXPECT_EQ(nano_cpu->category, OnDeviceModelCategory::kGeneralPurpose);
  EXPECT_EQ(nano_cpu->category_title, u"General purpose");
  EXPECT_EQ(nano_cpu->category_description,
            u"Helps to write, summarize, and more");
  EXPECT_EQ(nano_cpu->model_display_name, u"Gemini Nano");
}

TEST(OnDeviceModelUiMetadataTest, LiveTranscriptionModel) {
  auto speech = GetOnDeviceModelUiMetadata(
      OnDeviceBaseModel::kSpeechRecognitionSmallExpert);
  ASSERT_TRUE(speech.has_value());
  EXPECT_EQ(speech->category, OnDeviceModelCategory::kLiveTranscription);
  EXPECT_EQ(speech->category_title, u"Live transcription");
  EXPECT_EQ(speech->category_description,
            u"Transcribes speech to text in real time");
  EXPECT_EQ(speech->model_display_name, u"Live Caption and Transcription");
}

TEST(OnDeviceModelUiMetadataTest, HiddenModelsReturnNullopt) {
  EXPECT_FALSE(
      GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kUnknown).has_value());
  EXPECT_FALSE(GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kXxs).has_value());
  EXPECT_FALSE(GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kXs).has_value());
  EXPECT_FALSE(
      GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kV2Nano).has_value());
  EXPECT_FALSE(
      GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kV3Nano).has_value());
  EXPECT_FALSE(
      GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kClassifier).has_value());
  EXPECT_FALSE(GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kLanguageDetection)
                   .has_value());
  EXPECT_FALSE(
      GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kProofreaderSmallExpert)
          .has_value());
  EXPECT_FALSE(
      GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kSummarizerSmallExpert)
          .has_value());
  EXPECT_FALSE(
      GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kLayaEnWint8).has_value());
  EXPECT_FALSE(
      GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kLayaMlWint8).has_value());
}

}  // namespace
}  // namespace optimization_guide
