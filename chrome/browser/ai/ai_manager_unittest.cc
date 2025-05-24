// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_manager.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/task/current_thread.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ai/ai_language_model.h"
#include "chrome/browser/ai/ai_test_utils.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_rewriter.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_summarizer.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_writer.mojom.h"

using optimization_guide::MockSession;
using testing::_;
using testing::AtMost;
using testing::Invoke;
using testing::NiceMock;

class AIManagerTest : public AITestUtils::AITestBase {
 protected:
  AIManagerTest()
      : fake_broker_(optimization_guide::FakeAdaptationAsset({
            .config =
                [] {
                  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig
                      config;
                  config.set_can_skip_text_safety(true);
                  config.set_feature(
                      optimization_guide::proto::ModelExecutionFeature::
                          MODEL_EXECUTION_FEATURE_PROMPT_API);
                  return config;
                }(),
        })) {}

  void SetUp() override {
    AITestUtils::AITestBase::SetUp();
    SetupMockOptimizationGuideKeyedService();
    ai_manager_ =
        std::make_unique<AIManager>(main_rfh()->GetBrowserContext(),
                                    &component_update_service_, main_rfh());
  }

  void TearDown() override {
    ai_manager_.reset();
    AITestUtils::AITestBase::TearDown();
  }

  void SetupMockOptimizationGuideKeyedService() override {
    AITestUtils::AITestBase::SetupMockOptimizationGuideKeyedService();

    ON_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
        .WillByDefault(
            [&] { return std::make_unique<NiceMock<MockSession>>(&session_); });
    ON_CALL(session_, GetTokenLimits())
        .WillByDefault(AITestUtils::GetFakeTokenLimits);
    ON_CALL(session_, GetExecutionInputSizeInTokens(_, _))
        .WillByDefault(
            [&](optimization_guide::MultimodalMessageReadView request_metadata,
                optimization_guide::OptimizationGuideModelSizeInTokenCallback
                    callback) {
              std::move(callback).Run(
                  blink::mojom::kWritingAssistanceMaxInputTokenSize);
            });
    ON_CALL(session_, GetOnDeviceFeatureMetadata())
        .WillByDefault(AITestUtils::GetFakeFeatureMetadata);
    ON_CALL(*mock_optimization_guide_keyed_service_, GetOnDeviceCapabilities())
        .WillByDefault(testing::Return(on_device_model::Capabilities()));
    ON_CALL(*mock_optimization_guide_keyed_service_,
            GetOnDeviceModelEligibility(_))
        .WillByDefault(testing::Return(
            optimization_guide::OnDeviceModelEligibilityReason::kSuccess));
    ON_CALL(*mock_optimization_guide_keyed_service_, CreateModelBrokerClient())
        .WillByDefault([&]() {
          return std::make_unique<optimization_guide::ModelBrokerClient>(
              fake_broker_.BindAndPassRemote(),
              optimization_guide::CreateSessionArgs(nullptr, {}));
        });
  }

  void SetBuildInAIAPIsEnterprisePolicy(bool value) {
    profile()->GetPrefs()->SetBoolean(
        policy::policy_prefs::kBuiltInAIAPIsEnabled, value);
  }

 private:
  testing::NiceMock<MockSession> session_;
  optimization_guide::FakeModelBroker fake_broker_;
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

// Tests the `AIUserDataSet`'s behavior of managing the lifetime of
// `AILanguageModel`s.
TEST_F(AIManagerTest, AIContextBoundObjectSet) {
  mojo::Remote<blink::mojom::AILanguageModel> mock_session;
  AITestUtils::MockCreateLanguageModelClient mock_create_language_model_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_language_model_client, OnResult(_, _))
      .WillOnce(testing::Invoke(
          [&](mojo::PendingRemote<blink::mojom::AILanguageModel> language_model,
              blink::mojom::AILanguageModelInstanceInfoPtr info) {
            EXPECT_TRUE(language_model);
            mock_session = mojo::Remote<blink::mojom::AILanguageModel>(
                std::move(language_model));
            run_loop.Quit();
          }));

  mojo::Remote<blink::mojom::AIManager> mock_remote = GetAIManagerRemote();
  // Initially the `AIContextBoundObjectSet` is empty.
  ASSERT_EQ(0u, GetAIManagerContextBoundObjectSetSize());

  // After creating one `AILanguageModel`, the `AIContextBoundObjectSet`
  // contains 1 element.
  mock_remote->CreateLanguageModel(
      mock_create_language_model_client.BindNewPipeAndPassRemote(),
      blink::mojom::AILanguageModelCreateOptions::New(
          /*sampling_params=*/nullptr,
          /*initial_prompts=*/
          std::vector<blink::mojom::AILanguageModelPromptPtr>(),
          /*expected_inputs=*/std::nullopt,
          /*expected_outputs=*/std::nullopt));
  run_loop.Run();
  ASSERT_EQ(1u, GetAIManagerContextBoundObjectSetSize());

  // After resetting the session, the size of `AIContextBoundObjectSet` becomes
  // empty again.
  mock_session.reset();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return GetAIManagerContextBoundObjectSetSize() == 0u; }));
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
  optimization_guide::ModelBasedCapabilityKey key =
      optimization_guide::ModelBasedCapabilityKey::kPromptApi;
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::kAvailable))
      .Times(1);
  EXPECT_CALL(callback, Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableModelAdaptationNotAvailable))
      .Times(2);
  on_device_model::Capabilities capabilities;
  ai_manager_->CanCreateSession(key, capabilities, callback.Get());
  capabilities.Put(on_device_model::CapabilityFlags::kImageInput);
  ai_manager_->CanCreateSession(key, capabilities, callback.Get());
  capabilities.Clear();
  capabilities.Put(on_device_model::CapabilityFlags::kAudioInput);
  ai_manager_->CanCreateSession(key, capabilities, callback.Get());
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
  optimization_guide::ModelBasedCapabilityKey key =
      optimization_guide::ModelBasedCapabilityKey::kPromptApi;
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::kAvailable))
      .Times(3);
  on_device_model::Capabilities capabilities;
  ai_manager_->CanCreateSession(key, capabilities, callback.Get());
  capabilities.Put(on_device_model::CapabilityFlags::kImageInput);
  ai_manager_->CanCreateSession(key, capabilities, callback.Get());
  capabilities.Put(on_device_model::CapabilityFlags::kAudioInput);
  ai_manager_->CanCreateSession(key, capabilities, callback.Get());
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

class AIManagerIsLanguagesSupportedTest : public AITestUtils::AITestBase {
 protected:
  static constexpr char kValidLanguageCode[] = "en";
  static constexpr char kInvalidLanguageCode[] = "ja";

  std::vector<blink::mojom::AILanguageCodePtr> valid_language_codes() {
    std::vector<blink::mojom::AILanguageCodePtr> languages;
    languages.emplace_back(
        blink::mojom::AILanguageCode::New(kValidLanguageCode));
    return languages;
  }

  std::vector<blink::mojom::AILanguageCodePtr> invalid_language_codes() {
    std::vector<blink::mojom::AILanguageCodePtr> languages;
    languages.emplace_back(
        blink::mojom::AILanguageCode::New(kInvalidLanguageCode));
    return languages;
  }

  std::vector<blink::mojom::AILanguageCodePtr> mixed_language_codes() {
    std::vector<blink::mojom::AILanguageCodePtr> languages;
    languages.emplace_back(
        blink::mojom::AILanguageCode::New(kValidLanguageCode));
    languages.emplace_back(
        blink::mojom::AILanguageCode::New(kInvalidLanguageCode));
    return languages;
  }
};

TEST_F(AIManagerIsLanguagesSupportedTest, OneVector) {
  EXPECT_TRUE(AIManager::IsLanguagesSupported(valid_language_codes()));
  EXPECT_FALSE(AIManager::IsLanguagesSupported(invalid_language_codes()));
  EXPECT_FALSE(AIManager::IsLanguagesSupported(mixed_language_codes()));
}

TEST_F(AIManagerIsLanguagesSupportedTest, TwoVectorsAndOneCode) {
  EXPECT_TRUE(AIManager::IsLanguagesSupported(
      valid_language_codes(), valid_language_codes(),
      blink::mojom::AILanguageCode::New(kValidLanguageCode)));
  EXPECT_FALSE(AIManager::IsLanguagesSupported(
      valid_language_codes(), invalid_language_codes(),
      blink::mojom::AILanguageCode::New(kValidLanguageCode)));
  EXPECT_FALSE(AIManager::IsLanguagesSupported(
      invalid_language_codes(), mixed_language_codes(),
      blink::mojom::AILanguageCode::New(kValidLanguageCode)));
  EXPECT_FALSE(AIManager::IsLanguagesSupported(
      valid_language_codes(), valid_language_codes(),
      blink::mojom::AILanguageCode::New(kInvalidLanguageCode)));
}
