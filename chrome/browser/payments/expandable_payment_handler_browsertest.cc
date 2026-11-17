// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <string>

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/payments/core/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features_generated.h"

#if BUILDFLAG(IS_ANDROID)
#define DISABLE_ON_ANDROID(test_name) DISABLED_##test_name
#else
#define DISABLE_ON_ANDROID(test_name) test_name
#endif

namespace payments {
namespace {

class ExpandablePaymentHandlerBrowserTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  ExpandablePaymentHandlerBrowserTest()
      : http_server_(net::EmbeddedTestServer::TYPE_HTTP) {}

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
    NavigateTo("/maxpay.test/merchant.html");

    // Start http (not https!) server.
    http_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/");
    ASSERT_TRUE(http_server_.Start());
  }

  GURL GetHttpPageUrl() {
    return http_server_.GetURL("maxpay.test", "/maxpay.test/merchant.html");
  }

 private:
  net::EmbeddedTestServer http_server_;
};

// Make sure payment apps served from an http connection are rejected.
IN_PROC_BROWSER_TEST_F(ExpandablePaymentHandlerBrowserTest,
                       OpenWindowRejectHttp) {
  std::string payment_method;
  InstallPaymentApp("maxpay.test", "/maxpay.test/payment_handler_sw.js",
                    &payment_method);
  EXPECT_EQ("open_window_failed",
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace("launchAndWaitUntilReady($1, $2)",
                                   GetHttpPageUrl().spec(), payment_method)));
}

class ExpandableReadyPaymentHandlerBrowserTest
    : public ExpandablePaymentHandlerBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ExpandablePaymentHandlerBrowserTest::SetUpOnMainThread();
    std::string payment_method;
    InstallPaymentApp("maxpay.test", "/maxpay.test/payment_handler_sw.js",
                      &payment_method);
    EXPECT_EQ(
        "app_is_ready",
        content::EvalJs(
            GetActiveWebContents(),
            content::JsReplace(
                "launchAndWaitUntilReady($1, $2)",
                https_server()->GetURL(
                    "maxpay.test", "/maxpay.test/payment_handler_window.html"),
                payment_method)));
  }
};

// Make sure merchants can confirm the payment.
IN_PROC_BROWSER_TEST_F(ExpandableReadyPaymentHandlerBrowserTest,
                       ConfirmPayment) {
  DCHECK(test_controller()->GetPaymentHandlerWebContents());
  EXPECT_EQ("confirmed",
            content::EvalJs(test_controller()->GetPaymentHandlerWebContents(),
                            "confirm()"));
  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), "getResult()"));
}

// Make sure the security icon is clickable.
IN_PROC_BROWSER_TEST_F(ExpandableReadyPaymentHandlerBrowserTest,
                       DISABLE_ON_ANDROID(ClickSecurityIcon)) {
  DCHECK(test_controller()->GetPaymentHandlerWebContents());
  EXPECT_TRUE(test_controller()->ClickPaymentHandlerSecurityIcon());
}

// Make sure merchants can cancel the payment.
IN_PROC_BROWSER_TEST_F(ExpandableReadyPaymentHandlerBrowserTest,
                       CancelPayment) {
  DCHECK(test_controller()->GetPaymentHandlerWebContents());
  EXPECT_EQ("canceled",
            content::EvalJs(test_controller()->GetPaymentHandlerWebContents(),
                            "cancel()"));
  EXPECT_EQ("unknown", content::EvalJs(GetActiveWebContents(), "getResult()"));
}

// Make sure merchants can fail the payment.
IN_PROC_BROWSER_TEST_F(ExpandableReadyPaymentHandlerBrowserTest,
                       PaymentFailed) {
  DCHECK(test_controller()->GetPaymentHandlerWebContents());
  EXPECT_EQ("failed",
            content::EvalJs(test_controller()->GetPaymentHandlerWebContents(),
                            "fail()"));
  EXPECT_EQ("fail", content::EvalJs(GetActiveWebContents(), "getResult()"));
}

// Make sure openWindow() can be resolved into window client.
// Android: Flaky. See https://crbug.com/40687687.
IN_PROC_BROWSER_TEST_F(ExpandableReadyPaymentHandlerBrowserTest,
                       DISABLE_ON_ANDROID(WindowClientReady)) {
  DCHECK(test_controller()->GetPaymentHandlerWebContents());
  EXPECT_EQ(true,
            content::EvalJs(test_controller()->GetPaymentHandlerWebContents(),
                            "isWindowClientReady()"));
}

class ExpandablePaymentHandlerPreferredSizeBrowserTest
    : public ExpandablePaymentHandlerBrowserTest {
 public:
  ExpandablePaymentHandlerPreferredSizeBrowserTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kPaymentHandlerStaticCustomSize);
  }

 protected:
  // Installs the payment app, opens its window with `open_window_options` (a
  // JavaScript expression passed to PaymentRequestEvent.openWindow()), waits
  // until the window client is ready, and returns the payment handler
  // WebContents, or nullptr on failure.
  content::WebContents* OpenPaymentHandlerWindow(
      const std::string& open_window_options) {
    std::string payment_method;
    InstallPaymentApp("maxpay.test", "/maxpay.test/payment_handler_sw.js",
                      &payment_method);
    EXPECT_EQ(
        "app_is_ready",
        content::EvalJs(
            GetActiveWebContents(),
            content::JsReplace(
                "launchAndWaitUntilReady($1, $2, " + open_window_options + ")",
                https_server()->GetURL(
                    "maxpay.test", "/maxpay.test/payment_handler_window.html"),
                payment_method)));

    content::WebContents* payment_handler_web_contents =
        test_controller()->GetPaymentHandlerWebContents();
    if (!payment_handler_web_contents) {
      return nullptr;
    }
    EXPECT_EQ(true, content::EvalJs(payment_handler_web_contents,
                                    "waitUntilWindowClientReady()"));
    return payment_handler_web_contents;
  }

  // Confirms the payment in `payment_handler_web_contents` and checks that the
  // merchant receives the response.
  void ConfirmPayment(content::WebContents* payment_handler_web_contents) {
    EXPECT_EQ("confirmed",
              content::EvalJs(payment_handler_web_contents, "confirm()"));
    EXPECT_EQ("success",
              content::EvalJs(GetActiveWebContents(), "getResult()"));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Make sure openWindow({preferredSize}) opens the payment handler window and
// resolves into a window client. On Android, this takes the main path through
// payments::android::ShowPaymentHandlerWindow() instead of ServiceTabLauncher.
IN_PROC_BROWSER_TEST_F(ExpandablePaymentHandlerPreferredSizeBrowserTest,
                       OpenWindowWithPreferredSize) {
  content::WebContents* payment_handler_web_contents =
      OpenPaymentHandlerWindow("{preferredSize: {width: 300, height: 200}}");
  ASSERT_TRUE(payment_handler_web_contents);

  // The payment handler view is sized to the preferred height, in dp. The
  // fallback path ignores preferredSize and would size it to the sheet's full
  // height instead. Allow 1dp for rounding to physical pixels.
  EXPECT_NEAR(200, payment_handler_web_contents->GetContainerBounds().height(),
              1);

  ConfirmPayment(payment_handler_web_contents);
}

// Make sure a too small preferred height is raised to the minimum: 146 CSS px
// of web content, i.e. desktop's 196px minimum dialog height minus its 48px
// header and 2px progress bar.
IN_PROC_BROWSER_TEST_F(ExpandablePaymentHandlerPreferredSizeBrowserTest,
                       OpenWindowWithTooSmallPreferredSize) {
  content::WebContents* payment_handler_web_contents =
      OpenPaymentHandlerWindow("{preferredSize: {width: 300, height: 50}}");
  ASSERT_TRUE(payment_handler_web_contents);

  // Allow 1dp for rounding to physical pixels.
  EXPECT_NEAR(146, payment_handler_web_contents->GetContainerBounds().height(),
              1);

  ConfirmPayment(payment_handler_web_contents);
}

class ExpandablePaymentHandlerResizeToBrowserTest
    : public ExpandablePaymentHandlerPreferredSizeBrowserTest {
 public:
  ExpandablePaymentHandlerResizeToBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kPaymentHandlerDynamicCustomSize);
  }

 protected:
  // Calls window.resizeTo(`width`, `height`) in `payment_handler_web_contents`.
  // Returns whether the script ran. Use WaitForHeight() to wait for the result.
  bool ResizeTo(content::WebContents* payment_handler_web_contents,
                int width,
                int height) {
    return content::ExecJs(
        payment_handler_web_contents,
        content::JsReplace("resizeTo($1, $2)", width, height));
  }

  // Waits until the height of `payment_handler_web_contents` is within 1dp of
  // `expected_height`, allowing for rounding to physical pixels. Waiting for
  // the page's resize event isn't enough: the event can arrive before
  // GetContainerBounds() reflects the new height, e.g. while the sheet is
  // still animating to it.
  [[nodiscard]] bool WaitForHeight(
      content::WebContents* payment_handler_web_contents,
      int expected_height) {
    return base::test::RunUntil([&] {
      return std::abs(
                 payment_handler_web_contents->GetContainerBounds().height() -
                 expected_height) <= 1;
    });
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Make sure window.resizeTo() changes the height of the payment handler
// WebContents, repeatedly.
IN_PROC_BROWSER_TEST_F(ExpandablePaymentHandlerResizeToBrowserTest,
                       ResizeToRepeatedlyWhenOpenedWithPreferredSize) {
  content::WebContents* payment_handler_web_contents =
      OpenPaymentHandlerWindow("{preferredSize: {width: 300, height: 200}}");
  ASSERT_TRUE(payment_handler_web_contents);
  // Allow 1dp for rounding to physical pixels.
  EXPECT_NEAR(200, payment_handler_web_contents->GetContainerBounds().height(),
              1);

  ASSERT_TRUE(ResizeTo(payment_handler_web_contents, 400, 320));
  EXPECT_TRUE(WaitForHeight(payment_handler_web_contents, 320));

  ASSERT_TRUE(ResizeTo(payment_handler_web_contents, 250, 180));
  EXPECT_TRUE(WaitForHeight(payment_handler_web_contents, 180));
}

// Make sure a too small height is raised to the minimum, as for preferredSize.
IN_PROC_BROWSER_TEST_F(ExpandablePaymentHandlerResizeToBrowserTest,
                       ResizeToRaisedToMinimum) {
  content::WebContents* payment_handler_web_contents =
      OpenPaymentHandlerWindow("{preferredSize: {width: 300, height: 200}}");
  ASSERT_TRUE(payment_handler_web_contents);
  // Allow 1dp for rounding to physical pixels.
  EXPECT_NEAR(200, payment_handler_web_contents->GetContainerBounds().height(),
              1);

  ASSERT_TRUE(ResizeTo(payment_handler_web_contents, 300, 50));

  EXPECT_TRUE(WaitForHeight(payment_handler_web_contents, 146));
}

// Make sure window.resizeTo() also works when the window is opened without a
// preferredSize, i.e. with the default height.
IN_PROC_BROWSER_TEST_F(ExpandablePaymentHandlerResizeToBrowserTest,
                       ResizeToWhenOpenedWithoutPreferredSize) {
  content::WebContents* payment_handler_web_contents =
      OpenPaymentHandlerWindow("{}");
  ASSERT_TRUE(payment_handler_web_contents);
  // The default height is 550, or less if clamped to a small window, but never
  // as small as 200.
  EXPECT_GT(payment_handler_web_contents->GetContainerBounds().height(), 200);

  ASSERT_TRUE(ResizeTo(payment_handler_web_contents, 300, 200));
  EXPECT_TRUE(WaitForHeight(payment_handler_web_contents, 200));

  ConfirmPayment(payment_handler_web_contents);
}

class ExpandablePaymentHandlerResizeToDisabledBrowserTest
    : public ExpandablePaymentHandlerPreferredSizeBrowserTest {
 public:
  ExpandablePaymentHandlerResizeToDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(
        features::kPaymentHandlerDynamicCustomSize);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Make sure window.resizeTo() is ignored when PaymentHandlerDynamicCustomSize
// is disabled.
IN_PROC_BROWSER_TEST_F(ExpandablePaymentHandlerResizeToDisabledBrowserTest,
                       ResizeToIgnoredWhenFeatureDisabled) {
  content::WebContents* payment_handler_web_contents =
      OpenPaymentHandlerWindow("{preferredSize: {width: 300, height: 200}}");
  ASSERT_TRUE(payment_handler_web_contents);
  // Allow 1dp for rounding to physical pixels.
  EXPECT_NEAR(200, payment_handler_web_contents->GetContainerBounds().height(),
              1);

  // No resize event is expected, so wait for two frames instead.
  ASSERT_TRUE(content::ExecJs(payment_handler_web_contents,
                              "resizeTo(400, 320);"
                              "new Promise(r => requestAnimationFrame(() => "
                              "requestAnimationFrame(r)))"));

  EXPECT_NEAR(200, payment_handler_web_contents->GetContainerBounds().height(),
              1);
}

}  // namespace
}  // namespace payments
