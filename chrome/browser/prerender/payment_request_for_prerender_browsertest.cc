// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class PaymentRequestForPrerenderBrowserTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  PaymentRequestForPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &PaymentRequestForPrerenderBrowserTest::GetActiveWebContents,
            base::Unretained(this))) {}

  void SetUp() override {
    prerender_helper_.SetUp(https_server());
    PaymentRequestPlatformBrowserTestBase::SetUp();
  }

  // PaymentRequestTestObserver
  void OnAppListReady() override {
    is_app_list_ready_fired_ = true;
    PaymentRequestPlatformBrowserTestBase::OnAppListReady();
  }

 protected:
  bool is_app_list_ready_fired_ = false;
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(PaymentRequestForPrerenderBrowserTest,
                       ShowAfterPrerendered) {
  // Navigate to an initial page.
  NavigateTo("/payment_request_creator.html");

  // Start a prerender.
  GURL prerender_url =
      https_server()->GetURL("/payment_request_no_shipping_test.html");

  int prerender_id = prerender_helper_.AddPrerender(prerender_url);
  content::RenderFrameHost* prerender_render_frame_host =
      prerender_helper_.GetPrerenderedMainFrameHost(prerender_id);

  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);

  // Try to show PaymentRequest by JS.
  EXPECT_TRUE(content::ExecJs(prerender_render_frame_host,
                              "document.getElementById('buy').click();",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Run the loop to give the test a chance to fail if is_app_list_ready_fired_
  // is set to true too early.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(is_app_list_ready_fired_);

  // Activate the prerendered page.
  prerender_helper_.NavigatePrimaryPage(prerender_url);

  WaitForObservedEvent();
  EXPECT_TRUE(is_app_list_ready_fired_);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestForPrerenderBrowserTest,
                       AbortAfterPrerendered) {
  // Navigate to an initial page.
  NavigateTo("/payment_request_creator.html");

  // Start a prerender.
  auto prerender_url =
      https_server()->GetURL("/payment_request_abort_test.html");

  int prerender_id = prerender_helper_.AddPrerender(prerender_url);
  auto* prerender_render_frame_host =
      prerender_helper_.GetPrerenderedMainFrameHost(prerender_id);

  ResetEventWaiterForSingleEvent(TestEvent::kAbortCalled);

  // Try to show PaymentRequest by JS.
  EXPECT_TRUE(content::ExecJs(prerender_render_frame_host,
                              "document.getElementById('buy').click();",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  EXPECT_TRUE(content::ExecJs(prerender_render_frame_host,
                              "document.getElementById('abort').click();",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Activate the prerendered page.
  prerender_helper_.NavigatePrimaryPage(prerender_url);

  WaitForObservedEvent();

  ExpectBodyContains({R"(Aborted)"});
}

}  // namespace
}  // namespace payments
