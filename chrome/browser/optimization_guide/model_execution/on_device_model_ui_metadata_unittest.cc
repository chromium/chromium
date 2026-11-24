// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/model_execution/on_device_model_ui_metadata.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ai/features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_names.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

namespace optimization_guide {
namespace {

using ::testing::ElementsAre;

TEST(OnDeviceModelUiMetadataTest, GeneralPurposeModels) {
  // By default, kAIApiFoundationalModel is disabled, so kGemma4 is hidden
  // while Nano V3 is visible.
  EXPECT_FALSE(
      GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kGemma4).has_value());

  {
    // Enabling kAIApiFoundationalModel without `model_version=v4` keeps
    // kGemma4 hidden (consistent with AIManager falling back to Nano V3).
    base::test::ScopedFeatureList without_version;
    without_version.InitAndEnableFeature(kAIApiFoundationalModel);
    EXPECT_FALSE(
        GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kGemma4).has_value());
  }

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(kAIApiFoundationalModel,
                                                  {{"model_version", "v4"}});

  auto gemma4 = GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kGemma4);
  ASSERT_TRUE(gemma4.has_value());
  EXPECT_EQ(gemma4->category, OnDeviceModelCategory::kGeneralPurpose);
  EXPECT_EQ(gemma4->category_title, u"General-purpose writing");
  EXPECT_EQ(gemma4->category_description,
            u"Helps write, rewrite, and summarize text");
  EXPECT_EQ(gemma4->model_display_name, u"Gemma 4");
  EXPECT_THAT(
      gemma4->details,
      ElementsAre(
          u"Helps draft, rephrase, and organize text on sites that use it",
          u"Handles many tasks, so one download covers multiple features"));

  auto nano_gpu = GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kV3NanoGpu);
  ASSERT_TRUE(nano_gpu.has_value());
  EXPECT_EQ(nano_gpu->category, OnDeviceModelCategory::kGeneralPurpose);
  EXPECT_EQ(nano_gpu->category_title, u"General-purpose writing");
  EXPECT_EQ(nano_gpu->category_description,
            u"Helps write, rewrite, and summarize text");
  EXPECT_EQ(nano_gpu->model_display_name, u"Gemini Nano");
  EXPECT_THAT(
      nano_gpu->details,
      ElementsAre(
          u"Helps draft, rephrase, and organize text on sites that use it",
          u"Handles many tasks, so one download covers multiple features"));

  auto nano_cpu = GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kV3NanoCpu);
  ASSERT_TRUE(nano_cpu.has_value());
  EXPECT_EQ(nano_cpu->category, OnDeviceModelCategory::kGeneralPurpose);
  EXPECT_EQ(nano_cpu->category_title, u"General-purpose writing");
  EXPECT_EQ(nano_cpu->category_description,
            u"Helps write, rewrite, and summarize text");
  EXPECT_EQ(nano_cpu->model_display_name, u"Gemini Nano");
  EXPECT_THAT(
      nano_cpu->details,
      ElementsAre(
          u"Helps draft, rephrase, and organize text on sites that use it",
          u"Handles many tasks, so one download covers multiple features"));
}

TEST(OnDeviceModelUiMetadataTest, LiveTranscriptionModel) {
  EXPECT_FALSE(GetOnDeviceModelUiMetadata(
                   OnDeviceBaseModel::kSpeechRecognitionSmallExpert)
                   .has_value());

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kOnDeviceWebSpeechSmallExpertModel);

  auto speech = GetOnDeviceModelUiMetadata(
      OnDeviceBaseModel::kSpeechRecognitionSmallExpert);
  ASSERT_TRUE(speech.has_value());
  EXPECT_EQ(speech->category, OnDeviceModelCategory::kLiveTranscription);
  EXPECT_EQ(speech->category_title, u"Speech recognition");
  EXPECT_EQ(speech->category_description,
            u"Turns speech into text, like captions");
  EXPECT_EQ(speech->model_display_name, u"Gemma 500M");
  EXPECT_THAT(
      speech->details,
      ElementsAre(u"Works on sites that support live audio transcription",
                  u"Downloads once for all languages you use"));
}

TEST(OnDeviceModelUiMetadataTest, ProofreadingAndSummarizingModels) {
  // kAIProofreadingAPI is enabled by default, so the proofreader is visible
  // without a flag. The summarizer is not.
  EXPECT_TRUE(
      GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kProofreaderSmallExpert)
          .has_value());
  EXPECT_FALSE(
      GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kSummarizerSmallExpert)
          .has_value());

  {
    // Disabling kAIProofreadingAPI (e.g. by a kill switch) hides the
    // proofreader.
    base::test::ScopedFeatureList disabled;
    disabled.InitAndDisableFeature(blink::features::kAIProofreadingAPI);
    EXPECT_FALSE(
        GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kProofreaderSmallExpert)
            .has_value());
  }

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {blink::features::kAISummarizationAPI,
       blink::features::kAISummarizationPerformancePreference},
      {});

  auto proofreader =
      GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kProofreaderSmallExpert);
  ASSERT_TRUE(proofreader.has_value());
  EXPECT_EQ(proofreader->category, OnDeviceModelCategory::kProofreading);
  EXPECT_EQ(proofreader->category_title, u"Proofreading");
  EXPECT_EQ(proofreader->category_description, u"Checks spelling and grammar");
  EXPECT_EQ(proofreader->model_display_name, u"Gemma 197M");
  EXPECT_THAT(
      proofreader->details,
      ElementsAre(
          u"Suggests fixes for spelling, grammar, and punctuation as you type",
          u"Only suggests fixes without rewriting your text"));

  auto summarizer =
      GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kSummarizerSmallExpert);
  ASSERT_TRUE(summarizer.has_value());
  EXPECT_EQ(summarizer->category, OnDeviceModelCategory::kSummarizing);
  EXPECT_EQ(summarizer->category_title, u"Summarizing");
  EXPECT_EQ(summarizer->category_description,
            u"Turns long pages of text into key points");
  EXPECT_EQ(summarizer->model_display_name, u"Gemma 197M");
  EXPECT_THAT(
      summarizer->details,
      ElementsAre(u"Gives you key points, a short summary, or a headline",
                  u"Faster and more compact than general-purpose writing"));
}

TEST(OnDeviceModelUiMetadataTest, HiddenModelsReturnNullopt) {
  EXPECT_FALSE(
      GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kUnknown).has_value());
  EXPECT_FALSE(
      GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kClassifier).has_value());
  EXPECT_FALSE(GetOnDeviceModelUiMetadata(OnDeviceBaseModel::kLanguageDetection)
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
