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

// A browsertest that checks for canvas interventions behavior. This test suite
// is parameterized to run all of its tests under various configurations defined
// by the `TestConfiguration` struct.
//
// The `kTestConfigurations` array defines the full set of configurations. Each
// test case (defined using `IN_PROC_BROWSER_TEST_P`) will be executed for each
// configuration in this array. This allows developers to write a single test
// implementation and have it automatically verified across different feature
// states and browser modes.
//
// To add a new test, simply implement a new `IN_PROC_BROWSER_TEST_P` test body.
//
// A `TestConfiguration` consists of:
// - `FeatureState`: How the `kCanvasNoise` feature is configured.
//   - `kDisabled`: The feature is turned off.
//   - `kEnabled`: The feature is active, but noise is only applied in
//     incognito mode.
//   - `kEnabledInRegular`: The feature is active and noise is applied in both
//     regular and incognito modes.
// - `BrowserMode`: Whether the test runs in a regular or incognito browser.
// - `ShouldHaveToken`: The expected outcome, i.e., whether a canvas noise
//   token should be present.

enum class FeatureState {
  kDisabled,
  kEnabled,
  kEnabledInRegular,
};

enum class BrowserMode { kRegular, kIncognito };

using ShouldHaveToken = base::StrongAlias<class HasTokenTag, bool>;

struct TestConfiguration {
  FeatureState feature_state;
  BrowserMode browser_mode;
  ShouldHaveToken should_browsing_mode_have_token;
};

class CanvasInterventionsBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<TestConfiguration> {
 public:
  CanvasInterventionsBrowserTest() {
    const TestConfiguration& test_configuration = GetParam();
    switch (test_configuration.feature_state) {
      case FeatureState::kDisabled:
        feature_list_.InitAndDisableFeature(
            fingerprinting_protection_interventions::features::kCanvasNoise);
        break;
      case FeatureState::kEnabled:
        feature_list_.InitAndEnableFeature(
            fingerprinting_protection_interventions::features::kCanvasNoise);
        break;
      case FeatureState::kEnabledInRegular:
        feature_list_.InitAndEnableFeatureWithParameters(
            fingerprinting_protection_interventions::features::kCanvasNoise,
            {{"enable_in_regular_mode", "true"}});
        break;
    }
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_https_test_server().Start());

    switch (GetParam().browser_mode) {
      case BrowserMode::kRegular:
        browser_ = browser();
        break;
      case BrowserMode::kIncognito:
        browser_ = CreateIncognitoBrowser();
        break;
    }
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

  bool should_browsing_mode_have_token() {
    return GetParam().should_browsing_mode_have_token.value();
  }

  Browser* GetBrowser() const { return browser_; }

  content::RenderFrameHost* CreateChildFrameAndNavigateToURL(
      const content::ToRenderFrameHost& to_rfh,
      GURL iframe_url) {
    // This method assumes there are no existing iframes on the main frame.
    CHECK_EQ(0,
             EvalJs(to_rfh, "document.getElementsByTagName('iframe').length"));

    // TODO(https://crbug.com/449204853): Add a new html file that creates
    // service worker under an iframe to replace this logic.
    std::string script =
        "var iframe = document.createElement('iframe');"
        "document.body.appendChild(iframe);";
    CHECK(ExecJs(to_rfh, script));
    CHECK(WaitForLoadStop(web_contents()));

    auto* iframe = content::ChildFrameAt(to_rfh, 0);
    CHECK(content::NavigateToURLFromRenderer(iframe, iframe_url));
    iframe = content::ChildFrameAt(to_rfh, 0);
    CHECK(iframe);
    return iframe;
  }

  std::optional<blink::NoiseToken> GetBrowserTokenFromPage(
      const content::ToRenderFrameHost& to_rfh) {
    return content::GetCanvasNoiseTokenForPage(
        to_rfh.render_frame_host()->GetOutermostMainFrame()->GetPage());
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

constexpr const TestConfiguration kTestConfigurations[] = {
    {FeatureState::kDisabled, BrowserMode::kRegular, ShouldHaveToken(false)},
    {FeatureState::kDisabled, BrowserMode::kIncognito, ShouldHaveToken(false)},
    {FeatureState::kEnabled, BrowserMode::kRegular, ShouldHaveToken(false)},
    {FeatureState::kEnabled, BrowserMode::kIncognito, ShouldHaveToken(true)},
    {FeatureState::kEnabledInRegular, BrowserMode::kRegular,
     ShouldHaveToken(true)},
    {FeatureState::kEnabledInRegular, BrowserMode::kIncognito,
     ShouldHaveToken(true)}};

IN_PROC_BROWSER_TEST_P(CanvasInterventionsBrowserTest, MainFrame) {
  const GURL url(embedded_https_test_server().GetURL("a.com", "/empty.html"));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  EXPECT_EQ(GetRendererTokenFromJs(web_contents()),
            GetBrowserTokenFromPage(web_contents()));
  if (should_browsing_mode_have_token()) {
    EXPECT_NE(GetRendererTokenFromJs(web_contents()), std::nullopt);
    EXPECT_NE(GetBrowserTokenFromPage(web_contents()), std::nullopt);
  } else {
    EXPECT_EQ(GetRendererTokenFromJs(web_contents()), std::nullopt);
    EXPECT_EQ(GetBrowserTokenFromPage(web_contents()), std::nullopt);
  }
}

IN_PROC_BROWSER_TEST_P(CanvasInterventionsBrowserTest,
                       SubframeSameOriginSameToken) {
  const GURL url(embedded_https_test_server().GetURL("a.com", "/iframe.html"));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  auto* iframe = content::ChildFrameAt(web_contents(), 0);
  ASSERT_TRUE(iframe);

  EXPECT_EQ(GetRendererTokenFromJs(iframe), GetBrowserTokenFromPage(iframe));
  if (should_browsing_mode_have_token()) {
    EXPECT_NE(GetRendererTokenFromJs(iframe), std::nullopt);
    EXPECT_NE(GetBrowserTokenFromPage(iframe), std::nullopt);
  } else {
    EXPECT_EQ(GetRendererTokenFromJs(iframe), std::nullopt);
    EXPECT_EQ(GetBrowserTokenFromPage(iframe), std::nullopt);
  }
}

IN_PROC_BROWSER_TEST_P(CanvasInterventionsBrowserTest,
                       SubframeCrossOriginSameToken) {
  const GURL url(
      embedded_https_test_server().GetURL("a.com", "/iframe_cross_site.html"));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  auto* iframe = content::ChildFrameAt(web_contents(), 0);
  ASSERT_TRUE(iframe);

  EXPECT_EQ(GetRendererTokenFromJs(iframe), GetBrowserTokenFromPage(iframe));
  if (should_browsing_mode_have_token()) {
    EXPECT_NE(GetRendererTokenFromJs(iframe), std::nullopt);
    EXPECT_NE(GetBrowserTokenFromPage(iframe), std::nullopt);
  } else {
    EXPECT_EQ(GetRendererTokenFromJs(iframe), std::nullopt);
    EXPECT_EQ(GetBrowserTokenFromPage(iframe), std::nullopt);
  }
}

IN_PROC_BROWSER_TEST_P(CanvasInterventionsBrowserTest,
                       SubframeAboutBlankSameToken) {
  const GURL url =
      embedded_https_test_server().GetURL("a.com", "/iframe_about_blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  auto* iframe = content::ChildFrameAt(web_contents(), 0);
  ASSERT_TRUE(iframe);

  EXPECT_EQ(GetRendererTokenFromJs(iframe), GetBrowserTokenFromPage(iframe));
  if (should_browsing_mode_have_token()) {
    EXPECT_NE(GetRendererTokenFromJs(iframe), std::nullopt);
    EXPECT_NE(GetBrowserTokenFromPage(iframe), std::nullopt);
  } else {
    EXPECT_EQ(GetRendererTokenFromJs(iframe), std::nullopt);
    EXPECT_EQ(GetBrowserTokenFromPage(iframe), std::nullopt);
  }
}

IN_PROC_BROWSER_TEST_P(CanvasInterventionsBrowserTest,
                       WithinTabCrossOriginDifferentToken) {
  const GURL url_a =
      embedded_https_test_server().GetURL("a.com", "/empty.html");
  const GURL url_b =
      embedded_https_test_server().GetURL("b.com", "/empty.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_a));
  auto token_a = GetRendererTokenFromJs(web_contents());
  ASSERT_EQ(token_a.has_value(), should_browsing_mode_have_token());

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_b));
  auto token_b = GetRendererTokenFromJs(web_contents());
  ASSERT_EQ(token_b.has_value(), should_browsing_mode_have_token());

  if (should_browsing_mode_have_token()) {
    EXPECT_NE(token_a, token_b);
  }
}

IN_PROC_BROWSER_TEST_P(CanvasInterventionsBrowserTest,
                       WithinTabSameOriginSameToken) {
  const GURL url = embedded_https_test_server().GetURL("a.com", "/empty.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  auto token_a = GetRendererTokenFromJs(web_contents());
  ASSERT_EQ(token_a.has_value(), should_browsing_mode_have_token());

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  auto token_b = GetRendererTokenFromJs(web_contents());
  ASSERT_EQ(token_b.has_value(), should_browsing_mode_have_token());

  EXPECT_EQ(token_a, token_b);
}

IN_PROC_BROWSER_TEST_P(CanvasInterventionsBrowserTest,
                       AcrossTabsCrossOriginDifferentToken) {
  const GURL url_a =
      embedded_https_test_server().GetURL("a.com", "/empty.html");
  const GURL url_b =
      embedded_https_test_server().GetURL("b.com", "/empty.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_a));
  auto token_a = GetRendererTokenFromJs(web_contents());
  ASSERT_EQ(token_a.has_value(), should_browsing_mode_have_token());

  content::WebContents* new_tab = chrome::AddAndReturnTabAt(
      GetBrowser(), GURL(), /*index=*/-1, /*foreground=*/true);
  ASSERT_TRUE(content::NavigateToURL(new_tab, url_b));

  auto token_b = GetRendererTokenFromJs(new_tab);
  ASSERT_EQ(token_b.has_value(), should_browsing_mode_have_token());

  if (should_browsing_mode_have_token()) {
    EXPECT_NE(token_a, token_b);
  }
}

IN_PROC_BROWSER_TEST_P(CanvasInterventionsBrowserTest,
                       AcrossTabsSameOriginSameToken) {
  const GURL url = embedded_https_test_server().GetURL("a.com", "/empty.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  auto token_a = GetRendererTokenFromJs(web_contents());
  ASSERT_EQ(token_a.has_value(), should_browsing_mode_have_token());

  content::WebContents* new_tab = chrome::AddAndReturnTabAt(
      GetBrowser(), GURL(), /*index=*/-1, /*foreground=*/true);
  ASSERT_TRUE(content::NavigateToURL(new_tab, url));

  auto token_b = GetRendererTokenFromJs(new_tab);
  ASSERT_EQ(token_b.has_value(), should_browsing_mode_have_token());

  EXPECT_EQ(token_a, token_b);
}

IN_PROC_BROWSER_TEST_P(CanvasInterventionsBrowserTest,
                       RegularAndIncognitoDifferentToken) {
  if (GetParam().browser_mode == BrowserMode::kIncognito) {
    GTEST_SKIP() << "This test tests both profiles";
  }

  const GURL url = embedded_https_test_server().GetURL("a.com", "/empty.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  auto regular_token = GetRendererTokenFromJs(web_contents());
  ASSERT_EQ(regular_token.has_value(), should_browsing_mode_have_token());

  Browser* incognito_browser = CreateIncognitoBrowser();
  content::WebContents* incognito_web_contents =
      incognito_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(incognito_web_contents, url));

  auto incognito_token = GetRendererTokenFromJs(incognito_web_contents);

  if (GetParam().feature_state == FeatureState::kDisabled) {
    EXPECT_EQ(regular_token, incognito_token);
  } else {
    ASSERT_TRUE(incognito_token.has_value());
    EXPECT_NE(regular_token, incognito_token);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    CanvasInterventionsBrowserTest,
    testing::ValuesIn(kTestConfigurations),
    [](const testing::TestParamInfo<TestConfiguration>& info) {
      std::string name;
      switch (info.param.feature_state) {
        case FeatureState::kDisabled:
          name += "Disabled";
          break;
        case FeatureState::kEnabled:
          name += "Enabled";
          break;
        case FeatureState::kEnabledInRegular:
          name += "EnabledInRegular";
          break;
      }
      switch (info.param.browser_mode) {
        case BrowserMode::kRegular:
          name += "_Regular";
          break;
        case BrowserMode::kIncognito:
          name += "_Incognito";
          break;
      }
      return name;
    });
