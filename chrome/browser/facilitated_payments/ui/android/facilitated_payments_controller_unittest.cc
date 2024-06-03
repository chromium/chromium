// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_controller.h"

#include <memory>

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
  MockFacilitatedPaymentsBottomSheetBridge() = default;

  ~MockFacilitatedPaymentsBottomSheetBridge() override = default;

  MOCK_METHOD(bool,
              RequestShowContent,
              (base::span<const autofill::BankAccount> bank_account_suggestions,
               FacilitatedPaymentsController* controller,
               content::WebContents* web_contents),
              (override));
};

}  // namespace

class FacilitatedPaymentsControllerTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  FacilitatedPaymentsControllerTest() = default;
  ~FacilitatedPaymentsControllerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
  }

  void TearDown() override {
    mock_view_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<MockFacilitatedPaymentsBottomSheetBridge> mock_view_;
  FacilitatedPaymentsController controller_;
  const std::vector<autofill::BankAccount> bank_accounts_ = {
      autofill::test::CreatePixBankAccount(100L),
      autofill::test::CreatePixBankAccount(200L)};
};

// Test Show method returns true when FacilitatedPaymentsBottomSheetBridge
// is able to show.
TEST_F(FacilitatedPaymentsControllerTest, Show_BridgeWasAbleToShow) {
  mock_view_ = std::make_unique<MockFacilitatedPaymentsBottomSheetBridge>();
  ON_CALL(*mock_view_, RequestShowContent(_, _, _)).WillByDefault(Return(true));

  EXPECT_CALL(*mock_view_,
              RequestShowContent(testing::ElementsAreArray(bank_accounts_),
                                 &controller_, _));

  // The first call should return true when no bottom sheet is shown yet.
  EXPECT_TRUE(
      controller_.Show(std::move(mock_view_), bank_accounts_, web_contents()));
  // The second call should return false because the bottom sheet is already
  // shown after the first call.
  EXPECT_FALSE(
      controller_.Show(std::move(mock_view_), bank_accounts_, web_contents()));
}

// Test Show method returns false when FacilitatedPaymentsBottomSheetBridge
// returns false.
TEST_F(FacilitatedPaymentsControllerTest, Show_BridgeWasNotAbleToShow) {
  mock_view_ = std::make_unique<MockFacilitatedPaymentsBottomSheetBridge>();
  ON_CALL(*mock_view_, RequestShowContent(_, _, _))
      .WillByDefault(Return(false));

  EXPECT_CALL(*mock_view_,
              RequestShowContent(testing::ElementsAreArray(bank_accounts_),
                                 &controller_, _));

  // The  call should return false when bridge fails to show a bottom sheet.
  EXPECT_FALSE(
      controller_.Show(std::move(mock_view_), bank_accounts_, web_contents()));
}

// Test Show method returns false when there's no bank account.
TEST_F(FacilitatedPaymentsControllerTest, Show_NoBankAccounts) {
  mock_view_ = std::make_unique<MockFacilitatedPaymentsBottomSheetBridge>();

  EXPECT_CALL(*mock_view_, RequestShowContent).Times(0);

  // The call should return false when there's no bank account.
  EXPECT_FALSE(controller_.Show(std::move(mock_view_), {}, web_contents()));
}
