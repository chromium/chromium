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
      bool,
      RequestShowContent,
      (base::span<const autofill::BankAccount> bank_account_suggestions),
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
};

// Test Show method returns true when FacilitatedPaymentsBottomSheetBridge
// is able to show.
TEST_F(FacilitatedPaymentsControllerTest, Show_BridgeWasAbleToShow) {
  ON_CALL(*mock_view_, RequestShowContent).WillByDefault(Return(true));

  EXPECT_CALL(*mock_view_,
              RequestShowContent(testing::ElementsAreArray(bank_accounts_)));

  // Verify that the `Show` returns true when the bridge is able to show the
  // bottom sheet.
  EXPECT_TRUE(controller_->Show(bank_accounts_, base::DoNothing()));
}

// Test Show method returns false when FacilitatedPaymentsBottomSheetBridge
// returns false.
TEST_F(FacilitatedPaymentsControllerTest, Show_BridgeWasNotAbleToShow) {
  ON_CALL(*mock_view_, RequestShowContent).WillByDefault(Return(false));

  // The bottom sheet could not be shown, verify that the view is informed about
  // this failure.
  EXPECT_CALL(*mock_view_,
              RequestShowContent(testing::ElementsAreArray(bank_accounts_)));
  EXPECT_CALL(*mock_view_, OnDismissed);

  // The call should return false when bridge fails to show a bottom sheet.
  EXPECT_FALSE(controller_->Show(bank_accounts_, base::DoNothing()));
}

// Test Show method returns false when there's no bank account.
TEST_F(FacilitatedPaymentsControllerTest, Show_NoBankAccounts) {
  EXPECT_CALL(*mock_view_, RequestShowContent).Times(0);

  // The call should return false when there's no bank account.
  EXPECT_FALSE(controller_->Show({}, base::DoNothing()));
}

// Test OnDismissed method.
TEST_F(FacilitatedPaymentsControllerTest, OnDismissed) {
  // Show the bottom sheet and set the user decision callback.
  base::MockCallback<base::OnceCallback<void(bool, int64_t)>>
      mock_on_user_decision_callback;
  ON_CALL(*mock_view_, RequestShowContent).WillByDefault(Return(true));
  controller_->Show(bank_accounts_, mock_on_user_decision_callback.Get());

  // Verify that dismissal event is forwarded to the view. Also verify that the
  // manager is informed of the diamissal via the callback.
  EXPECT_CALL(*mock_view_, OnDismissed);
  EXPECT_CALL(mock_on_user_decision_callback,
              Run(/*is_selected=*/false, /*selected_bank_account_id=*/-1L));

  controller_->OnDismissed(nullptr);
}

// Test onBankAccountSelected method.
TEST_F(FacilitatedPaymentsControllerTest, onBankAccountSelected) {
  base::MockCallback<base::OnceCallback<void(bool, int64_t)>>
      mock_on_user_decision_callback;
  ON_CALL(*mock_view_, RequestShowContent).WillByDefault(Return(true));

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
