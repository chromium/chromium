// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_manager_keyed_service.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/current_thread.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ai/ai_assistant.h"
#include "chrome/browser/ai/ai_manager_keyed_service_factory.h"
#include "chrome/browser/ai/ai_test_utils.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_assistant.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"

using optimization_guide::MockSession;
using optimization_guide::MockSessionWrapper;
using testing::_;
using testing::AtMost;
using testing::Invoke;
using testing::NiceMock;

class AIManagerKeyedServiceTest : public AITestUtils::AITestBase {
 protected:
  void SetupMockOptimizationGuideKeyedService() {
    AITestUtils::AITestBase::SetupMockOptimizationGuideKeyedService();

    ON_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
        .WillByDefault(
            [&] { return std::make_unique<MockSessionWrapper>(&session_); });
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
TEST_F(AIManagerKeyedServiceTest, NoUAFWithInvalidOnDeviceModelPath) {
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

  base::MockCallback<blink::mojom::AIManager::CanCreateAssistantCallback>
      callback;
  EXPECT_CALL(callback, Run(_))
      .Times(AtMost(1))
      .WillOnce(Invoke([&](blink::mojom::ModelAvailabilityCheckResult result) {
        EXPECT_EQ(
            result,
            blink::mojom::ModelAvailabilityCheckResult::kNoFeatureNotEnabled);
      }));

  AIManagerKeyedService* ai_manager =
      AIManagerKeyedServiceFactory::GetAIManagerKeyedService(
          main_rfh()->GetBrowserContext());
  ai_manager->CanCreateAssistant(callback.Get());

  // The callback may still be pending, delete the WebContents and destroy the
  // associated RFH, which should not result in a UAF.
  DeleteContents();

  task_environment()->RunUntilIdle();
}

// Tests the `AIUserDataSet`'s behavior of managing the lifetime of
// `AIAssistant`s.
TEST_F(AIManagerKeyedServiceTest, AIContextBoundObjectSet) {
  SetupMockOptimizationGuideKeyedService();

  mojo::Remote<blink::mojom::AIAssistant> mock_session;
  AITestUtils::MockCreateAssistantClient mock_create_assistant_client;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_create_assistant_client, OnResult(_, _))
      .WillOnce(testing::Invoke(
          [&](mojo::PendingRemote<blink::mojom::AIAssistant> assistant,
              blink::mojom::AIAssistantInfoPtr info) {
            EXPECT_TRUE(assistant);
            mock_session =
                mojo::Remote<blink::mojom::AIAssistant>(std::move(assistant));
            run_loop.Quit();
          }));

  mojo::Remote<blink::mojom::AIManager> mock_remote = GetAIManagerRemote();
  // Initially the `AIContextBoundObjectSet` only contains the
  // `AIManagerReceiverRemover`.
  base::WeakPtr<AIContextBoundObjectSet> context_bound_objects =
      AIContextBoundObjectSet::GetFromContext(mock_host())
          ->GetWeakPtrForTesting();
  ASSERT_EQ(1u, context_bound_objects->GetSizeForTesting());

  // After creating one `AIAssistant`, the `AIContextBoundObjectSet` contains 2
  // elements.
  mock_remote->CreateAssistant(
      mock_create_assistant_client.BindNewPipeAndPassRemote(),
      blink::mojom::AIAssistantCreateOptions::New(
          /*sampling_params=*/nullptr,
          /*system_prompt=*/std::nullopt,
          /*initial_prompts=*/
          std::vector<blink::mojom::AIAssistantInitialPromptPtr>()));
  run_loop.Run();
  ASSERT_EQ(2u, context_bound_objects->GetSizeForTesting());

  // After resetting the session, the size of `AIContextBoundObjectSet` becomes
  // 1 again and should be removed from the context.
  mock_session.reset();
  ASSERT_TRUE(base::test::RunUntil([&context_bound_objects] {
    return context_bound_objects->GetSizeForTesting() == 1u;
  }));
}

// Tests that the receiver will be removed after the `ReceiverContext` is
// destroyed.
TEST_F(AIManagerKeyedServiceTest, ClearReceiverAfterResetHost) {
  SetupMockOptimizationGuideKeyedService();

  // Initially, the receiver set is empty.
  ASSERT_EQ(0u, GetAIManagerReceiversSize());

  mojo::Remote<blink::mojom::AIManager> mock_remote = GetAIManagerRemote();

  // After getting the `AIManager`, the receiver set contains 1 element.
  ASSERT_EQ(1u, GetAIManagerReceiversSize());

  // After resetting the host, the corresponding receivers should be cleared
  // from the set.
  ResetMockHost();
  ASSERT_TRUE(base::test::RunUntil(
      [this] { return GetAIManagerReceiversSize() == 0u; }));
}
