// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_bottom_sheet_bridge.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {
namespace {

using FacilitatedPaymentsBottomSheetBridgeTest =
    ChromeRenderViewHostTestHarness;

TEST_F(FacilitatedPaymentsBottomSheetBridgeTest, RequestShowContent) {
  FacilitatedPaymentsBottomSheetBridge bridge;

  bool did_show = bridge.RequestShowContent(web_contents());

  EXPECT_FALSE(did_show);
}

}  // namespace
}  // namespace payments::facilitated
