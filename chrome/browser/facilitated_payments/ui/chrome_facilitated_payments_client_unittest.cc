// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/chrome_facilitated_payments_client.h"

#include <memory>

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

class MockOptimizationGuideDecider
    : public optimization_guide::OptimizationGuideDecider {
 public:
  MOCK_METHOD(void,
              RegisterOptimizationTypes,
              (const std::vector<optimization_guide::proto::OptimizationType>&),
              (override));
  MOCK_METHOD(void,
              CanApplyOptimization,
              (const GURL&,
               optimization_guide::proto::OptimizationType,
               optimization_guide::OptimizationGuideDecisionCallback),
              (override));
  MOCK_METHOD(optimization_guide::OptimizationGuideDecision,
              CanApplyOptimization,
              (const GURL&,
               optimization_guide::proto::OptimizationType,
               optimization_guide::OptimizationMetadata*),
              (override));
  MOCK_METHOD(
      void,
      CanApplyOptimizationOnDemand,
      (const std::vector<GURL>&,
       const base::flat_set<optimization_guide::proto::OptimizationType>&,
       optimization_guide::proto::RequestContext,
       optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback,
       std::optional<optimization_guide::proto::RequestContextMetadata>
           request_context_metadata),
      (override));
};

class MockFacilitatedPaymentsController : public FacilitatedPaymentsController {
 public:
  MockFacilitatedPaymentsController() = default;

  MOCK_METHOD(
      bool,
      Show,
      (std::unique_ptr<
           payments::facilitated::FacilitatedPaymentsBottomSheetBridge> view,
       base::span<const autofill::BankAccount> bank_account_suggestions,
       content::WebContents* web_contents),
      (override));
};

class ChromeFacilitatedPaymentsClientTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    client_ = std::make_unique<ChromeFacilitatedPaymentsClient>(
        web_contents(), &optimization_guide_decider_);
    auto controller = std::make_unique<MockFacilitatedPaymentsController>();
    controller_ = controller.get();
    client().SetFacilitatedPaymentsControllerForTesting(std::move(controller));
  }

  void TearDown() override {
    controller_ = nullptr;
    client_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  auto& base_client() {
    return static_cast<payments::facilitated::FacilitatedPaymentsClient&>(
        client());
  }
  ChromeFacilitatedPaymentsClient& client() { return *client_; }
  MockFacilitatedPaymentsController& controller() { return *controller_; }

 private:
  MockOptimizationGuideDecider optimization_guide_decider_;
  std::unique_ptr<ChromeFacilitatedPaymentsClient> client_;
  raw_ptr<MockFacilitatedPaymentsController> controller_;
};

TEST_F(ChromeFacilitatedPaymentsClientTest, GetPaymentsDataManager) {
  EXPECT_NE(nullptr, base_client().GetPaymentsDataManager());
}

TEST_F(ChromeFacilitatedPaymentsClientTest,
       GetFacilitatedPaymentsNetworkInterface) {
  EXPECT_NE(nullptr, base_client().GetFacilitatedPaymentsNetworkInterface());
}

// Test ShowPixPaymentPrompt method returns true when
// FacilitatedPaymentsController returns true.
TEST_F(ChromeFacilitatedPaymentsClientTest,
       ShowPixPaymentPrompt_ControllerDefaultTrue) {
  EXPECT_CALL(controller(), Show).WillOnce(Return(true));
  EXPECT_TRUE(base_client().ShowPixPaymentPrompt({}, base::DoNothing()));
}

// Test ShowPixPaymentPrompt method returns false when
// FacilitatedPaymentsController returns false.
TEST_F(ChromeFacilitatedPaymentsClientTest,
       ShowPixPaymentPrompt_ControllerDefaultFalse) {
  EXPECT_CALL(controller(), Show).WillOnce(Return(false));
  EXPECT_FALSE(base_client().ShowPixPaymentPrompt({}, base::DoNothing()));
}

// Test ShowPixPaymentPrompt method returns false when there's no bank account.
TEST_F(ChromeFacilitatedPaymentsClientTest,
       ShowPixPaymentPrompt_NoBankAccounts) {
  EXPECT_CALL(controller(), Show);
  EXPECT_FALSE(base_client().ShowPixPaymentPrompt({}, base::DoNothing()));
}
