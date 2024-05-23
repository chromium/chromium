// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_controller.h"

#include <memory>

#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_bottom_sheet_bridge.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace {

class MockFacilitatedPaymentsBottomSheetBridge
    : public payments::facilitated::FacilitatedPaymentsBottomSheetBridge {
 public:
  // The showResult parameter here stands for the expected result we can get
  // from FacilitatedPaymentsBottomSheetBridge::RequestShowContent.
  explicit MockFacilitatedPaymentsBottomSheetBridge(bool showResult) {
    ON_CALL(*this, RequestShowContent(_, _)).WillByDefault(Return(showResult));
  }
  ~MockFacilitatedPaymentsBottomSheetBridge() override = default;

  MOCK_METHOD(bool,
              RequestShowContent,
              (FacilitatedPaymentsController * controller,
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
};

// Test FacilitatedPaymentsController::Show
TEST_F(FacilitatedPaymentsControllerTest, Show) {
  mock_view_ = std::make_unique<MockFacilitatedPaymentsBottomSheetBridge>(
      /*showResult=*/true);

  EXPECT_CALL(*mock_view_, RequestShowContent(&controller_, _));

  // The first call should return true when no bottom sheet is shown yet.
  EXPECT_TRUE(controller_.Show(std::move(mock_view_), web_contents()));
  // The second call should return false because the bottom sheet is already
  // shown after the first call.
  EXPECT_FALSE(controller_.Show(std::move(mock_view_), web_contents()));
}

// Test FacilitatedPaymentsController::NotShow
TEST_F(FacilitatedPaymentsControllerTest, NotShow) {
  mock_view_ = std::make_unique<MockFacilitatedPaymentsBottomSheetBridge>(
      /*showResult=*/false);

  EXPECT_CALL(*mock_view_, RequestShowContent(&controller_, _));

  // The  call should return false when bridge fails to show a bottom sheet.
  EXPECT_FALSE(controller_.Show(std::move(mock_view_), web_contents()));
}
