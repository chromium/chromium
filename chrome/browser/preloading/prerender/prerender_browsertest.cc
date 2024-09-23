// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace {

namespace {

// Following definitions are equal to content::PrerenderFinalStatus.
constexpr int kFinalStatusActivated = 0;
constexpr int kFinalStatusInvalidSchemeNavigation = 6;
constexpr int kFinalStatusTriggerDestroyed = 16;
constexpr int kFinalStatusTabClosedWithoutUserGesture = 55;
constexpr int kFinalStatusCrossSiteNavigationInMainFrameNavigation = 64;

}  // namespace

using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;
using ukm::builders::Preloading_Attempt;
static const auto kMockElapsedTime =
    base::ScopedMockElapsedTimersForTest::kMockElapsedTime;

class PrerenderBrowserTest : public PlatformBrowserTest {
 public:
  PrerenderBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&PrerenderBrowserTest::GetActiveWebContents,
                                base::Unretained(this))) {
  }

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    prerender_helper_.RegisterServerRequestMonitor(&ssl_server_);
    PlatformBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(embedded_test_server()->Start());

    ssl_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    ssl_server_.ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(ssl_server_.Start());
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    ASSERT_TRUE(ssl_server_.ShutdownAndWaitUntilComplete());
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  GURL GetUrl(const std::string& path) {
    return ssl_server_.GetURL("a.test", path);
  }

  GURL GetSameSiteCrossOriginUrl(const std::string& path) {
    return ssl_server_.GetURL("b.a.test", path);
  }

  GURL GetCrossSiteUrl(const std::string& path) {
    return ssl_server_.GetURL("b.test", path);
  }

 protected:
  void TestPrerenderAndActivateInNewTab(const std::string& link_click_script,
                                        bool should_be_activated);

 private:
  content::test::PrerenderTestHelper prerender_helper_;
  net::test_server::EmbeddedTestServer ssl_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};
};

// An end-to-end test of prerendering and activating.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderAndActivate) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  // Start a prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/simple.html");
  prerender_helper().AddPrerender(prerender_url);

  // Activate.
  content::TestActivationManager activation_manager(GetActiveWebContents(),
                                                    prerender_url);
  ASSERT_TRUE(
      content::ExecJs(GetActiveWebContents()->GetPrimaryMainFrame(),
                      content::JsReplace("location = $1", prerender_url)));
  activation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(activation_manager.was_activated());

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      kFinalStatusActivated, 1);
}

void PrerenderBrowserTest::TestPrerenderAndActivateInNewTab(
    const std::string& link_click_script,
    bool should_be_activated) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("/prerender/simple_links.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  // Start a prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/prerender/empty.html");
  content::FrameTreeNodeId host_id =
      prerender_helper().AddPrerender(prerender_url,
                                      /*eagerness=*/std::nullopt, "_blank");
  EXPECT_TRUE(host_id);

  // Activate.
  EXPECT_TRUE(ExecJs(GetActiveWebContents(), link_click_script));

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      kFinalStatusActivated, should_be_activated ? 1 : 0);
}

// An end-to-end test of prerendering in a new tab and activating.
// Disabled on Android due to failures: https://crbug.com/355255740.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PrerenderAndActivate_InNewTab \
  DISABLED_PrerenderAndActivate_InNewTab
#else
#define MAYBE_PrerenderAndActivate_InNewTab PrerenderAndActivate_InNewTab
#endif
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MAYBE_PrerenderAndActivate_InNewTab) {
  TestPrerenderAndActivateInNewTab("clickSameSiteNewWindowLink();", true);
}

// Disabled on Android due to failures: https://crbug.com/355255740.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PrerenderAndActivate_InNewTab_Noopener \
  DISABLED_PrerenderAndActivate_InNewTab_Noopener
#else
#define MAYBE_PrerenderAndActivate_InNewTab_Noopener \
  PrerenderAndActivate_InNewTab_Noopener
#endif
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MAYBE_PrerenderAndActivate_InNewTab_Noopener) {
  TestPrerenderAndActivateInNewTab("clickSameSiteNewWindowWithNoopenerLink();",
                                   true);
}

// Prerendering in a new tab should not be activate for a new window with an
// opener.
// The test is flaky on android-12l-x64-dbg-tests: https://crbug.com/1490582.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PrerenderAndActivate_InNewTab_Opener \
  DISABLED_PrerenderAndActivate_InNewTab_Opener
#else
#define MAYBE_PrerenderAndActivate_InNewTab_Opener \
  PrerenderAndActivate_InNewTab_Opener
#endif  // #if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_X86)
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MAYBE_PrerenderAndActivate_InNewTab_Opener) {
  TestPrerenderAndActivateInNewTab("clickSameSiteNewWindowWithOpenerLink();",
                                   false);
}

// Prerendering in a new tab should not be activate for a current tab.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderAndActivate_InNewTab_CurrentTab) {
  TestPrerenderAndActivateInNewTab("clickSameSiteLink();", false);
}

// Tests main frame navigation on a prerendered page in a new tab.
// Disabled on Android due to failures: https://crbug.com/355255740.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_MainFrameNavigation_InNewTab DISABLED_MainFrameNavigation_InNewTab
#else
#define MAYBE_MainFrameNavigation_InNewTab MainFrameNavigation_InNewTab
#endif
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MAYBE_MainFrameNavigation_InNewTab) {
  base::HistogramTester histogram_tester;
  std::string link_click_script = "clickSameSiteNewWindowLink();";

  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("/prerender/simple_links.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  // Start a prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/prerender/empty.html");
  content::FrameTreeNodeId host_id = prerender_helper().AddPrerender(
      prerender_url, /*eagerness=*/std::nullopt, "_blank");
  EXPECT_TRUE(host_id);

  // Navigate a prerendered page to another page.
  GURL navigation_url =
      embedded_test_server()->GetURL("/prerender/empty.html?navigated");
  prerender_helper().NavigatePrerenderedPage(host_id, navigation_url);

  auto* prerender_web_contents =
      content::WebContents::FromFrameTreeNodeId(host_id);

  // Activate.
  content::test::PrerenderHostObserver prerender_observer(
      *prerender_web_contents, host_id);
  EXPECT_TRUE(ExecJs(GetActiveWebContents(), link_click_script));
  prerender_observer.WaitForActivation();
  EXPECT_EQ(prerender_web_contents->GetLastCommittedURL(), navigation_url);
  EXPECT_EQ(prerender_web_contents->GetVisibleURL(), navigation_url);

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      kFinalStatusActivated, 1);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderTriggeredByEmbedderAndActivate) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  GURL prerender_url = embedded_test_server()->GetURL("/simple.html");

  // Start embedder triggered prerendering.
  std::unique_ptr<content::PrerenderHandle> prerender_handle =
      GetActiveWebContents()->StartPrerendering(
          prerender_url, content::PreloadingTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          /*should_warm_up_compositor=*/false,
          content::PreloadingHoldbackStatus::kUnspecified,
          /*preloading_attempt=*/nullptr, /*url_match_predicate=*/{},
          /*prerender_navigation_handle_callback=*/{});
  EXPECT_TRUE(prerender_handle);
  content::test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *GetActiveWebContents(), prerender_url);

  // Activate.
  content::TestActivationManager activation_manager(GetActiveWebContents(),
                                                    prerender_url);
  // Simulate a browser-initiated navigation.
  GetActiveWebContents()->OpenURL(
      content::OpenURLParams(
          prerender_url, content::Referrer(),
          WindowOpenDisposition::CURRENT_TAB,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});
  activation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(activation_manager.was_activated());

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_DirectURLInput",
      kFinalStatusActivated, 1);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, EmbedderTrigger_ChromeUrl) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  GURL prerender_url("chrome://new-tab-page");
  ASSERT_FALSE(prerender_url.SchemeIsHTTPOrHTTPS());

  // Start embedder triggered prerendering.
  std::unique_ptr<content::PrerenderHandle> prerender_handle =
      GetActiveWebContents()->StartPrerendering(
          prerender_url, content::PreloadingTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          /*should_warm_up_compositor=*/false,
          content::PreloadingHoldbackStatus::kUnspecified,
          /*preloading_attempt=*/nullptr, /*url_match_predicate=*/{},
          /*prerender_navigation_handle_callback=*/{});
  EXPECT_FALSE(prerender_handle);

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_DirectURLInput",
      kFinalStatusInvalidSchemeNavigation, 1);
}

// Tests that UseCounter for SpeculationRules-triggered prerender is recorded.
// This cannot be tested in content/ as SpeculationHostImpl records the usage
// with ContentBrowserClient::LogWebFeatureForCurrentPage() that is not
// implemented in content/.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, UseCounter) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kSpeculationRulesPrerender, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kV8Document_Prerendering_AttributeGetter, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::
          kV8Document_Onprerenderingchange_AttributeSetter,
      0);
  histogram_tester.ExpectBucketCount("Blink.UseCounter.Features",
                                     blink::mojom::WebFeature::kPageVisits, 1);

  // Start a prerender. The API call should be recorded.
  GURL prerender_url = embedded_test_server()->GetURL("/simple.html");
  prerender_helper().AddPrerender(prerender_url);
  // kPageVisits should have been issued for kPageVisits already, but the value
  // hasn't been updated due to the update will be delayed until the activation
  // in the current design. The value is still expected to be one.
  // Please refer to crrev.com/c/3856942 for implementation details.
  histogram_tester.ExpectBucketCount("Blink.UseCounter.Features",
                                     blink::mojom::WebFeature::kPageVisits, 1);

  // Accessing related attributes should also be recorded.
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents()->GetPrimaryMainFrame(),
                              "const value = document.prerendering;"));
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents()->GetPrimaryMainFrame(),
                              "document.onprerenderingchange = e => {};"));

  // Make sure the counts are stored by navigating away.
  prerender_helper().NavigatePrimaryPage(prerender_url);

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kSpeculationRulesPrerender, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kV8Document_Prerendering_AttributeGetter, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::
          kV8Document_Onprerenderingchange_AttributeSetter,
      1);
  histogram_tester.ExpectBucketCount("Blink.UseCounter.Features",
                                     blink::mojom::WebFeature::kPageVisits, 2);
}

// Tests that Prerender2 cannot be triggered when preload setting is disabled.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DisableNetworkPrediction) {
  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  // Disable network prediction.
  PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();
  prefetch::SetPreloadPagesState(prefs,
                                 prefetch::PreloadPagesState::kNoPreloading);
  ASSERT_EQ(prefetch::IsSomePreloadingEnabled(*prefs),
            content::PreloadingEligibility::kPreloadingDisabled);

  // Attempt to trigger prerendering.
  GURL prerender_url = embedded_test_server()->GetURL("/simple.html?1");
  prerender_helper().AddPrerenderAsync(prerender_url);
  // Since preload setting is disabled, prerender shouldn't be triggered.
  base::RunLoop().RunUntilIdle();
  content::FrameTreeNodeId host_id =
      prerender_helper().GetHostForUrl(prerender_url);
  EXPECT_TRUE(host_id.is_null());

  // Reload the initial page to reset the speculation rules.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  // Re-enable the setting.
  prefetch::SetPreloadPagesState(
      prefs, prefetch::PreloadPagesState::kStandardPreloading);
  ASSERT_EQ(prefetch::IsSomePreloadingEnabled(*prefs),
            content::PreloadingEligibility::kEligible);

  // Attempt to trigger prerendering again.
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());
  prerender_helper().AddPrerenderAsync(prerender_url);
  // Since preload setting is enabled, prerender should be triggered
  // successfully.
  registry_observer.WaitForTrigger(prerender_url);
  host_id = prerender_helper().GetHostForUrl(prerender_url);
  EXPECT_TRUE(host_id);
}

// Tests that DevTools open overrides PreloadingConfig's holdback.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PreloadingHoldbackOverridden) {
  prerender_helper().SetHoldback("Prerender", "SpeculationRules", true);
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));
  PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();

  // IsSomePreloadingEnabled is *not* affected by PreloadingConfig.
  ASSERT_EQ(prefetch::IsSomePreloadingEnabled(*prefs),
            content::PreloadingEligibility::kEligible);

  // Emulating Devtools attached to make PreloadingHoldback overridden. Retain
  // the returned host until the test finishes to avoid DevTools termination.
  scoped_refptr<content::DevToolsAgentHost> dev_tools_agent_host =
      content::DevToolsAgentHost::GetOrCreateFor(GetActiveWebContents());
  ASSERT_TRUE(dev_tools_agent_host);

  // Start a prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/simple.html");
  prerender_helper().AddPrerender(prerender_url);

  // Activate.
  content::TestActivationManager activation_manager(GetActiveWebContents(),
                                                    prerender_url);
  ASSERT_TRUE(
      content::ExecJs(GetActiveWebContents()->GetPrimaryMainFrame(),
                      content::JsReplace("location = $1", prerender_url)));
  activation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(activation_manager.was_activated());

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      kFinalStatusActivated, 1);
}

// Tests that Prerender2 cannot be triggered when PreloadingConfig's
// holdback is not overridden by DevTools.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PreloadingHoldbackNotOverridden) {
  prerender_helper().SetHoldback("Prerender", "SpeculationRules", true);

  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();

  // IsSomePreloadingEnabled is *not* affected by PreloadingConfig.
  ASSERT_EQ(prefetch::IsSomePreloadingEnabled(*prefs),
            content::PreloadingEligibility::kEligible);

  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  // Attempt to trigger prerendering.
  GURL prerender_url = embedded_test_server()->GetURL("/simple.html?1");
  prerender_helper().AddPrerenderAsync(prerender_url);
  // Since preload setting is disabled, prerender shouldn't be triggered.
  registry_observer.WaitForTrigger(prerender_url);
  content::FrameTreeNodeId host_id =
      prerender_helper().GetHostForUrl(prerender_url);
  EXPECT_TRUE(host_id.is_null());
}

// Tests that the same-origin main frame navigation in an embedder triggered
// prerendering page succeeds.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, SameOriginMainFrameNavigation) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  GURL url = GetUrl("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  GURL prerender_url = GetUrl("/title1.html");
  GURL navigation_url = GetUrl("/title2.html");

  // Start an embedder triggered prerendering.
  std::unique_ptr<content::PrerenderHandle> prerender_handle =
      GetActiveWebContents()->StartPrerendering(
          prerender_url, content::PreloadingTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          /*should_warm_up_compositor=*/false,
          content::PreloadingHoldbackStatus::kUnspecified,
          /*preloading_attempt=*/nullptr, /*url_match_predicate=*/{},
          /*prerender_navigation_handle_callback=*/{});
  EXPECT_TRUE(prerender_handle);
  content::test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *GetActiveWebContents(), prerender_url);

  content::FrameTreeNodeId host_id =
      prerender_helper().GetHostForUrl(prerender_url);
  ASSERT_TRUE(host_id);

  // Start a same-origin navigation in the prerender frame tree. It will not
  // cancel the initiator's prerendering.
  prerender_helper().NavigatePrerenderedPage(host_id, navigation_url);

  // Activate.
  content::TestActivationManager activation_manager(GetActiveWebContents(),
                                                    prerender_url);
  // Simulate a browser-initiated navigation.
  GetActiveWebContents()->OpenURL(
      content::OpenURLParams(
          prerender_url, content::Referrer(),
          WindowOpenDisposition::CURRENT_TAB,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});
  activation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(activation_manager.was_activated());

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_DirectURLInput",
      kFinalStatusActivated, 1);
}

// Tests that the same-site cross-origin main frame navigation in an embedder
// triggered prerendering page succeeds.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       SameSiteCrossOriginMainFrameNavigation) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  GURL url = GetUrl("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  GURL prerender_url = GetUrl("/title1.html");
  GURL navigation_url =
      GetSameSiteCrossOriginUrl("/prerender_with_opt_in_header.html");

  // Start an embedder triggered prerendering.
  std::unique_ptr<content::PrerenderHandle> prerender_handle =
      GetActiveWebContents()->StartPrerendering(
          prerender_url, content::PreloadingTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          /*should_warm_up_compositor=*/false,
          content::PreloadingHoldbackStatus::kUnspecified,
          /*preloading_attempt=*/nullptr, /*url_match_predicate=*/{},
          /*prerender_navigation_handle_callback=*/{});
  EXPECT_TRUE(prerender_handle);
  content::test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *GetActiveWebContents(), prerender_url);

  content::FrameTreeNodeId host_id =
      prerender_helper().GetHostForUrl(prerender_url);
  ASSERT_TRUE(host_id);

  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), host_id);

  // Start a same-site cross-origin main frame navigation in the prerender frame
  // tree. It will not cancel the initiator's prerendering.
  prerender_helper().NavigatePrerenderedPage(host_id, navigation_url);

  // Activate.
  content::TestActivationManager activation_manager(GetActiveWebContents(),
                                                    prerender_url);
  // Simulate a browser-initiated navigation.
  GetActiveWebContents()->OpenURL(
      content::OpenURLParams(
          prerender_url, content::Referrer(),
          WindowOpenDisposition::CURRENT_TAB,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});
  activation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(activation_manager.was_activated());

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_DirectURLInput",
      kFinalStatusActivated, 1);
}

// Tests that the cross-site main frame navigation in an embedder triggered
// prerendering page cancels the prerendering.
IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    CrossSiteMainFrameNavigationCancelsEmbedderTriggeredPrerendering) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  GURL url = GetUrl("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  GURL prerender_url = GetUrl("/title1.html");
  GURL navigation_url = GetCrossSiteUrl("/prerender_with_opt_in_header.html");

  // Start an embedder triggered prerendering.
  std::unique_ptr<content::PrerenderHandle> prerender_handle =
      GetActiveWebContents()->StartPrerendering(
          prerender_url, content::PreloadingTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          /*should_warm_up_compositor=*/false,
          content::PreloadingHoldbackStatus::kUnspecified,
          /*preloading_attempt=*/nullptr, /*url_match_predicate=*/{},
          /*prerender_navigation_handle_callback=*/{});
  EXPECT_TRUE(prerender_handle);
  content::test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *GetActiveWebContents(), prerender_url);

  content::FrameTreeNodeId host_id =
      prerender_helper().GetHostForUrl(prerender_url);
  ASSERT_TRUE(host_id);

  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), host_id);

  // Start a cross-site main frame navigation in the prerender frame tree. It
  // will cancel the initiator's prerendering.
  prerender_helper().NavigatePrerenderedPage(host_id, navigation_url);

  prerender_observer.WaitForDestroyed();

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_DirectURLInput",
      kFinalStatusCrossSiteNavigationInMainFrameNavigation, 1);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderWebContentsDelegate_CloseContents) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("/prerender/simple_links.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  // Start a prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/prerender/empty.html");
  content::FrameTreeNodeId host_id = prerender_helper().AddPrerender(
      prerender_url, /*eagerness=*/std::nullopt, "_blank");

  // Navigate a prerendered page to another page.
  GURL navigation_url =
      embedded_test_server()->GetURL("/prerender/empty.html?navigated");
  prerender_helper().NavigatePrerenderedPage(host_id, navigation_url);

  // WebContents::Close() should eventually call
  // PrerenderWebContentsDelegate::CloseContents() that cancels prerendering.
  auto* prerender_web_contents =
      content::WebContents::FromFrameTreeNodeId(host_id);
  ASSERT_TRUE(prerender_web_contents);
  content::WebContentsDestroyedWatcher destroyed_watcher(
      prerender_web_contents);
  prerender_web_contents->Close();

  // WebContents created for the new-tab host will eventually be destroyed after
  // host cancellation.
  destroyed_watcher.Wait();
  EXPECT_FALSE(prerender_helper().HasNewTabHandle(host_id));

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      kFinalStatusTabClosedWithoutUserGesture, 1);
}

class PrerenderNewTabPageBrowserTest
    : public PrerenderBrowserTest,
      public testing::WithParamInterface<content::PreloadingPredictor> {
 public:
  PrerenderNewTabPageBrowserTest() = default;

  void SetUpOnMainThread() override {
    PrerenderBrowserTest::SetUpOnMainThread();
    // Initialize PreloadingAttempt builder for the test suite.
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    attempt_entry_builder_ =
        std::make_unique<content::test::PreloadingAttemptUkmEntryBuilder>(
            GetParam());
    test_timer_ = std::make_unique<base::ScopedMockElapsedTimersForTest>();
  }

  void SimulateNewTabNavigation(const GURL& url) {
    GetActiveWebContents()->OpenURL(
        content::OpenURLParams(
            url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
            ui::PageTransitionFromInt(ui::PAGE_TRANSITION_AUTO_BOOKMARK),
            /*is_renderer_initiated=*/false),
        base::BindRepeating(&page_load_metrics::NavigationHandleUserData::
                                AttachNewTabPageNavigationHandleUserData));
  }

  void ExpectPrerenderPageLoad(
      const GURL& prerender_url,
      page_load_metrics::NavigationHandleUserData::InitiatorLocation
          initiator_location) {
    auto entries =
        test_ukm_recorder()->GetMergedEntriesByName("PrerenderPageLoad");
    for (auto& kv : entries) {
      const ukm::mojom::UkmEntry* entry = kv.second.get();
      const ukm::UkmSource* source =
          test_ukm_recorder()->GetSourceForSourceId(entry->source_id);
      if (!source) {
        continue;
      }
      EXPECT_TRUE(source->url().is_valid());
      if (source->url() != prerender_url) {
        continue;
      }
      test_ukm_recorder()->ExpectEntryMetric(
          entry,
          ukm::builders::PrerenderPageLoad::kNavigation_InitiatorLocationName,
          static_cast<int>(initiator_location));
      return;
    }
    EXPECT_TRUE(false) << "PrerenderPageLoad hasn't been recorded.";
  }

  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

  const content::test::PreloadingAttemptUkmEntryBuilder&
  attempt_entry_builder() {
    return *attempt_entry_builder_;
  }

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  std::unique_ptr<content::test::PreloadingAttemptUkmEntryBuilder>
      attempt_entry_builder_;
  // This timer is for making TimeToNextNavigation in UKM consistent.
  std::unique_ptr<base::ScopedMockElapsedTimersForTest> test_timer_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PrerenderNewTabPageBrowserTest,
    testing::Values(chrome_preloading_predictor::kMouseHoverOnNewTabPage,
                    chrome_preloading_predictor::kPointerDownOnNewTabPage));

IN_PROC_BROWSER_TEST_P(PrerenderNewTabPageBrowserTest,
                       PrerenderTriggeredByNewTabPageAndActivate) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(chrome::kChromeUINewTabURL)));
  GURL prerender_url = GetUrl("/simple.html");

  PrerenderManager::CreateForWebContents(GetActiveWebContents());
  auto* prerender_manager =
      PrerenderManager::FromWebContents(GetActiveWebContents());
  EXPECT_TRUE(
      prerender_manager->StartPrerenderNewTabPage(prerender_url, GetParam()));
  content::test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *GetActiveWebContents(), prerender_url);

  // Activate.
  content::TestActivationManager activation_manager(GetActiveWebContents(),
                                                    prerender_url);
  SimulateNewTabNavigation(prerender_url);
  activation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(activation_manager.was_activated());

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_NewTabPage",
      kFinalStatusActivated, 1);
  histogram_tester.ExpectTotalCount(
      "NewTabPage.PrerenderNavigationToActivation", 1);

  ExpectPrerenderPageLoad(prerender_url,
                          page_load_metrics::NavigationHandleUserData::
                              InitiatorLocation::kNewTabPage);
}

// This test verifies that a NTP mouse hover trigger followed a NTP mouse down
// trigger can be activated normally.
IN_PROC_BROWSER_TEST_P(
    PrerenderNewTabPageBrowserTest,
    PrerenderTriggeredByNewTabPageMouseDownAfterHoverAndActivate) {
  base::HistogramTester histogram_tester;
  // This test only verifies the scenario where a mouse hover triggers
  // followed by a mouse down trigger.
  if (GetParam() != chrome_preloading_predictor::kPointerDownOnNewTabPage) {
    return;
  }

  // Navigate to an initial page.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(chrome::kChromeUINewTabURL)));
  GURL prerender_url = GetUrl("/simple.html");

  PrerenderManager::CreateForWebContents(GetActiveWebContents());
  auto* prerender_manager =
      PrerenderManager::FromWebContents(GetActiveWebContents());
  // Start a mouse hover New Tab Page first.
  base::WeakPtr<content::PrerenderHandle> handle1 =
      prerender_manager->StartPrerenderNewTabPage(
          prerender_url, chrome_preloading_predictor::kMouseHoverOnNewTabPage);
  content::test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *GetActiveWebContents(), prerender_url);
  // Then start a mouse down New Tab Page for the same page.
  base::WeakPtr<content::PrerenderHandle> handle2 =
      prerender_manager->StartPrerenderNewTabPage(prerender_url, GetParam());
  content::test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *GetActiveWebContents(), prerender_url);

  // The both attempts should return the same non-null handle.
  EXPECT_TRUE(handle1 && handle2);
  EXPECT_EQ(handle1.get(), handle2.get());

  // Activate.
  content::TestActivationManager activation_manager(GetActiveWebContents(),
                                                    prerender_url);
  SimulateNewTabNavigation(prerender_url);
  activation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(activation_manager.was_activated());

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_NewTabPage",
      kFinalStatusActivated, 1);
  histogram_tester.ExpectTotalCount(
      "NewTabPage.PrerenderNavigationToActivation", 1);

  ExpectPrerenderPageLoad(prerender_url,
                          page_load_metrics::NavigationHandleUserData::
                              InitiatorLocation::kNewTabPage);

  std::unique_ptr<content::test::PreloadingAttemptUkmEntryBuilder>
      mouse_hover_attempt_entry_builder =
          std::make_unique<content::test::PreloadingAttemptUkmEntryBuilder>(
              chrome_preloading_predictor::kMouseHoverOnNewTabPage);

  ukm::SourceId ukm_source_id = activation_manager.next_page_ukm_source_id();
  content::test::ExpectPreloadingAttemptUkm(
      *test_ukm_recorder(),
      {mouse_hover_attempt_entry_builder->BuildEntry(
           ukm_source_id, content::PreloadingType::kPrerender,
           content::PreloadingEligibility::kEligible,
           content::PreloadingHoldbackStatus::kAllowed,
           content::PreloadingTriggeringOutcome::kSuccess,
           content::PreloadingFailureReason::kUnspecified,
           /*accurate=*/true,
           /*ready_time=*/kMockElapsedTime),
       attempt_entry_builder().BuildEntry(
           ukm_source_id, content::PreloadingType::kPrerender,
           content::PreloadingEligibility::kEligible,
           content::PreloadingHoldbackStatus::kAllowed,
           content::PreloadingTriggeringOutcome::kDuplicate,
           content::PreloadingFailureReason::kUnspecified,
           /*accurate=*/true,
           /*ready_time=*/std::nullopt)});
}

// Verify that NewTabPage prerender rejects non https url.
IN_PROC_BROWSER_TEST_P(PrerenderNewTabPageBrowserTest,
                       NewTabPagePrerenderNonHttps) {
  // Navigate to an initial page.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(chrome::kChromeUINewTabURL)));
  GURL prerender_url = embedded_test_server()->GetURL("/simple.html?prerender");

  PrerenderManager::CreateForWebContents(GetActiveWebContents());
  auto* prerender_manager =
      PrerenderManager::FromWebContents(GetActiveWebContents());
  EXPECT_FALSE(
      prerender_manager->StartPrerenderNewTabPage(prerender_url, GetParam()));
  base::RunLoop().RunUntilIdle();
  content::FrameTreeNodeId host_id =
      prerender_helper().GetHostForUrl(prerender_url);
  EXPECT_TRUE(host_id.is_null());

  // Navigate to a different URL other than the prerender_url to flush the
  // metrics.
  ASSERT_TRUE(content::NavigateToURL(
      GetActiveWebContents(), embedded_test_server()->GetURL("/simple.html")));

  ukm::SourceId ukm_source_id =
      GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  content::test::ExpectPreloadingAttemptUkm(
      *test_ukm_recorder(),
      {attempt_entry_builder().BuildEntry(
          ukm_source_id, content::PreloadingType::kPrerender,
          content::PreloadingEligibility::kHttpsOnly,
          content::PreloadingHoldbackStatus::kUnspecified,
          content::PreloadingTriggeringOutcome::kUnspecified,
          content::PreloadingFailureReason::kUnspecified,
          /*accurate=*/false)});
}

IN_PROC_BROWSER_TEST_P(PrerenderNewTabPageBrowserTest,
                       PrerenderTriggeredCancelAndRetrigger) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(chrome::kChromeUINewTabURL)));
  GURL prerender_url = GetUrl("/simple.html");

  PrerenderManager::CreateForWebContents(GetActiveWebContents());
  auto* prerender_manager =
      PrerenderManager::FromWebContents(GetActiveWebContents());

  base::WeakPtr<content::PrerenderHandle> prerender_handle =
      prerender_manager->StartPrerenderNewTabPage(prerender_url, GetParam());
  content::test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *GetActiveWebContents(), prerender_url);

  prerender_manager->StopPrerenderNewTabPage(prerender_handle);

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_NewTabPage",
      kFinalStatusTriggerDestroyed, 1);

  // Retrigger after cancelation.
  EXPECT_TRUE(
      prerender_manager->StartPrerenderNewTabPage(prerender_url, GetParam()));
  content::test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *GetActiveWebContents(), prerender_url);

  // Activate.
  content::TestActivationManager activation_manager(GetActiveWebContents(),
                                                    prerender_url);
  SimulateNewTabNavigation(prerender_url);
  activation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(activation_manager.was_activated());

  histogram_tester.ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_NewTabPage",
      kFinalStatusActivated, 1);
  histogram_tester.ExpectTotalCount(
      "NewTabPage.PrerenderNavigationToActivation", 1);
}

IN_PROC_BROWSER_TEST_P(PrerenderNewTabPageBrowserTest,
                       DestroyedOnNavigatedAway) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(chrome::kChromeUINewTabURL)));
  GURL prerender_url = GetUrl("/simple.html?prerender");

  PrerenderManager::CreateForWebContents(GetActiveWebContents());
  auto* prerender_manager =
      PrerenderManager::FromWebContents(GetActiveWebContents());

  prerender_manager->StartPrerenderNewTabPage(prerender_url, GetParam());
  content::test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *GetActiveWebContents(), prerender_url);

  content::FrameTreeNodeId host_id =
      prerender_helper().GetHostForUrl(prerender_url);
  ASSERT_TRUE(host_id);

  // Navigate to a different page. This should cancel prerendering.
  GURL different_url = GetUrl("/simple.html?different");
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), host_id);
  SimulateNewTabNavigation(different_url);
  prerender_observer.WaitForDestroyed();

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_NewTabPage",
      kFinalStatusTriggerDestroyed, 1);
}

}  // namespace
