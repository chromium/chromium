// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/chrome_facilitated_payments_client.h"

#include <memory>

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/facilitated_payments/core/browser/pix_account_linking_manager.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/optimization_guide/core/mock_optimization_guide_decider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

class MockFacilitatedPaymentsController : public FacilitatedPaymentsController {
 public:
  explicit MockFacilitatedPaymentsController(content::WebContents* web_contents)
      : FacilitatedPaymentsController(web_contents) {}
  ~MockFacilitatedPaymentsController() override = default;

  MOCK_METHOD(bool, IsInLandscapeMode, (), (override));
  MOCK_METHOD(void,
              Show,
              (base::span<const autofill::BankAccount> bank_account_suggestions,
               base::OnceCallback<void(int64_t)> on_payment_account_selected),
              (override));
  MOCK_METHOD(void,
              ShowForEwallet,
              (base::span<const autofill::Ewallet> ewallet_suggestions,
               base::OnceCallback<void(int64_t)> on_payment_account_selected),
              (override));
  MOCK_METHOD(void, ShowProgressScreen, (), (override));
  MOCK_METHOD(void, ShowErrorScreen, (), (override));
  MOCK_METHOD(void, Dismiss, (), (override));
  MOCK_METHOD(void, ShowPixAccountLinkingPrompt, (), (override));
};

class MockPixAccountLinkingManager
    : public payments::facilitated::PixAccountLinkingManager {
 public:
  explicit MockPixAccountLinkingManager(
      payments::facilitated::FacilitatedPaymentsClient* client)
      : PixAccountLinkingManager(client) {}
  ~MockPixAccountLinkingManager() override = default;

  MOCK_METHOD(void, MaybeShowPixAccountLinkingPrompt, (), (override));
};

class ChromeFacilitatedPaymentsClientTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    client_ = std::make_unique<ChromeFacilitatedPaymentsClient>(
        web_contents(), &optimization_guide_decider_);
    auto controller =
        std::make_unique<MockFacilitatedPaymentsController>(web_contents());
    controller_ = controller.get();
    client_->SetFacilitatedPaymentsControllerForTesting(std::move(controller));
    auto pix_account_linking_manager =
        std::make_unique<MockPixAccountLinkingManager>(client_.get());
    pix_account_linking_manager_ = pix_account_linking_manager.get();
    client_->SetPixAccountLinkingManagerForTesting(
        std::move(pix_account_linking_manager));
  }

  void TearDown() override {
    ChromeRenderViewHostTestHarness::TearDown();
  }

  auto& base_client() {
    return static_cast<payments::facilitated::FacilitatedPaymentsClient&>(
        *client_);
  }

  MockFacilitatedPaymentsController& controller() { return *controller_; }

  MockPixAccountLinkingManager& pix_account_linking_manager() {
    return *pix_account_linking_manager_;
  }

 protected:
  optimization_guide::MockOptimizationGuideDecider optimization_guide_decider_;
  std::unique_ptr<ChromeFacilitatedPaymentsClient> client_;
  raw_ptr<MockFacilitatedPaymentsController> controller_;
  raw_ptr<MockPixAccountLinkingManager> pix_account_linking_manager_;
};

TEST_F(ChromeFacilitatedPaymentsClientTest, GetPaymentsDataManager) {
  EXPECT_NE(nullptr, base_client().GetPaymentsDataManager());
}

TEST_F(ChromeFacilitatedPaymentsClientTest,
       GetFacilitatedPaymentsNetworkInterface) {
  EXPECT_NE(nullptr, base_client().GetFacilitatedPaymentsNetworkInterface());
}

// Test the client forwards call to show Pix FOP selector to the controller.
TEST_F(ChromeFacilitatedPaymentsClientTest,
       ShowPixPaymentPrompt_ControllerDefaultTrue) {
  EXPECT_CALL(controller(), Show);

  base_client().ShowPixPaymentPrompt({}, base::DoNothing());
}

// Test that the `EWALLET_MERCHANT_ALLOWLIST` and
// `PIX_PAYMENT_MERCHANT_ALLOWLIST` optimization type is registered when the
// `ChromeFacilitatedPaymentClient` is created.
TEST_F(ChromeFacilitatedPaymentsClientTest, RegisterAllowlists) {
  base::test::ScopedFeatureList feature_list(
      payments::facilitated::kEwalletPayments);
  EXPECT_CALL(optimization_guide_decider_,
              RegisterOptimizationTypes(testing::ElementsAre(
                  optimization_guide::proto::PIX_MERCHANT_ORIGINS_ALLOWLIST)))
      .Times(1);
  EXPECT_CALL(optimization_guide_decider_,
              RegisterOptimizationTypes(testing::ElementsAre(
                  optimization_guide::proto::EWALLET_MERCHANT_ALLOWLIST)))
      .Times(1);

  // Re-create the client; it should register the allowlist.
  client_ = std::make_unique<ChromeFacilitatedPaymentsClient>(
      web_contents(), &optimization_guide_decider_);
}

// Test that the `EWALLET_MERCHANT_ALLOWLIST` optimization type is not
// registered when when the `ChromeFacilitatedPaymentClient` is created and the
// eWallet experiment is disabled.
TEST_F(ChromeFacilitatedPaymentsClientTest, RegisterAllowlists_EWalletExpOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(payments::facilitated::kEwalletPayments);

  EXPECT_CALL(optimization_guide_decider_,
              RegisterOptimizationTypes(testing::ElementsAre(
                  optimization_guide::proto::PIX_MERCHANT_ORIGINS_ALLOWLIST)))
      .Times(1);
  EXPECT_CALL(optimization_guide_decider_,
              RegisterOptimizationTypes(testing::ElementsAre(
                  optimization_guide::proto::EWALLET_MERCHANT_ALLOWLIST)))
      .Times(0);

  // Re-create the client; it should not register the allowlist.
  client_ = std::make_unique<ChromeFacilitatedPaymentsClient>(
      web_contents(), &optimization_guide_decider_);
}

// Test the client forwards call for showing the progress screen to the
// controller.
TEST_F(ChromeFacilitatedPaymentsClientTest, ShowProgressScreen) {
  EXPECT_CALL(controller(), ShowProgressScreen);

  base_client().ShowProgressScreen();
}

// Test the client forwards call for showing the error screen to the controller.
TEST_F(ChromeFacilitatedPaymentsClientTest, ShowErrorScreen) {
  EXPECT_CALL(controller(), ShowErrorScreen);

  base_client().ShowErrorScreen();
}

// Test that the client forwards call to show Pix account linking prompt to the
// controller.
TEST_F(ChromeFacilitatedPaymentsClientTest, ShowPixAccountLinkingPrompt) {
  EXPECT_CALL(controller(), ShowPixAccountLinkingPrompt);

  base_client().ShowPixAccountLinkingPrompt();
}

// Test that the controller is able to process requests to show different
// screens back to back.
TEST_F(ChromeFacilitatedPaymentsClientTest,
       ControllerIsAbleToProcessBackToBackShowRequests) {
  EXPECT_CALL(controller(), Show);
  EXPECT_CALL(controller(), ShowProgressScreen);

  base_client().ShowPixPaymentPrompt({}, base::DoNothing());
  base_client().ShowProgressScreen();
}

// Test the client forwards call for closing the bottom sheet to the
// controller.
TEST_F(ChromeFacilitatedPaymentsClientTest, DismissPrompt) {
  EXPECT_CALL(controller(), Dismiss);

  base_client().DismissPrompt();
}

// Test the client forwards call to check the device screen orientation to the
// controller.
TEST_F(ChromeFacilitatedPaymentsClientTest, IsInLandscapeMode) {
  EXPECT_CALL(controller(), IsInLandscapeMode);

  base_client().IsInLandscapeMode();
}

// Test that the client forwards call to show eWallet FOP selector to the
// controller.
TEST_F(ChromeFacilitatedPaymentsClientTest,
       ShowEwalletPaymentPrompt_ControllerInvoked) {
  EXPECT_CALL(controller(), ShowForEwallet);
  base_client().ShowEwalletPaymentPrompt({}, base::DoNothing());
}

// Test that the client forwards call to initiate Pix account linking flow to
// the Pix account linking manager.
TEST_F(ChromeFacilitatedPaymentsClientTest, InitPixAccountLinkingFlow) {
  EXPECT_CALL(pix_account_linking_manager(), MaybeShowPixAccountLinkingPrompt);

  base_client().InitPixAccountLinkingFlow();
}
