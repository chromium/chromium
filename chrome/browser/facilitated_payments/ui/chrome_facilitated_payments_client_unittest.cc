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

using ChromeFacilitatedPaymentsClientTest = ChromeRenderViewHostTestHarness;

TEST_F(ChromeFacilitatedPaymentsClientTest, GetPaymentsDataManager) {
  MockOptimizationGuideDecider optimization_guide_decider;
  auto client = std::make_unique<ChromeFacilitatedPaymentsClient>(
      web_contents(), &optimization_guide_decider);

  EXPECT_NE(nullptr, client->GetPaymentsDataManager());
}

TEST_F(ChromeFacilitatedPaymentsClientTest,
       GetFacilitatedPaymentsNetworkInterface) {
  MockOptimizationGuideDecider optimization_guide_decider;
  auto client = std::make_unique<ChromeFacilitatedPaymentsClient>(
      web_contents(), &optimization_guide_decider);

  EXPECT_NE(nullptr, client->GetFacilitatedPaymentsNetworkInterface());
}

// Test ShowPixPaymentPrompt method returns true when
// FacilitatedPaymentsController returns true.
TEST_F(ChromeFacilitatedPaymentsClientTest,
       ShowPixPaymentPrompt_ControllerDefaultTrue) {
  MockOptimizationGuideDecider optimization_guide_decider;
  auto client = std::make_unique<ChromeFacilitatedPaymentsClient>(
      web_contents(), &optimization_guide_decider);

  std::unique_ptr<MockFacilitatedPaymentsController> mock_controller =
      std::make_unique<MockFacilitatedPaymentsController>();
  EXPECT_CALL(*mock_controller, Show(_, _, _)).WillOnce(Return(true));
  client->SetFacilitatedPaymentsControllerForTesting(
      std::move(mock_controller));

  EXPECT_TRUE(client->ShowPixPaymentPrompt({}, base::DoNothing()));
}

// Test ShowPixPaymentPrompt method returns false when
// FacilitatedPaymentsController returns false.
TEST_F(ChromeFacilitatedPaymentsClientTest,
       ShowPixPaymentPrompt_ControllerDefaultFalse) {
  MockOptimizationGuideDecider optimization_guide_decider;
  auto client = std::make_unique<ChromeFacilitatedPaymentsClient>(
      web_contents(), &optimization_guide_decider);

  std::unique_ptr<MockFacilitatedPaymentsController> mock_controller =
      std::make_unique<MockFacilitatedPaymentsController>();
  EXPECT_CALL(*mock_controller, Show(_, _, _)).WillOnce(Return(false));
  client->SetFacilitatedPaymentsControllerForTesting(
      std::move(mock_controller));

  EXPECT_FALSE(client->ShowPixPaymentPrompt({}, base::DoNothing()));
}

// Test ShowPixPaymentPrompt method returns false when there's no bank account.
TEST_F(ChromeFacilitatedPaymentsClientTest,
       ShowPixPaymentPrompt_NoBankAccounts) {
  MockOptimizationGuideDecider optimization_guide_decider;
  auto client = std::make_unique<ChromeFacilitatedPaymentsClient>(
      web_contents(), &optimization_guide_decider);

  std::unique_ptr<MockFacilitatedPaymentsController> mock_controller =
      std::make_unique<MockFacilitatedPaymentsController>();
  EXPECT_CALL(*mock_controller, Show);
  client->SetFacilitatedPaymentsControllerForTesting(
      std::move(mock_controller));

  EXPECT_FALSE(client->ShowPixPaymentPrompt({}, base::DoNothing()));
}
