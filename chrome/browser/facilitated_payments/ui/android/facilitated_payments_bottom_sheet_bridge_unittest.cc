// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_bottom_sheet_bridge.h"

#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {
namespace {

// TODO(b/337180783): Add the test for success when the flow is completed.
class FacilitatedPaymentsBottomSheetBridgeTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Create a scoped window so that
    // WebContents::GetNativeView()->GetWindowAndroid() does not return null.
    window_ = ui::WindowAndroid::CreateForTesting();
    window_.get()->get()->AddChild(web_contents()->GetNativeView());
  }

  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window_;
};

TEST_F(FacilitatedPaymentsBottomSheetBridgeTest, RequestShowContent) {
  FacilitatedPaymentsController controller(web_contents());
  FacilitatedPaymentsBottomSheetBridge bridge =
      FacilitatedPaymentsBottomSheetBridge(web_contents(), &controller);
  const std::vector<autofill::BankAccount> bank_accounts_ = {
      autofill::test::CreatePixBankAccount(100L),
      autofill::test::CreatePixBankAccount(200L)};

  // A Java BottomSheetController can't be initialized from the native side. So
  // no bottom sheet is shown.
  EXPECT_FALSE(bridge.RequestShowContent(bank_accounts_));
}

}  // namespace
}  // namespace payments::facilitated
