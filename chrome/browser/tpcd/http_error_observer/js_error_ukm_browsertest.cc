// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"

constexpr char kHostA[] = "a.test";

namespace {
// Handles Favicon requests so they don't produce a 404 and augment error
// metrics during a test
std::unique_ptr<net::test_server::HttpResponse> HandleFaviconRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != "/favicon.ico") {
    return nullptr;
  }
  // The response doesn't have to be a valid favicon to avoid logging a
  // console error. Any 200 response will do.
  return std::make_unique<net::test_server::BasicHttpResponse>();
}
}  // namespace

class JSErrProcBrowserTest : public InProcessBrowserTest {
 public:
  JSErrProcBrowserTest() : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    https_server_.RegisterRequestHandler(
        base::BindRepeating(&HandleFaviconRequest));
    ASSERT_TRUE(https_server_.Start());
  }

  // Checks that the JavaScript error has been properly recorded in the metrics
  void CheckJSErrBreakageMetrics(ukm::TestAutoSetUkmRecorder& ukm_recorder,
                                 size_t size,
                                 size_t index,
                                 const base::Location& location = FROM_HERE) {
    auto entries = ukm_recorder.GetEntries(
        "ThirdPartyCookies.BreakageIndicator.UncaughtJSError", {"HasOccurred"});
    EXPECT_EQ(entries.size(), size)
        << "(expected at " << location.ToString() << ")";
    EXPECT_EQ(entries.at(index).metrics.at("HasOccurred"), 1)
        << "(expected at " << location.ToString() << ")";
  }

 private:
  net::EmbeddedTestServer https_server_;
};

// Test that no error is registered when JS with no errors is embedded
IN_PROC_BROWSER_TEST_F(JSErrProcBrowserTest, NoErr) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(kHostA, "/empty_script.html")));
  EXPECT_EQ(
      ukm_recorder
          .GetEntries("ThirdPartyCookies.BreakageIndicator.UncaughtJSError",
                      {"HasOccurred"})
          .size(),
      0u);
}

// Test that JS Error is registered on HTML page containing uncaught JS error
// TODO(b/338241225): Reenable once the UncaughtJSError event is being sent.
IN_PROC_BROWSER_TEST_F(JSErrProcBrowserTest, DISABLED_JSErr) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server()->GetURL(kHostA, "/uncaught_error_script.html")));
  CheckJSErrBreakageMetrics(
      /*ukm_recorder=*/ukm_recorder,
      /*size=*/1,
      /*index=*/0);
}

// Test that JS Error is registered on Iframe containing uncaught JS error
// Flaky: https://crbug.com/1503531.
IN_PROC_BROWSER_TEST_F(JSErrProcBrowserTest, DISABLED_IframeJSErr) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("b.test", "/iframe.html")));
  ASSERT_TRUE(NavigateIframeToURL(
      browser()->tab_strip_model()->GetActiveWebContents(), "test",
      https_server()->GetURL(kHostA, "/uncaught_error_script.html")));
  CheckJSErrBreakageMetrics(
      /*ukm_recorder=*/ukm_recorder,
      /*size=*/1,
      /*index=*/0);
}

// Test that no JS Error is registered on embedded script with handled errors
IN_PROC_BROWSER_TEST_F(JSErrProcBrowserTest, HandledJSErr) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(kHostA, "/handles_error_script.html")));
  EXPECT_EQ(
      ukm_recorder
          .GetEntries("ThirdPartyCookies.BreakageIndicator.UncaughtJSError",
                      {"HasOccurred"})
          .size(),
      0u);
}
