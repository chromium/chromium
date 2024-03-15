// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"

// Smoke test to detect if the code in third_party/blink/extensions/webview is
// accidentally turned on outside of WebView.
class WebViewRendererExtensionBrowserTest : public AndroidBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    AndroidBrowserTest::SetUpOnMainThread();

    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }
};

// The `window.android` interface is provided by a Blink renderer extension that
// adds Android WebView-specific APIs to the JavaScript execution environment.
// Because Android WebView and Chrome for Android may share the binary library,
// this test guards against the interface being exposed by accident in non-
// WebView environments on Android. Exposure on other Chrome platforms is
// monitored through Blink's regular webexposed test suite.
IN_PROC_BROWSER_TEST_F(WebViewRendererExtensionBrowserTest,
                       ExtensionNotAvailable) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), test_url));
  EXPECT_EQ(false, content::EvalJs(web_contents(), "'android' in window"));
}
