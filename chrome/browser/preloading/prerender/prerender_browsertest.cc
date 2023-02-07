// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/prefetch/prefetch_prefs.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
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

// This is equal to content::PrerenderFinalStatus::kActivated.
// TODO(crbug.com/1274021): Replace this with the FinalStatus enum value
// once it is exposed.
constexpr int kFinalStatusActivated = 0;
constexpr int kFinalStatusCrossSiteNavigation = 45;
constexpr int kFinalStatusSameSiteCrossOriginNavigation = 47;

}  // namespace

class PrerenderBrowserTest : public PlatformBrowserTest {
 public:
  PrerenderBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&PrerenderBrowserTest::GetActiveWebContents,
                                base::Unretained(this))) {}

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    PlatformBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

class PrerenderHoldbackBrowserTest : public PrerenderBrowserTest {
 public:
  PrerenderHoldbackBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kPreloadingHoldback);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
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
          nullptr);
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

// Tests that Devtools open overrides PreloadingHoldback.
IN_PROC_BROWSER_TEST_F(PrerenderHoldbackBrowserTest,
                       PreloadingHoldbackOverridden) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));
  PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();
  ASSERT_EQ(prefetch::IsSomePreloadingEnabled(*prefs),
            content::PreloadingEligibility::kPreloadingDisabled);

  // Emulating Devtools attached to make PreloadingHoldback overridden.
  ASSERT_NE(content::DevToolsAgentHost::GetOrCreateFor(GetActiveWebContents()),
            nullptr);

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

// Tests that Prerender2 cannot be triggered when PreloadingHoldback is not
// overridden by Devtools.
IN_PROC_BROWSER_TEST_F(PrerenderHoldbackBrowserTest,
                       PreloadingHoldbackNotOverridden) {
  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();
  ASSERT_EQ(prefetch::IsSomePreloadingEnabled(*prefs),
            content::PreloadingEligibility::kPreloadingDisabled);
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
// PrerenderBrowserTest once the feature is enabled by default.
class PrerenderMainFrameNavigationBrowserTest : public PrerenderBrowserTest {
 public:
  PrerenderMainFrameNavigationBrowserTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kPrerender2MainFrameNavigation);
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
          nullptr);
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

// TODO(crbug.com/1239281): Support the same-site cross-origin navigation.
// Tests that the same-site cross-origin main frame navigation in an embedder
// triggered prerendering page cancels the prerendering.
IN_PROC_BROWSER_TEST_F(
    PrerenderMainFrameNavigationBrowserTest,
    SameSiteCrossOriginMainFrameNavigationCancelsEmbedderTriggeredPrerendering) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("a.test", "/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  GURL prerender_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL navigation_url =
      embedded_test_server()->GetURL("b.a.test", "/title2.html");

  // Start an embedder triggered prerendering.
  std::unique_ptr<content::PrerenderHandle> prerender_handle =
      GetActiveWebContents()->StartPrerendering(
          prerender_url, content::PrerenderTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          nullptr);
  EXPECT_TRUE(prerender_handle);
  content::test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *GetActiveWebContents(), prerender_url);

  int host_id = prerender_helper().GetHostForUrl(prerender_url);
  ASSERT_NE(host_id, content::RenderFrameHost::kNoFrameTreeNodeId);

  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), host_id);

  // Start a same-site cross-origin main frame navigation in the prerender frame
  // tree. It will cancel the initiator's prerendering.
  prerender_helper().NavigatePrerenderedPage(host_id, navigation_url);

  prerender_observer.WaitForDestroyed();

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_DirectURLInput",
      kFinalStatusSameSiteCrossOriginNavigation, 1);
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
  GURL navigation_url =
      embedded_test_server()->GetURL("b.test", "/title2.html");

  // Start an embedder triggered prerendering.
  std::unique_ptr<content::PrerenderHandle> prerender_handle =
      GetActiveWebContents()->StartPrerendering(
          prerender_url, content::PrerenderTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          nullptr);
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
      kFinalStatusCrossSiteNavigation, 1);
}

}  // namespace
