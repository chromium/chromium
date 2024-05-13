// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_controller.h"

#include <memory>

#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_bottom_sheet_bridge.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace {

class MockFacilitatedPaymentsBottomSheetBridge
    : public payments::facilitated::FacilitatedPaymentsBottomSheetBridge {
 public:
  MockFacilitatedPaymentsBottomSheetBridge() = default;
  ~MockFacilitatedPaymentsBottomSheetBridge() override = default;

  MOCK_METHOD(bool,
              RequestShowContent,
              (content::WebContents * web_contents),
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
    mock_view_ = std::make_unique<MockFacilitatedPaymentsBottomSheetBridge>();
  }

  void TearDown() override {
    mock_view_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<MockFacilitatedPaymentsBottomSheetBridge> mock_view_;
  FacilitatedPaymentsController controller_;
};

TEST_F(FacilitatedPaymentsControllerTest, Show) {
  EXPECT_CALL(*mock_view_, RequestShowContent(::testing::_));

  controller_.Show(std::move(mock_view_), web_contents());
}
