// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_controller.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_bottom_sheet_bridge.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/facilitated_payments/core/ui_utils/facilitated_payments_ui_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace {

class MockFacilitatedPaymentsBottomSheetBridge
    : public payments::facilitated::FacilitatedPaymentsBottomSheetBridge {
 public:
  MockFacilitatedPaymentsBottomSheetBridge(
      content::WebContents* web_contents,
      FacilitatedPaymentsController* controller)
      : payments::facilitated::FacilitatedPaymentsBottomSheetBridge(
            web_contents,
            controller) {}

  ~MockFacilitatedPaymentsBottomSheetBridge() override = default;

  MOCK_METHOD(bool, IsInLandscapeMode, (), (override));
  MOCK_METHOD(
      void,
      RequestShowContent,
      (base::span<const autofill::BankAccount> bank_account_suggestions),
      (override));
  MOCK_METHOD(void,
              RequestShowContentForEwallet,
              (base::span<const autofill::Ewallet> ewallet_suggestions),
              (override));
  MOCK_METHOD(void, ShowProgressScreen, (), (override));
  MOCK_METHOD(void, ShowErrorScreen, (), (override));
  MOCK_METHOD(void, Dismiss, (), (override));
  MOCK_METHOD(void, OnDismissed, (), (override));
};

}  // namespace

class FacilitatedPaymentsControllerTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    controller_ =
        std::make_unique<FacilitatedPaymentsController>(web_contents());
    auto mock_view = std::make_unique<MockFacilitatedPaymentsBottomSheetBridge>(
        web_contents(), controller_.get());
    mock_view_ = mock_view.get();
    controller_->SetViewForTesting(std::move(mock_view));
  }

  void TearDown() override {
    mock_view_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  raw_ptr<MockFacilitatedPaymentsBottomSheetBridge> mock_view_;
  std::unique_ptr<FacilitatedPaymentsController> controller_;
  const std::vector<autofill::BankAccount> bank_accounts_ = {
      autofill::test::CreatePixBankAccount(100L),
      autofill::test::CreatePixBankAccount(200L)};
  const std::vector<autofill::Ewallet> ewallets_ = {
      autofill::test::CreateEwalletAccount(100L),
      autofill::test::CreateEwalletAccount(200L)};
};

// Test controller forwards call for showing the Pix FOP selector to the view.
TEST_F(FacilitatedPaymentsControllerTest, Show_UserHasPixAccounts) {
  EXPECT_CALL(*mock_view_,
              RequestShowContent(testing::ElementsAreArray(bank_accounts_)));

  controller_->Show(bank_accounts_, base::DoNothing());
}

// Test controller does not forward call for showing the Pix FOP selector to the
// view when there are no Pix accounts.
TEST_F(FacilitatedPaymentsControllerTest, Show_UserHasNoPixAccounts) {
  EXPECT_CALL(*mock_view_, RequestShowContent).Times(0);

  controller_->Show({}, base::DoNothing());
}

// Test OnDismissed method.
TEST_F(FacilitatedPaymentsControllerTest, OnDismissed) {
  // Show the bottom sheet and set the user decision callback.
  base::MockCallback<base::OnceCallback<void(bool, int64_t)>>
      mock_on_user_decision_callback;
  controller_->Show(bank_accounts_, mock_on_user_decision_callback.Get());

  // Verify that dismissal event is forwarded to the view. Also verify that the
  // manager is informed of the diamissal via the callback. The second
  // OnDismissed call is triggered when the test fixture destroys the
  // `controller`.
  EXPECT_CALL(*mock_view_, OnDismissed).Times(2);
  EXPECT_CALL(mock_on_user_decision_callback,
              Run(/*is_selected=*/false, /*selected_bank_account_id=*/-1L));

  controller_->OnDismissed(nullptr);
}

// Test onBankAccountSelected method.
TEST_F(FacilitatedPaymentsControllerTest, onBankAccountSelected) {
  base::MockCallback<base::OnceCallback<void(bool, int64_t)>>
      mock_on_user_decision_callback;

  // view_ is assigned when the bottom sheet is shown.
  controller_->Show(bank_accounts_, mock_on_user_decision_callback.Get());

  // When bank account is selected, call back should be called with true and
  // instrument id from selected bank account.
  EXPECT_CALL(mock_on_user_decision_callback,
              Run(/*is_selected=*/true, /*selected_bank_account_id=*/100L));

  controller_->OnBankAccountSelected(nullptr, 100L);
}

// Test controller forwards call for showing the progress screen to the view.
TEST_F(FacilitatedPaymentsControllerTest, ShowProgressScreen) {
  EXPECT_CALL(*mock_view_, ShowProgressScreen);

  controller_->ShowProgressScreen();
}

// Test controller forwards call for showing the progress screen to the view.
TEST_F(FacilitatedPaymentsControllerTest, ShowErrorScreen) {
  EXPECT_CALL(*mock_view_, ShowErrorScreen);

  controller_->ShowErrorScreen();
}

// Test that the view is able to process requests to show different screens back
// to back.
TEST_F(FacilitatedPaymentsControllerTest,
       ViewIsAbleToProcessBackToBackShowRequests) {
  EXPECT_CALL(*mock_view_, RequestShowContent);
  EXPECT_CALL(*mock_view_, ShowProgressScreen);

  controller_->Show(bank_accounts_, base::DoNothing());
  controller_->ShowProgressScreen();
}

// Test controller forwards call for closing the bottom sheet to the view.
TEST_F(FacilitatedPaymentsControllerTest, Dismiss) {
  EXPECT_CALL(*mock_view_, Dismiss);

  controller_->Dismiss();
}

// Test controller forwards call to check the device screen orientation to the
// view.
TEST_F(FacilitatedPaymentsControllerTest, IsInLandscapeMode) {
  EXPECT_CALL(*mock_view_, IsInLandscapeMode);

  controller_->IsInLandscapeMode();
}

class FacilitatedPaymentsControllerTestForUiEvents
    : public FacilitatedPaymentsControllerTest,
      public testing::WithParamInterface<payments::facilitated::UiEvent> {
 public:
  payments::facilitated::UiEvent ui_event() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    FacilitatedPaymentsControllerTest,
    FacilitatedPaymentsControllerTestForUiEvents,
    testing::Values(payments::facilitated::UiEvent::kNewScreenShown,
                    payments::facilitated::UiEvent::kScreenClosedNotByUser,
                    payments::facilitated::UiEvent::kScreenClosedByUser));

TEST_P(FacilitatedPaymentsControllerTestForUiEvents, OnUiEvent) {
  // Set the UI event listener.
  base::MockCallback<
      base::RepeatingCallback<void(payments::facilitated::UiEvent)>>
      mock_ui_event_listener;
  controller_->SetUiEventListener(mock_ui_event_listener.Get());

  // Verify that the UI event is communicated to the feature via the callback.
  EXPECT_CALL(mock_ui_event_listener, Run(ui_event()));
  if (ui_event() == payments::facilitated::UiEvent::kScreenClosedNotByUser ||
      ui_event() == payments::facilitated::UiEvent::kScreenClosedByUser) {
    // Verify that the screen closing event is communicated to the
    // view. The second OnDismissed call is triggered when the test
    // fixture destroys the `controller`.
    EXPECT_CALL(*mock_view_, OnDismissed).Times(2);
  }

  controller_->OnUiEvent(nullptr, static_cast<jint>(ui_event()));
}

// Test controller forwards call for showing the eWallet FOP selector to the
// view.
TEST_F(FacilitatedPaymentsControllerTest,
       ShowForEwallet_UserHasEwalletAccounts) {
  EXPECT_CALL(*mock_view_, RequestShowContentForEwallet(
                               testing::ElementsAreArray(ewallets_)));

  controller_->ShowForEwallet(ewallets_, base::DoNothing());
}

// Test controller does not forward call for showing the eWallet FOP selector to
// the view when there are no eWallet accounts.
TEST_F(FacilitatedPaymentsControllerTest,
       ShowForEwallet_UserHasNoEwalletAccounts) {
  EXPECT_CALL(*mock_view_, RequestShowContentForEwallet).Times(0);

  controller_->ShowForEwallet({}, base::DoNothing());
}

// Test OnDismissed method for eWallet.
TEST_F(FacilitatedPaymentsControllerTest, ShowForEwallet_OnDismissed) {
  // Show the bottom sheet and set the user decision callback.
  base::MockCallback<base::OnceCallback<void(bool, int64_t)>>
      mock_on_user_decision_callback;
  controller_->ShowForEwallet(ewallets_, mock_on_user_decision_callback.Get());

  // Verify that dismissal event is forwarded to the view. Also verify that the
  // manager is informed of the diamissal via the callback. The second
  // OnDismissed call is triggered when the test fixture destroys the
  // `controller`.
  EXPECT_CALL(*mock_view_, OnDismissed).Times(2);
  EXPECT_CALL(mock_on_user_decision_callback,
              Run(/*is_ewallet_selected=*/false,
                  /*selected_ewallet_instrument_id=*/-1L));

  controller_->OnDismissed(nullptr);
}

// Test OnEwalletSelected method.
TEST_F(FacilitatedPaymentsControllerTest, OnEwalletSelected) {
  base::MockCallback<base::OnceCallback<void(bool, int64_t)>>
      mock_on_user_decision_callback;

  // view_ is assigned when the bottom sheet is shown.
  controller_->ShowForEwallet(ewallets_, mock_on_user_decision_callback.Get());

  // When an eWallet is selected, call back should be called with true and
  // instrument id from selected eWallet.
  EXPECT_CALL(
      mock_on_user_decision_callback,
      Run(/*is_selected=*/true, /*selected_ewallet_instrument_id=*/100L));

  controller_->OnEwalletSelected(nullptr, 100L);
}
