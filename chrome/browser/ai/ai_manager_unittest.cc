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
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"

using optimization_guide::MockSession;
using testing::_;
using testing::AtMost;
using testing::Invoke;
using testing::NiceMock;

class AIManagerTest : public AITestUtils::AITestBase {
 protected:
  void SetupMockOptimizationGuideKeyedService() {
    AITestUtils::AITestBase::SetupMockOptimizationGuideKeyedService();

    ON_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
        .WillByDefault(
            [&] { return std::make_unique<NiceMock<MockSession>>(&session_); });
    ON_CALL(session_, GetTokenLimits())
        .WillByDefault(AITestUtils::GetFakeTokenLimits);
    ON_CALL(session_, GetOnDeviceFeatureMetadata())
        .WillByDefault(AITestUtils::GetFakeFeatureMetadata);
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

  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanCreateOnDeviceSession(_, _))
      .Times(AtMost(1))
      .WillOnce(Invoke([](optimization_guide::ModelBasedCapabilityKey feature,
                          optimization_guide::OnDeviceModelEligibilityReason*
                              on_device_model_eligibility_reason) {
        *on_device_model_eligibility_reason = optimization_guide::
            OnDeviceModelEligibilityReason::kFeatureNotEnabled;
        return false;
      }));

  base::MockCallback<blink::mojom::AIManager::CanCreateLanguageModelCallback>
      callback;
  EXPECT_CALL(callback, Run(_))
      .Times(AtMost(1))
      .WillOnce(Invoke([&](blink::mojom::ModelAvailabilityCheckResult result) {
        EXPECT_EQ(
            result,
            blink::mojom::ModelAvailabilityCheckResult::kNoFeatureNotEnabled);
      }));

  AIManager ai_manager = AIManager(main_rfh()->GetBrowserContext());
  ai_manager.CanCreateLanguageModel(callback.Get());

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
              blink::mojom::AILanguageModelInfoPtr info) {
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
          /*initial_prompts=*/
          std::vector<blink::mojom::AILanguageModelInitialPromptPtr>()));
  run_loop.Run();
  ASSERT_EQ(1u, GetAIManagerContextBoundObjectSetSize());

  // After resetting the session, the size of `AIContextBoundObjectSet` becomes
  // empty again.
  mock_session.reset();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return GetAIManagerContextBoundObjectSetSize() == 0u; }));
}
