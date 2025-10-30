// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/fingerprinting_protection_filter/interventions/common/interventions_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/fingerprinting_protection/noise_token.h"
#include "url/gurl.h"

namespace {

// A browsertest that checks for canvas interventions behavior.
class CanvasInterventionsBrowserTest : public InProcessBrowserTest {
 public:
  CanvasInterventionsBrowserTest() {}

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_https_test_server().Start());
    browser_ = browser();
  }

  void TearDownOnMainThread() override {
    browser_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "CanvasInterventionsTest");
  }

 protected:
  content::WebContents* web_contents() const {
    return browser_->tab_strip_model()->GetActiveWebContents();
  }

  std::optional<blink::NoiseToken> GetRendererTokenFromJs(
      const content::ToRenderFrameHost& to_rfh) {
    content::EvalJsResult js_result =
        EvalJs(to_rfh, "CanvasInterventionsTest.getCanvasNoiseToken()");

    return ParseTokenFromJsResult(js_result);
  }

 private:
  std::optional<blink::NoiseToken> ParseTokenFromJsResult(
      const content::EvalJsResult& js_result) {
    CHECK(js_result.is_ok()) << js_result.ExtractError();
    if (js_result == base::Value()) {
      return std::nullopt;
    }

    uint64_t raw_token;
    CHECK(base::StringToUint64(js_result.ExtractString(), &raw_token));
    return blink::NoiseToken(raw_token);
  }

  raw_ptr<Browser> browser_ = nullptr;
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(CanvasInterventionsBrowserTest, MainFrame) {
  const GURL url(embedded_https_test_server().GetURL("a.com", "/empty.html"));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_EQ(GetRendererTokenFromJs(web_contents()), std::nullopt);
}

IN_PROC_BROWSER_TEST_F(CanvasInterventionsBrowserTest, Subframe) {
  const GURL url(embedded_https_test_server().GetURL("a.com", "/iframe.html"));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  auto* iframe = content::ChildFrameAt(web_contents(), 0);
  ASSERT_TRUE(iframe);

  EXPECT_EQ(GetRendererTokenFromJs(iframe), std::nullopt);
}
