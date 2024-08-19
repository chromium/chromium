// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_manager_keyed_service.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ai/ai_manager_keyed_service_factory.h"
#include "chrome/browser/ai/ai_text_session.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_text_session.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_text_session_info.mojom.h"

using optimization_guide::MockSession;
using optimization_guide::MockSessionWrapper;
using testing::_;
using testing::An;
using testing::AtMost;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
namespace {

class MockSupportsUserData : public base::SupportsUserData {};

const optimization_guide::TokenLimits& GetFakeTokenLimits() {
  static const optimization_guide::TokenLimits limits{
      .max_tokens = 4096,
      .max_context_tokens = 2048,
      .max_execute_tokens = 1024,
      .max_output_tokens = 1024,
  };
  return limits;
}

}  // namespace

class AIManagerKeyedServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SetUpOptimizationGuide();
  }

  void TearDown() override {
    mock_optimization_guide_keyed_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  MockSupportsUserData* mock_host() { return &mock_host_; }

 private:
  void SetUpOptimizationGuide() {
    mock_optimization_guide_keyed_service_ =
        static_cast<NiceMock<MockOptimizationGuideKeyedService>*>(
            OptimizationGuideKeyedServiceFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    profile(),
                    base::BindRepeating([](content::BrowserContext* context)
                                            -> std::unique_ptr<KeyedService> {
                      return std::make_unique<
                          NiceMock<MockOptimizationGuideKeyedService>>();
                    })));

    ON_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
        .WillByDefault(
            [&] { return std::make_unique<MockSessionWrapper>(&session_); });
    ON_CALL(session_, GetTokenLimits()).WillByDefault(GetFakeTokenLimits);
  }

  raw_ptr<testing::NiceMock<MockOptimizationGuideKeyedService>>
      mock_optimization_guide_keyed_service_;
  testing::NiceMock<MockSession> session_;
  MockSupportsUserData mock_host_;
};

// Tests that involve invalid on-device model file paths should not crash when
// the associated RFH is destroyed.
TEST_F(AIManagerKeyedServiceTest, NoUAFWithInvalidOnDeviceModelPath) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(
      optimization_guide::switches::kOnDeviceModelExecutionOverride,
      "invalid-on-device-model-file-path");

  base::MockCallback<blink::mojom::AIManager::CanCreateTextSessionCallback>
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
  ai_manager->CanCreateTextSession(callback.Get());

  // The callback may still be pending, delete the WebContents and destroy the
  // associated RFH, which should not result in a UAF.
  DeleteContents();

  task_environment()->RunUntilIdle();
}

// Tests the `AIUserDataSet`'s behavior of managing the lifetime of
// `AITextSession`s.
TEST_F(AIManagerKeyedServiceTest, AIContextBoundObjectSet) {
  base::MockCallback<blink::mojom::AIManager::CreateTextSessionCallback>
      callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(_))
      .Times(AtMost(1))
      .WillOnce(Invoke([&](blink::mojom::AITextSessionInfoPtr result) {
        EXPECT_TRUE(result);
        run_loop.Quit();
      }));

  AIManagerKeyedService* ai_manager =
      AIManagerKeyedServiceFactory::GetAIManagerKeyedService(
          main_rfh()->GetBrowserContext());

  mojo::Remote<blink::mojom::AIManager> mock_remote;
  mojo::Remote<blink::mojom::AITextSession> mock_session;
  ai_manager->AddReceiver(mock_remote.BindNewPipeAndPassReceiver(),
                          mock_host());
  // Initially the `AIUserDataSet` is empty.
  base::WeakPtr<AIContextBoundObjectSet> context_bound_objects =
      AIContextBoundObjectSet::GetFromContext(mock_host())
          ->GetWeakPtrForTesting();
  ASSERT_EQ(0u, context_bound_objects->GetSizeForTesting());

  // After creating one `AITextSession`, the `AIUserDataSet` contains 1
  // element.
  mock_remote->CreateTextSession(mock_session.BindNewPipeAndPassReceiver(),
                                 nullptr, std::nullopt, callback.Get());
  run_loop.Run();
  ASSERT_EQ(1u, context_bound_objects->GetSizeForTesting());

  // After resetting the session, the `AIUserDataSet` becomes empty again and
  // should be removed from the context.
  mock_session.reset();
  task_environment()->RunUntilIdle();
  ASSERT_FALSE(context_bound_objects);
}
