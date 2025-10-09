// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/fingerprinting_protection_filter/interventions/common/interventions_features.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
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

// ServiceWorkerContextObserver that waits until any service worker is finished
// running. Useful for tests that tests for service worker lifecycles.
class ServiceWorkerVersionStopper
    : public content::ServiceWorkerContextObserver {
 public:
  explicit ServiceWorkerVersionStopper(content::ServiceWorkerContext* context) {
    DCHECK(context);
    scoped_observation_.Observe(context);
  }

  ServiceWorkerVersionStopper(const ServiceWorkerVersionStopper&) = delete;
  ServiceWorkerVersionStopper& operator=(const ServiceWorkerVersionStopper&) =
      delete;

  void StopAndWaitWorkerStoppedRunning() {
    base::RunLoop outer_loop;
    content::ServiceWorkerContext* context = scoped_observation_.GetSource();
    context->StopAllServiceWorkers(outer_loop.QuitClosure());
    outer_loop.Run();
    run_loop_.Run();
  }

 protected:
  // content::ServiceWorkerContextObserver:
  void OnVersionStoppedRunning(int64_t version_id) override {
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
  base::ScopedObservation<content::ServiceWorkerContext,
                          content::ServiceWorkerContextObserver>
      scoped_observation_{this};
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

  testing::AssertionResult RegisterServiceWorker(
      const content::ToRenderFrameHost& to_rfh) {
    auto* rfh = to_rfh.render_frame_host();
    if (rfh->GetLastCommittedURL().path() !=
        "/service_worker/create_service_worker.html") {
      return testing::AssertionFailure()
             << "Not in '/service_worker/create_service_worker.html'";
    }
    constexpr const char kRegisterSWScript[] = R"(
      register('/fingerprinting_protection/canvas_noise_token_sw.js'))";
    content::EvalJsResult js_result = content::EvalJs(rfh, kRegisterSWScript);
    if (!js_result.is_ok()) {
      return testing::AssertionFailure() << js_result.ExtractError();
    }

    std::string result = js_result.ExtractString();
    if (result != "DONE") {
      return testing::AssertionFailure() << result;
    }

    return testing::AssertionSuccess();
  }

  std::optional<blink::NoiseToken> GetRendererTokenFromServiceWorker(
      const content::ToRenderFrameHost& to_rfh) {
    constexpr const char kRetrieveToken[] = R"(
  new Promise(async (resolve) => {
    navigator.serviceWorker.addEventListener('message', event => {
      resolve(event.data);
    }, { once: true });

    const registration = await navigator.serviceWorker.getRegistration(
      '/fingerprinting_protection/');
    registration.active.postMessage('get-canvas-noise-token');
  });
  )";

    auto js_result = content::EvalJs(to_rfh, kRetrieveToken);
    return ParseTokenFromJsResult(js_result);
  }

  std::optional<blink::NoiseToken> GetRendererTokenFromSharedWorker(
      const content::ToRenderFrameHost& to_rfh) {
    constexpr const char kScript[] = R"(
  new Promise(resolve => {
    worker.port.addEventListener('message', (event) => {
      resolve(event.data);
    }, { once: true });

    worker.port.postMessage('get-canvas-noise-token');
  });
  )";
    auto js_result = content::EvalJs(to_rfh, kScript);
    return ParseTokenFromJsResult(js_result);
  }

  std::optional<blink::NoiseToken> GetRendererTokenFromWorker(
      const content::ToRenderFrameHost& to_rfh) {
    constexpr const char kScript[] = R"(
  new Promise(resolve => {
    worker.addEventListener('message', (event) => {
      resolve(event.data);
    }, { once: true });

    worker.postMessage('get-canvas-noise-token');
  });
  )";
    auto js_result = content::EvalJs(to_rfh, kScript);
    return ParseTokenFromJsResult(js_result);
  }

  content::ServiceWorkerContext* service_worker_context() {
    return web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition()
        ->GetServiceWorkerContext();
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

IN_PROC_BROWSER_TEST_P(CanvasInterventionsBrowserTest, ServiceWorkerSameToken) {
  const GURL url = embedded_https_test_server().GetURL(
      "a.com", "/service_worker/create_service_worker.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_TRUE(RegisterServiceWorker(web_contents()));

  if (should_browsing_mode_have_token()) {
    EXPECT_NE(GetRendererTokenFromServiceWorker(web_contents()), std::nullopt);
    // TODO(https://crbug.com/442616874): Change to EXPECT_EQ once we key canvas
    // noise tokens with StorageKey.
    EXPECT_NE(GetRendererTokenFromServiceWorker(web_contents()),
              GetBrowserTokenFromPage(web_contents()));
  } else {
    EXPECT_EQ(GetRendererTokenFromServiceWorker(web_contents()), std::nullopt);
  }
}

IN_PROC_BROWSER_TEST_P(CanvasInterventionsBrowserTest,
                       ServiceWorkerSubframeSameToken) {
  const GURL main_frame_url = embedded_https_test_server().GetURL(
      "a.com", "/service_worker/create_service_worker.html");
  const GURL iframe_url = embedded_https_test_server().GetURL(
      "b.com", "/service_worker/create_service_worker.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), main_frame_url));
  auto* iframe = CreateChildFrameAndNavigateToURL(web_contents(), iframe_url);

  // Register a Service Worker in both frames.
  ASSERT_TRUE(RegisterServiceWorker(web_contents()));
  auto main_frame_sw_token = GetRendererTokenFromServiceWorker(web_contents());

  ASSERT_TRUE(RegisterServiceWorker(iframe));
  auto iframe_sw_token = GetRendererTokenFromServiceWorker(iframe);

  if (should_browsing_mode_have_token()) {
    EXPECT_NE(iframe_sw_token, std::nullopt);
    EXPECT_EQ(iframe_sw_token, main_frame_sw_token);
    // TODO(https://crbug.com/442616874): Change to EXPECT_EQ once we key canvas
    // noise tokens with StorageKey.
    EXPECT_NE(iframe_sw_token, GetBrowserTokenFromPage(web_contents()));
  } else {
    EXPECT_EQ(iframe_sw_token, std::nullopt);
  }
}

IN_PROC_BROWSER_TEST_P(CanvasInterventionsBrowserTest,
                       ServiceWorkerSameOriginUpdatesSameToken) {
  const GURL url = embedded_https_test_server().GetURL(
      "a.com", "/service_worker/create_service_worker.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  auto* tracking_protection_settings =
      TrackingProtectionSettingsFactory::GetForProfile(GetBrowser()->profile());

  // Register a Service Worker in the main frame.
  ASSERT_TRUE(RegisterServiceWorker(web_contents()));
  auto sw_token = GetRendererTokenFromServiceWorker(web_contents());

  if (should_browsing_mode_have_token()) {
    EXPECT_NE(sw_token, std::nullopt);
    // TODO(https://crbug.com/442616874): Change to EXPECT_EQ once we key canvas
    // noise tokens with StorageKey.
    EXPECT_NE(sw_token, GetBrowserTokenFromPage(web_contents()));
  } else {
    EXPECT_EQ(sw_token, std::nullopt);
  }

  // Adding tracking protection settings for this url means we bypass canvas
  // noising, therefore the updated token should be changed to nullopt and
  // passed to the service worker.
  tracking_protection_settings->AddTrackingProtectionException(url);
  // Refresh the page to simulate user bypass behavior.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  auto flipped_sw_token = GetRendererTokenFromServiceWorker(web_contents());

  EXPECT_EQ(flipped_sw_token, std::nullopt);
  if (should_browsing_mode_have_token()) {
    EXPECT_NE(sw_token, flipped_sw_token);
  } else {
    // Adding/removing urls from TrackingProtectionSettings shouldn't do
    // anything.
    EXPECT_EQ(sw_token, flipped_sw_token);
  }

  // Now re-enable canvas noise for the top url.
  tracking_protection_settings->RemoveTrackingProtectionException(url);
  flipped_sw_token = GetRendererTokenFromServiceWorker(web_contents());

  if (should_browsing_mode_have_token()) {
    // TODO(https://crbug.com/442616874): Change to EXPECT_EQ once we key canvas
    // noise tokens with StorageKey.
    EXPECT_EQ(sw_token, flipped_sw_token);
    EXPECT_NE(flipped_sw_token, GetBrowserTokenFromPage(web_contents()));
    EXPECT_NE(flipped_sw_token, std::nullopt);
  } else {
    // Adding/removing urls from TrackingProtectionSettings shouldn't do
    // anything.
    EXPECT_EQ(sw_token, flipped_sw_token);
  }
}

IN_PROC_BROWSER_TEST_P(CanvasInterventionsBrowserTest,
                       ServiceWorkerSubframeUpdatesSameToken) {
  auto* tracking_protection_settings =
      TrackingProtectionSettingsFactory::GetForProfile(GetBrowser()->profile());
  const GURL main_frame_url = embedded_https_test_server().GetURL(
      "a.com", "/service_worker/create_service_worker.html");
  const GURL iframe_url = embedded_https_test_server().GetURL(
      "b.com", "/service_worker/create_service_worker.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), main_frame_url));
  auto* iframe = CreateChildFrameAndNavigateToURL(web_contents(), iframe_url);

  // Register a Service Worker in both frames.
  ASSERT_TRUE(RegisterServiceWorker(web_contents()));
  auto main_frame_sw_token = GetRendererTokenFromServiceWorker(web_contents());

  ASSERT_TRUE(RegisterServiceWorker(iframe));
  auto iframe_sw_token = GetRendererTokenFromServiceWorker(iframe);

  if (should_browsing_mode_have_token()) {
    EXPECT_NE(iframe_sw_token, std::nullopt);
    EXPECT_EQ(iframe_sw_token, main_frame_sw_token);
    // TODO(https://crbug.com/442616874): Change to EXPECT_EQ once we key canvas
    // noise tokens with StorageKey.
    EXPECT_NE(iframe_sw_token, GetBrowserTokenFromPage(web_contents()));
  } else {
    EXPECT_EQ(main_frame_sw_token, std::nullopt);
    EXPECT_EQ(iframe_sw_token, std::nullopt);
  }

  // Adding tracking protection settings for this url means we bypass canvas
  // noising, therefore the updated token should be changed to nullopt and
  // passed to the service worker.
  tracking_protection_settings->AddTrackingProtectionException(main_frame_url);
  // Refresh the page to simulate user bypass behavior.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), main_frame_url));
  iframe = CreateChildFrameAndNavigateToURL(web_contents(), iframe_url);

  auto flipped_main_frame_sw_token =
      GetRendererTokenFromServiceWorker(web_contents());
  auto flipped_iframe_sw_token = GetRendererTokenFromServiceWorker(iframe);
  EXPECT_EQ(flipped_main_frame_sw_token, std::nullopt);
  EXPECT_EQ(flipped_main_frame_sw_token, flipped_iframe_sw_token);

  if (should_browsing_mode_have_token()) {
    EXPECT_NE(flipped_main_frame_sw_token, main_frame_sw_token);
    EXPECT_NE(flipped_iframe_sw_token, iframe_sw_token);
  } else {
    // Adding/removing urls from TrackingProtectionSettings shouldn't do
    // anything.
    EXPECT_EQ(flipped_main_frame_sw_token, main_frame_sw_token);
    EXPECT_EQ(flipped_iframe_sw_token, iframe_sw_token);
  }

  // Now re-enable canvas noise for the top url.
  tracking_protection_settings->RemoveTrackingProtectionException(
      main_frame_url);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), main_frame_url));
  iframe = CreateChildFrameAndNavigateToURL(web_contents(), iframe_url);
  ASSERT_TRUE(iframe);

  flipped_main_frame_sw_token =
      GetRendererTokenFromServiceWorker(web_contents());
  flipped_iframe_sw_token = GetRendererTokenFromServiceWorker(iframe);

  if (should_browsing_mode_have_token()) {
    EXPECT_NE(flipped_main_frame_sw_token, std::nullopt);
  } else {
    // Adding/removing urls from TrackingProtectionSettings shouldn't do
    // anything.
    EXPECT_EQ(flipped_main_frame_sw_token, std::nullopt);
  }

  EXPECT_EQ(flipped_main_frame_sw_token, main_frame_sw_token);
  EXPECT_EQ(flipped_iframe_sw_token, iframe_sw_token);
  EXPECT_EQ(flipped_main_frame_sw_token, flipped_iframe_sw_token);
}

IN_PROC_BROWSER_TEST_P(CanvasInterventionsBrowserTest,
                       ServiceWorkerStoppedDoesNotUpdateToken) {
  const GURL url = embedded_https_test_server().GetURL(
      "a.com", "/service_worker/create_service_worker.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  auto* tracking_protection_settings =
      TrackingProtectionSettingsFactory::GetForProfile(GetBrowser()->profile());
  // Register a Service Worker in the main frame.
  ASSERT_TRUE(RegisterServiceWorker(web_contents()));
  auto sw_token = GetRendererTokenFromServiceWorker(web_contents());

  if (should_browsing_mode_have_token()) {
    EXPECT_NE(sw_token, std::nullopt);
    // TODO(https://crbug.com/442616874): Change to EXPECT_EQ once we key canvas
    // noise tokens with StorageKey.
    EXPECT_NE(sw_token, GetBrowserTokenFromPage(web_contents()));
  } else {
    EXPECT_EQ(sw_token, std::nullopt);
  }

  ServiceWorkerVersionStopper worker_stopper(service_worker_context());
  worker_stopper.StopAndWaitWorkerStoppedRunning();
  EXPECT_EQ(service_worker_context()->GetRunningServiceWorkerInfos().size(), 0);

  // This should not crash.
  tracking_protection_settings->AddTrackingProtectionException(url);
}

IN_PROC_BROWSER_TEST_P(CanvasInterventionsBrowserTest, SharedWorkerSameToken) {
  const GURL url = embedded_https_test_server().GetURL(
      "a.com",
      "/workers/create_shared_worker.html?worker_url=/"
      "fingerprinting_protection/canvas_noise_token_shared_worker.js");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  if (should_browsing_mode_have_token()) {
    EXPECT_NE(GetRendererTokenFromSharedWorker(web_contents()), std::nullopt);
    // TODO(https://crbug.com/442616874): change to EXPECT_EQ once we key canvas
    // noise tokens with StorageKey.
    EXPECT_NE(GetRendererTokenFromSharedWorker(web_contents()),
              GetBrowserTokenFromPage(web_contents()));
  } else {
    EXPECT_EQ(GetRendererTokenFromSharedWorker(web_contents()), std::nullopt);
  }
}

IN_PROC_BROWSER_TEST_P(CanvasInterventionsBrowserTest,
                       SharedWorkerDifferentTabSameToken) {
  const GURL url = embedded_https_test_server().GetURL(
      "a.com",
      "/workers/create_shared_worker.html?worker_url=/"
      "fingerprinting_protection/canvas_noise_token_shared_worker.js");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  content::WebContents* other_tab = chrome::AddSelectedTabWithURL(
      GetBrowser(), url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  EXPECT_TRUE(content::WaitForLoadStop(other_tab));

  auto first_tab_token = GetRendererTokenFromSharedWorker(web_contents());
  auto second_tab_token = GetRendererTokenFromSharedWorker(other_tab);

  if (should_browsing_mode_have_token()) {
    EXPECT_NE(first_tab_token, std::nullopt);
    // TODO(https://crbug.com/442616874): change to EXPECT_EQ once we key canvas
    // noise tokens with StorageKey.
    EXPECT_NE(first_tab_token, GetBrowserTokenFromPage(web_contents()));
  } else {
    EXPECT_EQ(first_tab_token, std::nullopt);
  }

  EXPECT_EQ(first_tab_token, second_tab_token);
}

IN_PROC_BROWSER_TEST_P(CanvasInterventionsBrowserTest,
                       SharedWorkerRegularAndIncognitoDifferentToken) {
  if (GetParam().browser_mode == BrowserMode::kIncognito) {
    GTEST_SKIP() << "This test tests both profiles";
  }

  const GURL url = embedded_https_test_server().GetURL(
      "a.com",
      "/workers/create_shared_worker.html?worker_url=/"
      "fingerprinting_protection/canvas_noise_token_shared_worker.js");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  auto browser_token = GetRendererTokenFromSharedWorker(web_contents());

  content::WebContents* incognito_contents =
      CreateIncognitoBrowser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(incognito_contents, url));

  auto incognito_browser_token =
      GetRendererTokenFromSharedWorker(incognito_contents);

  if (GetParam().feature_state == FeatureState::kDisabled) {
    EXPECT_EQ(browser_token, incognito_browser_token);
  } else {
    EXPECT_NE(browser_token, incognito_browser_token);
  }
}

IN_PROC_BROWSER_TEST_P(CanvasInterventionsBrowserTest,
                       SharedWorkerSubframeSameToken) {
  const GURL main_frame_url = embedded_https_test_server().GetURL(
      "a.com",
      "/workers/create_shared_worker.html?worker_url=/"
      "fingerprinting_protection/canvas_noise_token_shared_worker.js");
  const GURL iframe_url = embedded_https_test_server().GetURL(
      "b.com",
      "/workers/create_shared_worker.html?worker_url=/"
      "fingerprinting_protection/canvas_noise_token_shared_worker.js");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), main_frame_url));
  auto* iframe = CreateChildFrameAndNavigateToURL(web_contents(), iframe_url);

  auto main_frame_shw_token = GetRendererTokenFromSharedWorker(web_contents());
  auto iframe_shw_token = GetRendererTokenFromSharedWorker(iframe);

  if (should_browsing_mode_have_token()) {
    EXPECT_NE(iframe_shw_token, std::nullopt);
    // TODO(https://crbug.com/442616874): change to EXPECT_EQ once we key canvas
    // noise tokens with StorageKey.
    EXPECT_NE(iframe_shw_token, GetBrowserTokenFromPage(web_contents()));
  } else {
    EXPECT_EQ(iframe_shw_token, std::nullopt);
  }

  EXPECT_EQ(iframe_shw_token, main_frame_shw_token);
}

IN_PROC_BROWSER_TEST_P(CanvasInterventionsBrowserTest,
                       SharedWorkerSameOriginUpdatesSameToken) {
  auto* tracking_protection_settings =
      TrackingProtectionSettingsFactory::GetForProfile(GetBrowser()->profile());
  const GURL url = embedded_https_test_server().GetURL(
      "a.com",
      "/workers/create_shared_worker.html?worker_url=/"
      "fingerprinting_protection/canvas_noise_token_shared_worker.js");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  auto shw_token = GetRendererTokenFromSharedWorker(web_contents());

  if (should_browsing_mode_have_token()) {
    EXPECT_NE(shw_token, std::nullopt);
    // TODO(https://crbug.com/442616874): Change to EXPECT_EQ once we key canvas
    // noise tokens with StorageKey.
    EXPECT_NE(shw_token, GetBrowserTokenFromPage(web_contents()));
  } else {
    EXPECT_EQ(shw_token, std::nullopt);
  }

  // Adding tracking protection settings for this url means we bypass canvas
  // noising, therefore the updated token should be changed to nullopt and
  // passed to the shared worker.
  tracking_protection_settings->AddTrackingProtectionException(url);
  // Refresh the page to simulate user bypass behavior.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  auto flipped_shw_token = GetRendererTokenFromSharedWorker(web_contents());

  EXPECT_EQ(flipped_shw_token, std::nullopt);
  if (should_browsing_mode_have_token()) {
    EXPECT_NE(shw_token, flipped_shw_token);
  } else {
    // Adding/removing urls from TrackingProtectionSettings shouldn't do
    // anything.
    EXPECT_EQ(shw_token, flipped_shw_token);
  }

  // Now re-enable canvas noise for the top url.
  tracking_protection_settings->RemoveTrackingProtectionException(url);
  flipped_shw_token = GetRendererTokenFromSharedWorker(web_contents());

  if (should_browsing_mode_have_token()) {
    EXPECT_NE(flipped_shw_token, std::nullopt);
    // TODO(https://crbug.com/442616874): Change to EXPECT_EQ once we key canvas
    // noise tokens with StorageKey.
    EXPECT_NE(flipped_shw_token, GetBrowserTokenFromPage(web_contents()));
  }

  EXPECT_EQ(shw_token, flipped_shw_token);
}

IN_PROC_BROWSER_TEST_P(CanvasInterventionsBrowserTest,
                       SharedWorkerSubframeUpdatesSameToken) {
  auto* tracking_protection_settings =
      TrackingProtectionSettingsFactory::GetForProfile(GetBrowser()->profile());
  const GURL main_frame_url = embedded_https_test_server().GetURL(
      "a.com",
      "/workers/create_shared_worker.html?worker_url=/"
      "fingerprinting_protection/canvas_noise_token_shared_worker.js");
  const GURL iframe_url = embedded_https_test_server().GetURL(
      "b.com",
      "/workers/create_shared_worker.html?worker_url=/"
      "fingerprinting_protection/canvas_noise_token_shared_worker.js");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), main_frame_url));

  auto main_frame_shw_token = GetRendererTokenFromSharedWorker(web_contents());
  auto* iframe = CreateChildFrameAndNavigateToURL(web_contents(), iframe_url);
  auto iframe_shw_token = GetRendererTokenFromSharedWorker(iframe);

  EXPECT_EQ(iframe_shw_token, main_frame_shw_token);

  // Simulate user bypass behavior.
  tracking_protection_settings->AddTrackingProtectionException(main_frame_url);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), main_frame_url));
  iframe = CreateChildFrameAndNavigateToURL(web_contents(), iframe_url);

  auto flipped_main_frame_shw_token =
      GetRendererTokenFromSharedWorker(web_contents());
  auto flipped_iframe_shw_token = GetRendererTokenFromSharedWorker(iframe);
  EXPECT_EQ(flipped_main_frame_shw_token, std::nullopt);
  EXPECT_EQ(flipped_main_frame_shw_token, flipped_iframe_shw_token);

  if (should_browsing_mode_have_token()) {
    EXPECT_NE(flipped_main_frame_shw_token, main_frame_shw_token);
    EXPECT_NE(flipped_iframe_shw_token, iframe_shw_token);
  } else {
    // Adding/removing urls from TrackingProtectionSettings shouldn't do
    // anything.
    EXPECT_EQ(flipped_main_frame_shw_token, main_frame_shw_token);
    EXPECT_EQ(flipped_iframe_shw_token, iframe_shw_token);
  }

  // Simulate user bypass behavior to re-enable canvas noise for
  // |main_frame_url|.
  tracking_protection_settings->RemoveTrackingProtectionException(
      main_frame_url);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), main_frame_url));
  iframe = CreateChildFrameAndNavigateToURL(web_contents(), iframe_url);

  flipped_main_frame_shw_token =
      GetRendererTokenFromSharedWorker(web_contents());
  flipped_iframe_shw_token = GetRendererTokenFromSharedWorker(iframe);

  if (should_browsing_mode_have_token()) {
    EXPECT_NE(flipped_main_frame_shw_token, std::nullopt);
  } else {
    // Adding/removing urls from TrackingProtectionSettings shouldn't do
    // anything.
    EXPECT_EQ(flipped_main_frame_shw_token, std::nullopt);
  }

  EXPECT_EQ(flipped_main_frame_shw_token, main_frame_shw_token);
  EXPECT_EQ(flipped_iframe_shw_token, iframe_shw_token);
  EXPECT_EQ(flipped_main_frame_shw_token, flipped_iframe_shw_token);
}

IN_PROC_BROWSER_TEST_P(CanvasInterventionsBrowserTest,
                       SharedWorkerDifferentTabUpdatesSameToken) {
  auto* tracking_protection_settings =
      TrackingProtectionSettingsFactory::GetForProfile(GetBrowser()->profile());
  const GURL url = embedded_https_test_server().GetURL(
      "a.com",
      "/workers/create_shared_worker.html?worker_url=/"
      "fingerprinting_protection/canvas_noise_token_shared_worker.js");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  content::WebContents* other_tab = chrome::AddAndReturnTabAt(
      GetBrowser(), url, /*index=*/-1, /*foreground=*/true);
  EXPECT_TRUE(content::WaitForLoadStop(other_tab));

  auto first_tab_token = GetRendererTokenFromSharedWorker(web_contents());
  auto second_tab_token = GetRendererTokenFromSharedWorker(other_tab);

  EXPECT_EQ(first_tab_token, second_tab_token);

  // Simulate user bypass. However, the |other_tab| is not expected to refresh
  // automatically.
  tracking_protection_settings->AddTrackingProtectionException(url);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  auto flipped_first_tab_token =
      GetRendererTokenFromSharedWorker(web_contents());
  auto flipped_second_tab_token = GetRendererTokenFromSharedWorker(other_tab);

  if (should_browsing_mode_have_token()) {
    EXPECT_NE(flipped_first_tab_token, first_tab_token);
    EXPECT_NE(flipped_second_tab_token, second_tab_token);
  } else {
    EXPECT_EQ(flipped_first_tab_token, first_tab_token);
    EXPECT_EQ(flipped_second_tab_token, second_tab_token);
  }

  EXPECT_EQ(flipped_first_tab_token, flipped_second_tab_token);

  // Revert user bypass. However, the |other_tab| is not expected to refresh
  // automatically.
  tracking_protection_settings->RemoveTrackingProtectionException(url);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  flipped_first_tab_token = GetRendererTokenFromSharedWorker(web_contents());
  flipped_second_tab_token = GetRendererTokenFromSharedWorker(other_tab);

  EXPECT_EQ(flipped_first_tab_token, first_tab_token);
  EXPECT_EQ(flipped_second_tab_token, second_tab_token);
}

IN_PROC_BROWSER_TEST_P(CanvasInterventionsBrowserTest,
                       DedicatedWorkerSameToken) {
  const GURL url = embedded_https_test_server().GetURL(
      "a.com",
      "/workers/create_dedicated_worker.html?worker_url=/"
      "fingerprinting_protection/canvas_noise_token_worker.js");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  if (should_browsing_mode_have_token()) {
    EXPECT_NE(GetRendererTokenFromWorker(web_contents()), std::nullopt);
    EXPECT_EQ(GetRendererTokenFromWorker(web_contents()),
              GetBrowserTokenFromPage(web_contents()));
  } else {
    EXPECT_EQ(GetRendererTokenFromWorker(web_contents()), std::nullopt);
  }
}

IN_PROC_BROWSER_TEST_P(CanvasInterventionsBrowserTest,
                       DedicatedWorkerSubframeSameToken) {
  const GURL url = embedded_https_test_server().GetURL("a.com", "/iframe.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  auto* iframe = content::ChildFrameAt(web_contents(), 0);
  ASSERT_TRUE(iframe);

  const GURL iframe_url = embedded_https_test_server().GetURL(
      "b.com",
      "/workers/create_dedicated_worker.html?worker_url=/"
      "fingerprinting_protection/canvas_noise_token_worker.js");
  ASSERT_TRUE(content::NavigateToURLFromRenderer(iframe, iframe_url));

  iframe = content::ChildFrameAt(web_contents(), 0);
  ASSERT_TRUE(iframe);

  if (should_browsing_mode_have_token()) {
    EXPECT_NE(GetRendererTokenFromWorker(iframe), std::nullopt);
    EXPECT_EQ(GetRendererTokenFromWorker(iframe),
              GetBrowserTokenFromPage(web_contents()));
  } else {
    EXPECT_EQ(GetRendererTokenFromWorker(iframe), std::nullopt);
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
