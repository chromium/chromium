// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_controller.h"

#include <memory>
#include <vector>

#include "base/android/jni_string.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_bottom_sheet_bridge.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_app_info_list.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_app_info_list.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_ui_utils.h"
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
  MOCK_METHOD(
      void,
      RequestShowContentForPaymentLink,
      (base::span<const autofill::Ewallet> ewallet_suggestions,
       std::unique_ptr<payments::facilitated::FacilitatedPaymentsAppInfoList>
           app_suggestions),
      (override));
  MOCK_METHOD(void, ShowProgressScreen, (), (override));
  MOCK_METHOD(void, ShowErrorScreen, (), (override));
  MOCK_METHOD(void, Dismiss, (), (override));
  MOCK_METHOD(void, OnDismissed, (), (override));
  MOCK_METHOD(void, ShowPixAccountLinkingPrompt, (), (override));
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
    apps_ = std::make_unique<
        payments::facilitated::MockFacilitatedPaymentsAppInfoList>();
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
  std::unique_ptr<payments::facilitated::MockFacilitatedPaymentsAppInfoList>
      apps_;
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

// Test onBankAccountSelected method.
TEST_F(FacilitatedPaymentsControllerTest, onBankAccountSelected) {
  base::MockCallback<base::OnceCallback<void(int64_t)>>
      mock_on_payment_account_selected;

  // view_ is assigned when the bottom sheet is shown.
  controller_->Show(bank_accounts_, mock_on_payment_account_selected.Get());

  // When bank account is selected, call back should be called with the
  // instrument id of the selected bank account.
  EXPECT_CALL(mock_on_payment_account_selected,
              Run(/*selected_bank_account_id=*/100L));

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

// Test controller forwards call for showing the Pix account linking prompt to
// the view.
TEST_F(FacilitatedPaymentsControllerTest, ShowPixAccountLinkingPrompt) {
  EXPECT_CALL(*mock_view_, ShowPixAccountLinkingPrompt);

  controller_->ShowPixAccountLinkingPrompt(base::DoNothing(),
                                           base::DoNothing());
}

TEST_F(FacilitatedPaymentsControllerTest, OnPixAccountLinkingPromptAccepted) {
  base::MockCallback<base::OnceCallback<void()>> mock_on_accepted;
  base::MockCallback<base::OnceCallback<void()>> mock_on_declined;
  controller_->ShowPixAccountLinkingPrompt(mock_on_accepted.Get(),
                                           mock_on_declined.Get());

  // When the Pix account linking prompt is accepted, callback should be called.
  EXPECT_CALL(mock_on_accepted, Run());
  EXPECT_CALL(mock_on_declined, Run).Times(0);

  controller_->OnPixAccountLinkingPromptAccepted(nullptr);
}

TEST_F(FacilitatedPaymentsControllerTest, OnPixAccountLinkingPromptDeclined) {
  base::MockCallback<base::OnceCallback<void()>> mock_on_accepted;
  base::MockCallback<base::OnceCallback<void()>> mock_on_declined;
  controller_->ShowPixAccountLinkingPrompt(mock_on_accepted.Get(),
                                           mock_on_declined.Get());

  // When the Pix account linking prompt is declined, callback should be called.
  EXPECT_CALL(mock_on_accepted, Run).Times(0);
  EXPECT_CALL(mock_on_declined, Run());

  controller_->OnPixAccountLinkingPromptDeclined(nullptr);
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
                    payments::facilitated::UiEvent::kScreenCouldNotBeShown,
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
  if (ui_event() == payments::facilitated::UiEvent::kScreenCouldNotBeShown ||
      ui_event() == payments::facilitated::UiEvent::kScreenClosedNotByUser ||
      ui_event() == payments::facilitated::UiEvent::kScreenClosedByUser) {
    // Verify that the screen closing event is communicated to the
    // view. The second OnDismissed call is triggered when the test
    // fixture destroys the `controller`.
    EXPECT_CALL(*mock_view_, OnDismissed).Times(2);
  }

  controller_->OnUiEvent(nullptr, static_cast<jint>(ui_event()));
}

// Test controller forwards call for showing the payment link FOP selector to
// the view when there are eWallets.
TEST_F(FacilitatedPaymentsControllerTest,
       ShowForPaymentLink_UserHasEwalletAccounts) {
  ON_CALL(*apps_, Size).WillByDefault(testing::Return(0));
  EXPECT_CALL(*mock_view_,
              RequestShowContentForPaymentLink(
                  testing::ElementsAreArray(ewallets_), testing::_));

  controller_->ShowForPaymentLink(ewallets_, std::move(apps_),
                                  base::DoNothing());
}

// Test controller forwards call for showing the payment link FOP selector to
// the view when there are payment apps.
TEST_F(FacilitatedPaymentsControllerTest,
       ShowForPaymentLink_UserHasPaymentApps) {
  EXPECT_CALL(*mock_view_,
              RequestShowContentForPaymentLink(testing::IsEmpty(), testing::_));
  EXPECT_CALL(*apps_, Size).WillOnce(testing::Return(2));
  controller_->ShowForPaymentLink({}, std::move(apps_), base::DoNothing());
}

// Test controller forwards call for showing the payment link FOP selector to
// the view when there are eWallets and payment apps.
TEST_F(FacilitatedPaymentsControllerTest,
       ShowForPaymentLink_UserHasEwalletAccountsAndPaymentApps) {
  ON_CALL(*apps_, Size).WillByDefault(testing::Return(2));
  EXPECT_CALL(*mock_view_,
              RequestShowContentForPaymentLink(
                  testing::ElementsAreArray(ewallets_), testing::_));

  controller_->ShowForPaymentLink(ewallets_, std::move(apps_),
                                  base::DoNothing());
}

// Test controller does not forward call for showing the payment link FOP
// selector to the view when there are no eWallet accounts and no payment apps.
TEST_F(FacilitatedPaymentsControllerTest,
       ShowForPaymentLink_UserHasNoEwalletAccounts) {
  EXPECT_CALL(*mock_view_, RequestShowContentForPaymentLink).Times(0);
  EXPECT_CALL(*apps_, Size).WillOnce(testing::Return(0));

  controller_->ShowForPaymentLink({}, std::move(apps_), base::DoNothing());
}

// Test OnEwalletSelected method.
TEST_F(FacilitatedPaymentsControllerTest, OnEwalletSelected) {
  base::MockCallback<
      base::OnceCallback<void(payments::facilitated::SelectedFopData)>>
      mock_on_fop_selected;

  // view_ is assigned when the bottom sheet is shown.
  controller_->ShowForPaymentLink(ewallets_, std::move(apps_),
                                  mock_on_fop_selected.Get());

  // When an eWallet is selected, call back should be called with the instrument
  // id of the selected eWallet.
  EXPECT_CALL(
      mock_on_fop_selected,
      Run(testing::AllOf(
          testing::Field(&payments::facilitated::SelectedFopData::fop_type,
                         payments::facilitated::FopType::kGPayInstrument),
          testing::Field(&payments::facilitated::SelectedFopData::instrument_id,
                         100L))));

  controller_->OnEwalletSelected(nullptr, 100L);
}

// Test OnPaymentAppSelected method.
TEST_F(FacilitatedPaymentsControllerTest, OnPaymentAppSelected) {
  base::MockCallback<
      base::OnceCallback<void(payments::facilitated::SelectedFopData)>>
      mock_on_fop_selected;
  const std::string package_name = "com.example.app";
  const std::string activity_name = "com.example.app.activity";

  ON_CALL(*apps_, Size).WillByDefault(testing::Return(1));

  // view_ is assigned when the bottom sheet is shown.
  controller_->ShowForPaymentLink({}, std::move(apps_),
                                  mock_on_fop_selected.Get());

  // When a payment app is selected, callback should be called with the package
  // name and activity name of the selected payment app.
  EXPECT_CALL(
      mock_on_fop_selected,
      Run(testing::AllOf(
          testing::Field(&payments::facilitated::SelectedFopData::fop_type,
                         payments::facilitated::FopType::kExternalPaymentApp),
          testing::Field(&payments::facilitated::SelectedFopData::package_name,
                         package_name),
          testing::Field(&payments::facilitated::SelectedFopData::activity_name,
                         activity_name))));

  JNIEnv* env = base::android::AttachCurrentThread();
  controller_->OnPaymentAppSelected(
      env, base::android::ConvertUTF8ToJavaString(env, package_name),
      base::android::ConvertUTF8ToJavaString(env, activity_name));
}
