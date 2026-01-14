// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_manager.h"

#include <memory>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/task/current_thread.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ai/ai_language_model.h"
#include "chrome/browser/ai/ai_test_utils.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"
#include "components/optimization_guide/core/model_execution/test/mock_on_device_capability.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_rewriter.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_summarizer.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_writer.mojom.h"

using optimization_guide::MockSession;
using testing::_;
using testing::AtMost;
using testing::NiceMock;

namespace {

std::vector<blink::mojom::AILanguageCodePtr> MakeLanguageCodeVector(
    const std::vector<std::string>& languages) {
  std::vector<blink::mojom::AILanguageCodePtr> result;
  for (const auto& language : languages) {
    result.push_back(blink::mojom::AILanguageCode::New(language));
  }
  return result;
}

class AIManagerTest : public AITestUtils::AITestBase {
 protected:
  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig CreateConfig()
      override {
    optimization_guide::proto::OnDeviceModelExecutionFeatureConfig config;
    config.set_can_skip_text_safety(true);
    config.set_feature(optimization_guide::proto::ModelExecutionFeature::
                           MODEL_EXECUTION_FEATURE_PROMPT_API);
    return config;
  }

  void SetupMockOptimizationGuideKeyedService() override {
    AITestUtils::AITestBase::SetupMockOptimizationGuideKeyedService();
    ON_CALL(*mock_optimization_guide_keyed_service_, GetOnDeviceCapabilities())
        .WillByDefault(testing::Return(on_device_model::Capabilities()));
  }

  void SetBuildInAIAPIsEnterprisePolicy(bool value) {
    profile()->GetPrefs()->SetBoolean(
        policy::policy_prefs::kBuiltInAIAPIsEnabled, value);
  }
};

// Tests that involve invalid on-device model file paths should not crash when
// the associated RFH is destroyed.
TEST_F(AIManagerTest, NoUAFWithInvalidOnDeviceModelPath) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(
      optimization_guide::switches::kOnDeviceModelExecutionOverride,
      "invalid-on-device-model-file-path");

  base::MockCallback<blink::mojom::AIManager::CanCreateLanguageModelCallback>
      callback;
  EXPECT_CALL(callback, Run(_)).Times(AtMost(1));
  ai_manager_->CanCreateLanguageModel(/*options=*/{}, callback.Get());

  // The callback may still be pending, delete the WebContents and destroy the
  // associated RFH, which should not result in a UAF.
  DeleteContents();

  task_environment()->RunUntilIdle();
}

TEST_F(AIManagerTest, CanCreate) {
  base::MockCallback<
      base::OnceCallback<void(blink::mojom::ModelAvailabilityCheckResult)>>
      callback;
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::kAvailable))
      .Times(4);

  ai_manager_->CanCreateLanguageModel(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateWriter(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateSummarizer(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateRewriter(/*options=*/{}, callback.Get());
}

TEST_F(AIManagerTest, CanCreateNotEnabled) {
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              GetOnDeviceModelEligibilityAsync(_, _, _))
      .Times(4)
      .WillRepeatedly([](auto feature, auto capabilities, auto callback) {
        std::move(callback).Run(
            optimization_guide::OnDeviceModelEligibilityReason::
                kFeatureNotEnabled);
      });
  base::MockCallback<
      base::OnceCallback<void(blink::mojom::ModelAvailabilityCheckResult)>>
      callback;
  EXPECT_CALL(callback, Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableFeatureNotEnabled))
      .Times(4);

  ai_manager_->CanCreateLanguageModel(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateWriter(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateSummarizer(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateRewriter(/*options=*/{}, callback.Get());
}

TEST_F(AIManagerTest, CanCreateSessionWithTextInputCapabilities) {
  base::MockCallback<blink::mojom::AIManager::CanCreateLanguageModelCallback>
      callback;
  optimization_guide::mojom::OnDeviceFeature feature =
      optimization_guide::mojom::OnDeviceFeature::kPromptApi;
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::kAvailable))
      .Times(1);
  EXPECT_CALL(callback, Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableModelAdaptationNotAvailable))
      .Times(2);
  on_device_model::Capabilities capabilities;
  ai_manager_->CanCreateSession(feature, capabilities, callback.Get());
  capabilities.Put(on_device_model::CapabilityFlags::kImageInput);
  ai_manager_->CanCreateSession(feature, capabilities, callback.Get());
  capabilities.Clear();
  capabilities.Put(on_device_model::CapabilityFlags::kAudioInput);
  ai_manager_->CanCreateSession(feature, capabilities, callback.Get());
}

TEST_F(AIManagerTest, CanCreateSessionWithImageAndAudioInputCapabilities) {
  base::test::ScopedFeatureList scoped_feature_list(
      blink::features::kAIPromptAPIMultimodalInput);
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              GetOnDeviceCapabilities())
      .Times(2)
      .WillRepeatedly(testing::Return(on_device_model::Capabilities(
          {on_device_model::CapabilityFlags::kImageInput,
           on_device_model::CapabilityFlags::kAudioInput})));
  base::MockCallback<blink::mojom::AIManager::CanCreateLanguageModelCallback>
      callback;
  optimization_guide::mojom::OnDeviceFeature feature =
      optimization_guide::mojom::OnDeviceFeature::kPromptApi;
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::kAvailable))
      .Times(3);
  on_device_model::Capabilities capabilities;
  ai_manager_->CanCreateSession(feature, capabilities, callback.Get());
  capabilities.Put(on_device_model::CapabilityFlags::kImageInput);
  ai_manager_->CanCreateSession(feature, capabilities, callback.Get());
  capabilities.Put(on_device_model::CapabilityFlags::kAudioInput);
  ai_manager_->CanCreateSession(feature, capabilities, callback.Get());
}

TEST_F(AIManagerTest, CanCreateEnterprisePolicyDisabled) {
  SetBuildInAIAPIsEnterprisePolicy(false);
  base::MockCallback<
      base::OnceCallback<void(blink::mojom::ModelAvailabilityCheckResult)>>
      callback;
  EXPECT_CALL(callback, Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableEnterprisePolicyDisabled))
      .Times(4);

  ai_manager_->CanCreateLanguageModel(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateWriter(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateSummarizer(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateRewriter(/*options=*/{}, callback.Get());
  SetBuildInAIAPIsEnterprisePolicy(true);
}

// Test CheckAndFixLanguages templates for LanguageModel.
TEST_F(AIManagerTest, CheckAndFixLanguagesLanguageModel) {
  base::flat_set<std::string_view> supported = {"en", "es", "ja"};
  auto make_expected = [](const base::flat_set<std::string>& languages) {
    auto expected = blink::mojom::AILanguageModelExpected::New();
    expected->languages.emplace();
    for (const auto& language : languages) {
      expected->languages->push_back(
          blink::mojom::AILanguageCode::New(language));
    }
    return expected;
  };

  auto make_options = [&](const base::flat_set<std::string>& inputs,
                          const base::flat_set<std::string>& outputs) {
    auto options = blink::mojom::AILanguageModelCreateOptions::New();
    options->expected_inputs.emplace();
    options->expected_inputs->push_back(make_expected(inputs));
    options->expected_outputs.emplace();
    options->expected_outputs->push_back(make_expected(outputs));
    return options;
  };

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  EXPECT_TRUE(ai_manager_->CheckAndFixLanguages(options, "API", supported));
  options = make_options({"en", "es-MX"}, {});
  EXPECT_TRUE(ai_manager_->CheckAndFixLanguages(options, "API", supported));
  options = make_options({}, {"en-UK", "es-SP", "ja-JP"});
  EXPECT_TRUE(ai_manager_->CheckAndFixLanguages(options, "API", supported));
  options = make_options({"en", "fr"}, {});
  EXPECT_FALSE(ai_manager_->CheckAndFixLanguages(options, "API", supported));
  options = make_options({"en"}, {"hi"});
  EXPECT_FALSE(ai_manager_->CheckAndFixLanguages(options, "API", supported));
}

// Test CheckAndFixLanguages templates for Summarizer, Writer, and Rewriter.
TEST_F(AIManagerTest, CheckAndFixLanguagesWritingAssistance) {
  base::flat_set<std::string_view> supported = {"en", "es", "ja"};
  auto make_options = [](const std::vector<std::string>& input,
                         const std::vector<std::string>& context,
                         const std::string& output) {
    auto options = blink::mojom::AISummarizerCreateOptions::New();
    options->expected_input_languages = MakeLanguageCodeVector(input);
    options->expected_context_languages = MakeLanguageCodeVector(context);
    options->output_language = blink::mojom::AILanguageCode::New(output);
    return options;
  };

  auto options = blink::mojom::AISummarizerCreateOptions::New();
  EXPECT_TRUE(ai_manager_->CheckAndFixLanguages(options, "API", supported));
  options = make_options({}, {}, "");
  EXPECT_TRUE(ai_manager_->CheckAndFixLanguages(options, "API", supported));
  EXPECT_TRUE(options->output_language->code.empty());
  options = make_options({"en", "es-MX"}, {"ja"}, "en-US");
  EXPECT_TRUE(ai_manager_->CheckAndFixLanguages(options, "API", supported));
  options = make_options({"en-UK", "en-US"}, {"en"}, "");
  EXPECT_TRUE(ai_manager_->CheckAndFixLanguages(options, "API", supported));
  EXPECT_EQ(options->output_language->code, "en-UK");
  options = make_options({"en", "fr"}, {}, "hi");
  EXPECT_FALSE(ai_manager_->CheckAndFixLanguages(options, "API", supported));
}

// Test CheckAndFixLanguages templates for Proofreader.
TEST_F(AIManagerTest, CheckAndFixLanguagesProofreader) {
  base::flat_set<std::string_view> supported = {"en", "es", "ja"};
  auto make_options = [](const std::vector<std::string>& input,
                         const std::string& correction_explanation) {
    auto options = blink::mojom::AIProofreaderCreateOptions::New();
    options->expected_input_languages = MakeLanguageCodeVector(input);
    options->correction_explanation_language =
        blink::mojom::AILanguageCode::New(correction_explanation);
    return options;
  };

  auto options = blink::mojom::AIProofreaderCreateOptions::New();
  EXPECT_TRUE(ai_manager_->CheckAndFixLanguages(options, "API", supported));
  options = make_options({}, "");
  EXPECT_TRUE(ai_manager_->CheckAndFixLanguages(options, "API", supported));
  EXPECT_TRUE(options->correction_explanation_language->code.empty());
  options = make_options({"en", "es-MX", "ja"}, "en-US");
  EXPECT_TRUE(ai_manager_->CheckAndFixLanguages(options, "API", supported));
  options = make_options({"en-UK", "en-US", "en"}, "");
  EXPECT_TRUE(ai_manager_->CheckAndFixLanguages(options, "API", supported));
  EXPECT_EQ(options->correction_explanation_language->code, "en-UK");
  options = make_options({"en", "fr"}, "hi");
  EXPECT_FALSE(ai_manager_->CheckAndFixLanguages(options, "API", supported));
}

// Test that GetLanguageModelParams returns null when sampling config is
// not available (model not downloaded yet).
TEST_F(AIManagerTest, GetLanguageModelParamsReturnsNullWhenNotAvailable) {
  ON_CALL(*mock_optimization_guide_keyed_service_,
          GetSamplingParamsConfig(_))
      .WillByDefault(testing::Return(std::nullopt));

  EXPECT_TRUE(ai_manager_->GetLanguageModelParams().is_null());
}

// Test that GetLanguageModelParams returns params when config is available
TEST_F(AIManagerTest, GetLanguageModelParamsReturnsValidParamsWhenAvailable) {
  optimization_guide::SamplingParamsConfig config{
      .default_top_k = 3,
      .default_temperature = 1.0f,
  };
  ON_CALL(*mock_optimization_guide_keyed_service_,
          GetSamplingParamsConfig(_))
      .WillByDefault(testing::Return(config));

  auto params = ai_manager_->GetLanguageModelParams();

  ASSERT_TRUE(params);
  ASSERT_TRUE(params->default_sampling_params);
  EXPECT_EQ(3u, params->default_sampling_params->top_k);
  EXPECT_FLOAT_EQ(1.0f, params->default_sampling_params->temperature);
}

}  // namespace
