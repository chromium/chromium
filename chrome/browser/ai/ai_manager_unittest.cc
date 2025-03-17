// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_manager.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/task/current_thread.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ai/ai_language_model.h"
#include "chrome/browser/ai/ai_test_utils.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_rewriter.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_writer.mojom.h"

using optimization_guide::MockSession;
using testing::_;
using testing::AtMost;
using testing::Invoke;
using testing::NiceMock;

class AIManagerTest : public AITestUtils::AITestBase {
 protected:
  void SetupMockOptimizationGuideKeyedService() override {
    AITestUtils::AITestBase::SetupMockOptimizationGuideKeyedService();

    ON_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
        .WillByDefault(
            [&] { return std::make_unique<NiceMock<MockSession>>(&session_); });
    ON_CALL(session_, GetTokenLimits())
        .WillByDefault(AITestUtils::GetFakeTokenLimits);
    ON_CALL(session_, GetOnDeviceFeatureMetadata())
        .WillByDefault(AITestUtils::GetFakeFeatureMetadata);
    ON_CALL(*mock_optimization_guide_keyed_service_,
            GetOnDeviceModelEligibility(_))
        .WillByDefault([](optimization_guide::ModelBasedCapabilityKey feature) {
          return optimization_guide::OnDeviceModelEligibilityReason::
              kFeatureNotEnabled;
        });
  }

 private:
  testing::NiceMock<MockSession> session_;
};

// Tests that involve invalid on-device model file paths should not crash when
// the associated RFH is destroyed.
TEST_F(AIManagerTest, NoUAFWithInvalidOnDeviceModelPath) {
  SetupMockOptimizationGuideKeyedService();

  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(
      optimization_guide::switches::kOnDeviceModelExecutionOverride,
      "invalid-on-device-model-file-path");

  base::MockCallback<blink::mojom::AIManager::CanCreateLanguageModelCallback>
      callback;
  EXPECT_CALL(callback, Run(_))
      .Times(AtMost(1))
      .WillOnce(Invoke([&](blink::mojom::ModelAvailabilityCheckResult result) {
        EXPECT_EQ(result, blink::mojom::ModelAvailabilityCheckResult::
                              kUnavailableFeatureNotEnabled);
      }));

  AIManager ai_manager = AIManager(main_rfh()->GetBrowserContext());
  ai_manager.CanCreateLanguageModel(/*options=*/{}, callback.Get());

  // The callback may still be pending, delete the WebContents and destroy the
  // associated RFH, which should not result in a UAF.
  DeleteContents();

  task_environment()->RunUntilIdle();
}

// Tests the `AIUserDataSet`'s behavior of managing the lifetime of
// `AILanguageModel`s.
TEST_F(AIManagerTest, AIContextBoundObjectSet) {
  SetupMockOptimizationGuideKeyedService();

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
          /*system_prompt=*/std::nullopt,
          std::vector<blink::mojom::AILanguageModelPromptPtr>(),
          std::vector<blink::mojom::AILanguageCodePtr>()));
  run_loop.Run();
  ASSERT_EQ(1u, GetAIManagerContextBoundObjectSetSize());

  // After resetting the session, the size of `AIContextBoundObjectSet` becomes
  // empty again.
  mock_session.reset();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return GetAIManagerContextBoundObjectSetSize() == 0u; }));
}

TEST_F(AIManagerTest, CanCreate) {
  SetupMockOptimizationGuideKeyedService();
  base::MockCallback<
      base::OnceCallback<void(blink::mojom::ModelAvailabilityCheckResult)>>
      callback;
  EXPECT_CALL(callback, Run(_))
      .Times(4)
      .WillRepeatedly(testing::Invoke(
          [&](blink::mojom::ModelAvailabilityCheckResult result) {
            EXPECT_EQ(result, blink::mojom::ModelAvailabilityCheckResult::
                                  kUnavailableFeatureNotEnabled);
          }));

  AIManager ai_manager = AIManager(main_rfh()->GetBrowserContext());
  ai_manager.CanCreateLanguageModel(/*options=*/{}, callback.Get());
  ai_manager.CanCreateWriter(/*options=*/{}, callback.Get());
  ai_manager.CanCreateSummarizer(/*options=*/{}, callback.Get());
  ai_manager.CanCreateRewriter(/*options=*/{}, callback.Get());
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
