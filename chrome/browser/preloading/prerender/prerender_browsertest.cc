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
#include "chrome/browser/prefetch/prefetch_prefs.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace {

namespace {

// Following definitions are equal to content::PrerenderFinalStatus.
constexpr int kFinalStatusActivated = 0;
constexpr int kFinalStatusCrossSiteNavigationInMainFrameNavigation = 64;

}  // namespace

using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;
using ukm::builders::Preloading_Attempt;

class PrerenderBrowserTest : public PlatformBrowserTest {
 public:
  PrerenderBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&PrerenderBrowserTest::GetActiveWebContents,
                                base::Unretained(this))) {}

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    prerender_helper_.SetUp(ssl_server());
    PlatformBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(embedded_test_server()->Start());

    ssl_server()->SetSSLConfig(
        net::EmbeddedTestServer::ServerCertificate::CERT_OK);
    ssl_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(ssl_server()->Start());
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    ASSERT_TRUE(ssl_server()->ShutdownAndWaitUntilComplete());
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  net::test_server::EmbeddedTestServer* ssl_server() { return &ssl_server_; }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
  net::test_server::EmbeddedTestServer ssl_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};
};

class PrerenderHoldbackBrowserTest : public PrerenderBrowserTest {
 public:
  PrerenderHoldbackBrowserTest() {
    preloading_config_override_.SetHoldback("Prerender", "SpeculationRules",
                                            true);
  }

 private:
  content::test::PreloadingConfigOverride preloading_config_override_;
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
          prerender_url, content::PrerenderTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          content::PreloadingHoldbackStatus::kUnspecified, nullptr);
  EXPECT_TRUE(prerender_handle);
  content::test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *GetActiveWebContents(), prerender_url);

  // Activate.
  content::TestActivationManager activation_manager(GetActiveWebContents(),
                                                    prerender_url);
  // Simulate a browser-initiated navigation.
  GetActiveWebContents()->OpenURL(content::OpenURLParams(
      prerender_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      /*is_renderer_initiated=*/false));
  activation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(activation_manager.was_activated());

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_DirectURLInput",
      kFinalStatusActivated, 1);
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
  int host_id = prerender_helper().GetHostForUrl(prerender_url);
  EXPECT_EQ(host_id, content::RenderFrameHost::kNoFrameTreeNodeId);

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
  EXPECT_NE(host_id, content::RenderFrameHost::kNoFrameTreeNodeId);
}

// Tests that DevTools open overrides PreloadingConfig's holdback.
IN_PROC_BROWSER_TEST_F(PrerenderHoldbackBrowserTest,
                       PreloadingHoldbackOverridden) {
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
IN_PROC_BROWSER_TEST_F(PrerenderHoldbackBrowserTest,
                       PreloadingHoldbackNotOverridden) {
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
  int host_id = prerender_helper().GetHostForUrl(prerender_url);
  EXPECT_EQ(host_id, content::RenderFrameHost::kNoFrameTreeNodeId);
}

// TODO(crbug.com/1239281): Merge PrerenderMainFrameNavigationBrowserTest into
// PrerenderBrowserTest.
class PrerenderMainFrameNavigationBrowserTest : public PrerenderBrowserTest {
 public:
  PrerenderMainFrameNavigationBrowserTest() {
    // TODO(crbug.com/1394910): Use HTTPS URLs in tests to avoid having to
    // disable this feature.
    feature_list_.InitAndDisableFeature(features::kHttpsUpgrades);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the same-origin main frame navigation in an embedder triggered
// prerendering page succeeds.
IN_PROC_BROWSER_TEST_F(PrerenderMainFrameNavigationBrowserTest,
                       SameOriginMainFrameNavigation) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  GURL prerender_url = embedded_test_server()->GetURL("/title1.html");
  GURL navigation_url = embedded_test_server()->GetURL("/title2.html");

  // Start an embedder triggered prerendering.
  std::unique_ptr<content::PrerenderHandle> prerender_handle =
      GetActiveWebContents()->StartPrerendering(
          prerender_url, content::PrerenderTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          content::PreloadingHoldbackStatus::kUnspecified, nullptr);
  EXPECT_TRUE(prerender_handle);
  content::test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *GetActiveWebContents(), prerender_url);

  int host_id = prerender_helper().GetHostForUrl(prerender_url);
  ASSERT_NE(host_id, content::RenderFrameHost::kNoFrameTreeNodeId);

  // Start a same-origin navigation in the prerender frame tree. It will not
  // cancel the initiator's prerendering.
  prerender_helper().NavigatePrerenderedPage(host_id, navigation_url);

  // Activate.
  content::TestActivationManager activation_manager(GetActiveWebContents(),
                                                    prerender_url);
  // Simulate a browser-initiated navigation.
  GetActiveWebContents()->OpenURL(content::OpenURLParams(
      prerender_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      /*is_renderer_initiated=*/false));
  activation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(activation_manager.was_activated());

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_DirectURLInput",
      kFinalStatusActivated, 1);
}

// Tests that the same-site cross-origin main frame navigation in an embedder
// triggered prerendering page succeeds.
IN_PROC_BROWSER_TEST_F(PrerenderMainFrameNavigationBrowserTest,
                       SameSiteCrossOriginMainFrameNavigation) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("a.test", "/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  GURL prerender_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL navigation_url = embedded_test_server()->GetURL(
      "b.a.test", "/prerender_with_opt_in_header.html");

  // Start an embedder triggered prerendering.
  std::unique_ptr<content::PrerenderHandle> prerender_handle =
      GetActiveWebContents()->StartPrerendering(
          prerender_url, content::PrerenderTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          content::PreloadingHoldbackStatus::kUnspecified, nullptr);
  EXPECT_TRUE(prerender_handle);
  content::test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *GetActiveWebContents(), prerender_url);

  int host_id = prerender_helper().GetHostForUrl(prerender_url);
  ASSERT_NE(host_id, content::RenderFrameHost::kNoFrameTreeNodeId);

  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), host_id);

  // Start a same-site cross-origin main frame navigation in the prerender frame
  // tree. It will not cancel the initiator's prerendering.
  prerender_helper().NavigatePrerenderedPage(host_id, navigation_url);

  // Activate.
  content::TestActivationManager activation_manager(GetActiveWebContents(),
                                                    prerender_url);
  // Simulate a browser-initiated navigation.
  GetActiveWebContents()->OpenURL(content::OpenURLParams(
      prerender_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      /*is_renderer_initiated=*/false));
  activation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(activation_manager.was_activated());

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_DirectURLInput",
      kFinalStatusActivated, 1);
}

// Tests that the cross-site main frame navigation in an embedder triggered
// prerendering page cancels the prerendering.
IN_PROC_BROWSER_TEST_F(
    PrerenderMainFrameNavigationBrowserTest,
    CrossSiteMainFrameNavigationCancelsEmbedderTriggeredPrerendering) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("a.test", "/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  GURL prerender_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL navigation_url = embedded_test_server()->GetURL(
      "b.test", "/prerender_with_opt_in_header.html");

  // Start an embedder triggered prerendering.
  std::unique_ptr<content::PrerenderHandle> prerender_handle =
      GetActiveWebContents()->StartPrerendering(
          prerender_url, content::PrerenderTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          content::PreloadingHoldbackStatus::kUnspecified, nullptr);
  EXPECT_TRUE(prerender_handle);
  content::test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *GetActiveWebContents(), prerender_url);

  int host_id = prerender_helper().GetHostForUrl(prerender_url);
  ASSERT_NE(host_id, content::RenderFrameHost::kNoFrameTreeNodeId);

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
  GURL prerender_url = ssl_server()->GetURL("/simple.html");

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
  // Simulate a browser-initiated navigation.
  GetActiveWebContents()->OpenURL(content::OpenURLParams(
      prerender_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK),
      /*is_renderer_initiated=*/false));
  activation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(activation_manager.was_activated());

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_NewTabPage",
      kFinalStatusActivated, 1);
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
  int host_id = prerender_helper().GetHostForUrl(prerender_url);
  EXPECT_EQ(host_id, content::RenderFrameHost::kNoFrameTreeNodeId);

  {
    // Navigate to a different URL other than the prerender_url to flush the
    // metrics.
    ASSERT_TRUE(
        content::NavigateToURL(GetActiveWebContents(),
                               embedded_test_server()->GetURL("/simple.html")));
    ukm::SourceId ukm_source_id =
        GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
    auto attempt_ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(attempt_ukm_entries.size(), 1u);

    UkmEntry expected_entry = attempt_entry_builder().BuildEntry(
        ukm_source_id, content::PreloadingType::kPrerender,
        content::PreloadingEligibility::kHttpsOnly,
        content::PreloadingHoldbackStatus::kUnspecified,
        content::PreloadingTriggeringOutcome::kUnspecified,
        content::PreloadingFailureReason::kUnspecified,
        /*accurate=*/false);
    EXPECT_EQ(attempt_ukm_entries[0], expected_entry)
        << content::test::ActualVsExpectedUkmEntryToString(
               attempt_ukm_entries[0], expected_entry);
  }
}

}  // namespace
