// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/payments/core/error_strings.h"
#include "components/payments/core/features.h"
#include "components/payments/core/journey_logger.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace payments {
namespace {

using PaymentHandlerUiBrowserTest = PaymentRequestPlatformBrowserTestBase;

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_OpenPaymentTwiceShouldBeSuccessful \
  DISABLED_OpenPaymentTwiceShouldBeSuccessful
#else
#define MAYBE_OpenPaymentTwiceShouldBeSuccessful \
  OpenPaymentTwiceShouldBeSuccessful
#endif

// Make sure a page can open the Payment UI multiple times.
IN_PROC_BROWSER_TEST_F(PaymentHandlerUiBrowserTest,
                       MAYBE_OpenPaymentTwiceShouldBeSuccessful) {
  NavigateTo("/maxpay.test/merchant.html");

  std::string payment_method;
  InstallPaymentApp("maxpay.test", "/maxpay.test/payment_handler_sw.js",
                    &payment_method);
  EXPECT_EQ("app_is_ready",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace(
                                "launchAndWaitUntilReady($1, $2)",
                                https_server()->GetURL(
                                    "maxpay.test",
                                    "/maxpay.test/payment_handler_window.html"),
                                payment_method)));
  EXPECT_TRUE(test_controller()->ClickPaymentHandlerCloseButton());
  EXPECT_EQ("User closed the Payment Request UI.",
            content::EvalJs(GetActiveWebContents(), "getResult()"));

  // The second time should be successful.
  EXPECT_EQ("app_is_ready",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace(
                                "launchAndWaitUntilReady($1, $2)",
                                https_server()->GetURL(
                                    "maxpay.test",
                                    "/maxpay.test/payment_handler_window.html"),
                                payment_method)));
  EXPECT_TRUE(test_controller()->ClickPaymentHandlerCloseButton());
  EXPECT_EQ("User closed the Payment Request UI.",
            content::EvalJs(GetActiveWebContents(), "getResult()"));
}

class PaymentHandlerRedirectCookieBrowserTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  PaymentHandlerRedirectCookieBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kPaymentHandlerDialogUseInitiatorInUrlLoad,
         features::kPaymentHandlerDialogUseFrameOriginAsInitiator},
        {});
  }

 protected:
  void SetUpOnMainThread() override {
    net::test_server::RegisterDefaultHandlers(https_server());
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Regression test for https://crbug.com/415848455: when a cross-origin iframe
// invokes `PaymentRequest`, the Payment Handler window load must use the
// invoking iframe's origin (not the top-level frame's origin) as
// `initiator_origin` so a 301 redirect back to the top-level site does not
// attach `SameSite=Strict` cookies.
IN_PROC_BROWSER_TEST_F(
    PaymentHandlerRedirectCookieBrowserTest,
    CrossOriginIframeExcludesSameSiteStrictCookiesOnRedirect) {
  NavigateTo("b.com",
             "/set-cookie?strict_cookie=1;SameSite=Strict;Secure"
             "&lax_cookie=1;SameSite=Lax;Secure");

  NavigateTo("b.com", "/payment_request_main.html");
  ASSERT_TRUE(
      content::ExecJs(GetActiveWebContents(),
                      "document.getElementById('test').allow = 'payment';"));
  GURL iframe_url = https_server()->GetURL("a.com", "/payment_handler.html");
  ASSERT_TRUE(
      content::NavigateIframeToURL(GetActiveWebContents(), "test", iframe_url));
  content::RenderFrameHost* iframe =
      content::ChildFrameAt(GetActiveWebContents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(iframe);
  ASSERT_NE(
      iframe->GetLastCommittedOrigin(),
      GetActiveWebContents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());

  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  GURL target_url = https_server()->GetURL("b.com", "/echoheader?Cookie");
  GURL redirect_url = https_server()->GetURL(
      "a.com", "/server-redirect-301?" + target_url.spec());

  content::WebContentsAddedObserver new_contents_observer;
  content::TestNavigationObserver nav_observer(target_url);
  nav_observer.StartWatchingNewWebContents();

  ASSERT_EQ("success",
            content::EvalJs(iframe, content::JsReplace(
                                        "launchWithoutWaitForResponse($1, $2)",
                                        method_name, redirect_url.spec())));

  content::WebContents* payment_handler_contents =
      new_contents_observer.GetWebContents();
  ASSERT_TRUE(payment_handler_contents);
  nav_observer.Wait();
  ASSERT_TRUE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(iframe->GetLastCommittedOrigin(),
            nav_observer.last_initiator_origin());
  EXPECT_EQ("lax_cookie=1", content::EvalJs(payment_handler_contents,
                                            "document.body.innerText"));
}

class PaymentHandlerRedirectCookieKillswitchBrowserTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  PaymentHandlerRedirectCookieKillswitchBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::
                                  kPaymentHandlerDialogUseInitiatorInUrlLoad},
        /*disabled_features=*/{
            features::kPaymentHandlerDialogUseFrameOriginAsInitiator});
  }

 protected:
  void SetUpOnMainThread() override {
    net::test_server::RegisterDefaultHandlers(https_server());
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that disabling `kPaymentHandlerDialogUseFrameOriginAsInitiator`
// falls back to using the top-level `WebContents` URL as `initiator_origin`
// while keeping `kPaymentHandlerDialogUseInitiatorInUrlLoad` enabled.
IN_PROC_BROWSER_TEST_F(PaymentHandlerRedirectCookieKillswitchBrowserTest,
                       DisabledKillswitchFallsBackToTopLevelOrigin) {
  NavigateTo("b.com", "/payment_request_main.html");
  ASSERT_TRUE(
      content::ExecJs(GetActiveWebContents(),
                      "document.getElementById('test').allow = 'payment';"));
  GURL iframe_url = https_server()->GetURL("a.com", "/payment_handler.html");
  ASSERT_TRUE(
      content::NavigateIframeToURL(GetActiveWebContents(), "test", iframe_url));
  content::RenderFrameHost* iframe =
      content::ChildFrameAt(GetActiveWebContents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(iframe);

  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  GURL app_url =
      https_server()->GetURL("a.com", "/payment_handler_window.html");
  content::TestNavigationObserver nav_observer(app_url);
  nav_observer.StartWatchingNewWebContents();

  ASSERT_EQ("success",
            content::EvalJs(iframe, content::JsReplace(
                                        "launchWithoutWaitForResponse($1, $2)",
                                        method_name, app_url.spec())));

  nav_observer.Wait();
  ASSERT_TRUE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(
      GetActiveWebContents()->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
      nav_observer.last_initiator_origin());
}

// Test that PaymentRequest allows one call to show() without a user activation.
class PaymentRequestActivationlessShowTest
    : public PaymentRequestPlatformBrowserTestBase {
 protected:
  PaymentRequestActivationlessShowTest() {
    SetBypassUserInteractionForTesting();
  }

  void ExpectEvent2(JourneyLogger::Event2 event, bool expected) {
    std::vector<base::Bucket> buckets =
        histogram_tester_.GetAllSamples("PaymentRequest.Events2");
    ASSERT_EQ(1U, buckets.size());
    EXPECT_EQ(expected, (buckets[0].min & static_cast<int>(event)) != 0);
  }

 private:
  base::HistogramTester histogram_tester_;
};

// TODO(crbug.com/41492121): Fix flakiness and re-enable.
IN_PROC_BROWSER_TEST_F(PaymentRequestActivationlessShowTest,
                       DISABLED_ActivationlessShow) {
  std::string payment_method =
      https_server()->GetURL("a.com", "/orenpay.test/pay").spec();
  NavigateTo("b.com", "/payment_handler_status.html");
  std::string show_js = content::JsReplace("getStatus($1)", payment_method);

  // The first call to show() without a user gesture succeeds.
  EXPECT_EQ(
      "success",
      content::EvalJs(GetActiveWebContents(), show_js,
                      content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));
  ExpectEvent2(JourneyLogger::Event2::kActivationlessShow, true);

  // A second call to show() without a user gesture gives an error.
  EXPECT_THAT(
      content::EvalJs(GetActiveWebContents(), show_js,
                      content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE)
          .ExtractString(),
      ::testing::HasSubstr(errors::kCannotShowWithoutUserActivation));

  // A following call to show() with a user gesture succeeds.
  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), show_js));
}

// Test that the JourneyLogger event for ActivationlessShow is not recorded when
// there is a user activation.
IN_PROC_BROWSER_TEST_F(PaymentRequestActivationlessShowTest,
                       EventNotRecordedForActivatedShow) {
  std::string payment_method =
      https_server()->GetURL("a.com", "/orenpay.test/pay").spec();
  NavigateTo("b.com", "/payment_handler_status.html");
  std::string show_js = content::JsReplace("getStatus($1)", payment_method);

  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), show_js));

  ExpectEvent2(JourneyLogger::Event2::kActivationlessShow, false);
}

}  // namespace
}  // namespace payments
