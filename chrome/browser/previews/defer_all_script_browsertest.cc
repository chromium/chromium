// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/hints_component_util.h"
#include "components/optimization_guide/optimization_guide_constants.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/test_hints_component_creator.h"
#include "components/previews/core/previews_black_list.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/network_quality_tracker.h"

namespace {

// Expected console output when defer preview is not applied to the test
// webpage.
static const char kNonDeferredPageExpectedOutput[] =
    "ScriptLog:_InlineScript_SyncScript_BodyEnd_DeveloperDeferScript_OnLoad";

// Expected console output when defer preview is applied to the test webpage.
static const char kDeferredPageExpectedOutput[] =
    "ScriptLog:_BodyEnd_InlineScript_SyncScript_DeveloperDeferScript_OnLoad";

// Retries fetching |histogram_name| until it contains at least |count| samples.
void RetryForHistogramUntilCountReached(base::HistogramTester* histogram_tester,
                                        const std::string& histogram_name,
                                        size_t count) {
  while (true) {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();

    content::FetchHistogramsFromChildProcesses();
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    const std::vector<base::Bucket> buckets =
        histogram_tester->GetAllSamples(histogram_name);
    size_t total_count = 0;
    for (const auto& bucket : buckets) {
      total_count += bucket.count;
    }
    if (total_count >= count) {
      break;
    }
  }
}

}  // namespace

class DeferAllScriptBrowserTest : public InProcessBrowserTest {
 public:
  DeferAllScriptBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {previews::features::kPreviews,
         previews::features::kDeferAllScriptPreviews,
         optimization_guide::features::kOptimizationHints,
         data_reduction_proxy::features::
             kDataReductionProxyEnabledWithNetworkService},
        {});
  }

  ~DeferAllScriptBrowserTest() override = default;

  void SetUpOnMainThread() override {
    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_2G);
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory("chrome/test/data/previews");
    ASSERT_TRUE(https_server_->Start());

    https_url_ = https_server_->GetURL("/defer_all_script_test.html");
    ASSERT_TRUE(https_url_.SchemeIs(url::kHttpsScheme));
    client_redirect_url_ = https_server_->GetURL("/client_redirect_base.html");
    client_redirect_url_target_url_ = https_server_->GetURL(
        "/client_redirect_loop_with_defer_all_script.html");
    server_redirect_url_ = https_server_->GetURL("/server_redirect_base.html");
    server_redirect_base_redirect_to_final_server_redirect_url_ =
        https_server_->GetURL(
            "/server_redirect_base_redirect_to_final_server_redirect.html");
    server_denylist_url_ = https_server_->GetURL("/login.html");

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("enable-spdy-proxy-auth");

    cmd->AppendSwitch("optimization-guide-disable-installer");
    cmd->AppendSwitch("purge_hint_cache_store");

    // Due to race conditions, it's possible that blacklist data is not loaded
    // at the time of first navigation. That may prevent Preview from
    // triggering, and causing the test to flake.
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlacklist);
  }

  // Creates hint data from the |component_info| and waits for it to be fully
  // processed before returning.
  void ProcessHintsComponent(
      const optimization_guide::HintsComponentInfo& component_info) {
    base::HistogramTester histogram_tester;

    g_browser_process->optimization_guide_service()->MaybeUpdateHintsComponent(
        component_info);

    RetryForHistogramUntilCountReached(
        &histogram_tester,
        optimization_guide::kComponentHintsUpdatedResultHistogramString, 1);
  }

  // Performs a navigation to |url| and waits for the the url's host's hints to
  // load before returning. This ensures that the hints will be available in the
  // hint cache for a subsequent navigation to a test url with the same host.
  void LoadHintsForUrl(const GURL& url) {
    base::HistogramTester histogram_tester;

    // Navigate to the url to prime the OptimizationGuide hints for the
    // url's host and ensure that they have been loaded from the store (via
    // histogram) prior to the navigation that tests functionality.
    ui_test_utils::NavigateToURL(browser(), url);

    RetryForHistogramUntilCountReached(
        &histogram_tester, optimization_guide::kLoadedHintLocalHistogramString,
        1);
  }

  void SetDeferAllScriptHintWithPageWithPattern(
      const GURL& hint_setup_url,
      const std::string& page_pattern) {
    ProcessHintsComponent(
        test_hints_component_creator_.CreateHintsComponentInfoWithPageHints(
            optimization_guide::proto::DEFER_ALL_SCRIPT,
            {hint_setup_url.host()}, page_pattern, {}));
    LoadHintsForUrl(hint_setup_url);
  }

  virtual const GURL& https_url() const { return https_url_; }

  const GURL& client_redirect_url() const { return client_redirect_url_; }

  const GURL& client_redirect_url_target_url() const {
    return client_redirect_url_target_url_;
  }

  const GURL& server_redirect_url() const { return server_redirect_url_; }

  const GURL& server_redirect_base_redirect_to_final_server_redirect() const {
    return server_redirect_base_redirect_to_final_server_redirect_url_;
  }

  const GURL& server_denylist_url() const { return server_denylist_url_; }

  std::string GetScriptLog() {
    std::string script_log;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents(), "sendLogToTest()",
        &script_log));
    return script_log;
  }

  std::string GetScriptLogForBrowser(Browser* browser) {
    std::string script_log;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        browser->tab_strip_model()->GetActiveWebContents(), "sendLogToTest()",
        &script_log));
    return script_log;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedFeatureList param_feature_list_;

 private:
  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server_->ShutdownAndWaitUntilComplete());

    InProcessBrowserTest::TearDownOnMainThread();
  }

  optimization_guide::testing::TestHintsComponentCreator
      test_hints_component_creator_;

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  GURL https_url_;
  GURL client_redirect_url_;
  GURL client_redirect_url_target_url_;
  GURL server_redirect_url_;
  GURL server_denylist_url_;

  GURL server_redirect_base_redirect_to_final_server_redirect_url_;

  DISALLOW_COPY_AND_ASSIGN(DeferAllScriptBrowserTest);
};

// Avoid flakes and issues on non-applicable platforms.
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) x
#endif

IN_PROC_BROWSER_TEST_F(
    DeferAllScriptBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(DeferAllScriptHttpsWhitelisted)) {
  GURL url = https_url();

  // Whitelist DeferAllScript for any path for the url's host.
  SetDeferAllScriptHintWithPageWithPattern(url, "*");

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  ui_test_utils::NavigateToURL(browser(), url);

  RetryForHistogramUntilCountReached(
      &histogram_tester, "PageLoad.DocumentTiming.NavigationToLoadEventFired",
      1);

  EXPECT_EQ(kDeferredPageExpectedOutput, GetScriptLog());

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.DeferAllScript",
      static_cast<int>(previews::PreviewsEligibilityReason::COMMITTED), 1);
  histogram_tester.ExpectBucketCount("Previews.PreviewShown.DeferAllScript",
                                     true, 1);
  histogram_tester.ExpectTotalCount("Previews.PageEndReason.DeferAllScript", 1);
  histogram_tester.ExpectUniqueSample(
      "Blink.Script.ForceDeferredScripts.Mainframe", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "Blink.Script.ForceDeferredScripts.Mainframe.External", 1, 1);

  // Verify UKM force deferred count entries.
  using UkmDeferEntry = ukm::builders::PreviewsDeferAllScript;
  auto entries = test_ukm_recorder.GetEntriesByName(UkmDeferEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  auto* entry = entries.at(0);
  test_ukm_recorder.ExpectEntryMetric(
      entry, UkmDeferEntry::kforce_deferred_scripts_mainframeName, 2);
  test_ukm_recorder.ExpectEntryMetric(
      entry, UkmDeferEntry::kforce_deferred_scripts_mainframe_externalName, 1);
}

// Test with an incognito browser.
IN_PROC_BROWSER_TEST_F(
    DeferAllScriptBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(DeferAllScriptHttpsWhitelisted_Incognito)) {
  GURL url = https_url();

  // Whitelist DeferAllScript for any path for the url's host.
  SetDeferAllScriptHintWithPageWithPattern(url, "*");

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  Browser* incognito = CreateIncognitoBrowser();
  ASSERT_FALSE(PreviewsServiceFactory::GetForProfile(incognito->profile()));
  ASSERT_TRUE(PreviewsServiceFactory::GetForProfile(browser()->profile()));

  ui_test_utils::NavigateToURL(incognito, url);

  RetryForHistogramUntilCountReached(
      &histogram_tester, "PageLoad.DocumentTiming.NavigationToLoadEventFired",
      1);

  EXPECT_EQ(kNonDeferredPageExpectedOutput, GetScriptLogForBrowser(incognito));
}

// Defer should not be used on a webpage whose URL matches the denylist regex.
IN_PROC_BROWSER_TEST_F(
    DeferAllScriptBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(DeferAllScriptHttpsWhitelistedDenylistURL)) {
  // Whitelist DeferAllScript for any path for the url's host.
  SetDeferAllScriptHintWithPageWithPattern(server_denylist_url(), "*");

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  ui_test_utils::NavigateToURL(browser(), server_denylist_url());

  RetryForHistogramUntilCountReached(
      &histogram_tester, "PageLoad.DocumentTiming.NavigationToLoadEventFired",
      1);
  RetryForHistogramUntilCountReached(
      &histogram_tester, "Previews.DeferAllScript.DenyListMatch", 1);
  histogram_tester.ExpectUniqueSample("Previews.DeferAllScript.DenyListMatch",
                                      true, 1);
  // Verify UKM entry.
  using UkmEntry = ukm::builders::Previews;
  auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  auto* entry = entries.at(0);
  test_ukm_recorder.ExpectEntryMetric(
      entry, UkmEntry::kdefer_all_script_eligibility_reasonName,
      static_cast<int>(previews::PreviewsEligibilityReason::DENY_LIST_MATCHED));

  EXPECT_EQ(kNonDeferredPageExpectedOutput, GetScriptLog());
}

IN_PROC_BROWSER_TEST_F(
    DeferAllScriptBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(DeferAllScriptHttpsNotWhitelisted)) {
  GURL url = https_url();

  // Whitelist DeferAllScript for the url's host but with nonmatching pattern.
  SetDeferAllScriptHintWithPageWithPattern(url, "/NoMatch/");

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  // The URL is not whitelisted.
  ui_test_utils::NavigateToURL(browser(), url);

  RetryForHistogramUntilCountReached(
      &histogram_tester, "PageLoad.DocumentTiming.NavigationToLoadEventFired",
      1);

  EXPECT_EQ(kNonDeferredPageExpectedOutput, GetScriptLog());

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.DeferAllScript",
      static_cast<int>(previews::PreviewsEligibilityReason::
                           NOT_ALLOWED_BY_OPTIMIZATION_GUIDE),
      1);
  histogram_tester.ExpectTotalCount("Previews.PreviewShown.DeferAllScript", 0);
  histogram_tester.ExpectTotalCount("Previews.PageEndReason.DeferAllScript", 0);
}

class DeferAllScriptBrowserTestWithCoinFlipHoldback
    : public DeferAllScriptBrowserTest {
 public:
  DeferAllScriptBrowserTestWithCoinFlipHoldback() {
    // Holdback the page load from previews and also disable offline previews to
    // ensure that only post-commit previews are enabled.
    feature_list_.InitWithFeaturesAndParameters(
        {{previews::features::kCoinFlipHoldback,
          {{"force_coin_flip_always_holdback", "true"}}}},
        {previews::features::kOfflinePreviews});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    DeferAllScriptBrowserTestWithCoinFlipHoldback,
    DISABLE_ON_WIN_MAC_CHROMEOS(
        DeferAllScriptHttpsWhitelistedButWithCoinFlipHoldback)) {
  GURL url = https_url();

  // Whitelist DeferAllScript for any path for the url's host.
  SetDeferAllScriptHintWithPageWithPattern(url, "*");

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  ui_test_utils::NavigateToURL(browser(), url);

  RetryForHistogramUntilCountReached(
      &histogram_tester, "PageLoad.DocumentTiming.NavigationToLoadEventFired",
      1);

  EXPECT_EQ(kNonDeferredPageExpectedOutput, GetScriptLog());

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.DeferAllScript",
      static_cast<int>(previews::PreviewsEligibilityReason::COMMITTED), 1);
  histogram_tester.ExpectTotalCount("Previews.PreviewShown.DeferAllScript", 0);
  histogram_tester.ExpectTotalCount("Previews.PageEndReason.DeferAllScript", 0);

  // Verify UKM entries.
  {
    using UkmEntry = ukm::builders::Previews;
    auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
    ASSERT_EQ(1u, entries.size());
    auto* entry = entries.at(0);
    test_ukm_recorder.ExpectEntryMetric(entry, UkmEntry::kpreviews_likelyName,
                                        1);
    test_ukm_recorder.ExpectEntryMetric(entry, UkmEntry::kdefer_all_scriptName,
                                        true);
  }

  {
    using UkmEntry = ukm::builders::PreviewsCoinFlip;
    auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
    ASSERT_EQ(1u, entries.size());
    auto* entry = entries.at(0);
    test_ukm_recorder.ExpectEntryMetric(entry, UkmEntry::kcoin_flip_resultName,
                                        2);
  }
}

// The client_redirect_url (/client_redirect_base.html) performs a client
// redirect to "/client_redirect_loop_with_defer_all_script.html" which
// peforms a client redirect back to the initial client_redirect_url if
// and only if script execution is deferred. This emulates the navigation
// pattern seen in crbug.com/987062
IN_PROC_BROWSER_TEST_F(
    DeferAllScriptBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(DeferAllScriptClientRedirectLoopStopped)) {
  PreviewsService* previews_service = PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetBrowserContext()));
  EXPECT_TRUE(previews_service->IsUrlEligibleForDeferAllScriptPreview(
      client_redirect_url()));
  EXPECT_TRUE(
      previews_service->IsUrlEligibleForDeferAllScriptPreview(https_url()));

  // Whitelist DeferAllScript for any path for the url's host.
  SetDeferAllScriptHintWithPageWithPattern(https_url(), "*");

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  ui_test_utils::NavigateToURL(browser(), client_redirect_url());

  RetryForHistogramUntilCountReached(
      &histogram_tester, "Navigation.ClientRedirectCycle.RedirectToReferrer",
      2);
  RetryForHistogramUntilCountReached(
      &histogram_tester, "Previews.PageEndReason.DeferAllScript", 3);

  // If there is a redirect loop, call to NavigateToURL() would never finish.
  // The checks belows are additional checks to ensure that the logic to detect
  // redirect loops is being called.
  //
  // Client redirect loop is broken on 2nd pass around the loop so expect 3
  // previews before previews turned off to stop loop.
  histogram_tester.ExpectTotalCount(
      "Navigation.ClientRedirectCycle.RedirectToReferrer", 2);
  histogram_tester.ExpectTotalCount("Previews.PageEndReason.DeferAllScript", 3);

  EXPECT_FALSE(previews_service->IsUrlEligibleForDeferAllScriptPreview(
      client_redirect_url()));
  EXPECT_FALSE(previews_service->IsUrlEligibleForDeferAllScriptPreview(
      client_redirect_url_target_url()));
  // https_url() is not in redirect chain and should still be eligible for the
  // preview.
  EXPECT_TRUE(
      previews_service->IsUrlEligibleForDeferAllScriptPreview(https_url()));
}

// The server_redirect_url (/server_redirect_url.html) performs a server
// rediect to client_redirect_url() which redirects to
// client_redirect_url_target_url(). Finally,
// client_redirect_url_target_url() performs a client redirect back to
// client_redirect_url() only if script execution is deferred.
IN_PROC_BROWSER_TEST_F(DeferAllScriptBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(
                           DeferAllScriptServerClientRedirectLoopStopped)) {
  PreviewsService* previews_service = PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetBrowserContext()));
  EXPECT_TRUE(previews_service->IsUrlEligibleForDeferAllScriptPreview(
      server_redirect_url()));
  EXPECT_TRUE(
      previews_service->IsUrlEligibleForDeferAllScriptPreview(https_url()));

  // Whitelist DeferAllScript for any path for the url's host.
  SetDeferAllScriptHintWithPageWithPattern(https_url(), "*");

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  ui_test_utils::NavigateToURL(browser(), server_redirect_url());

  // If there is a redirect loop, call to NavigateToURL() would never finish.
  // The checks belows are additional checks to ensure that the logic to detect
  // redirect loops is being called.
  //
  // Client redirect loop is broken on 2nd pass around the loop so expect 3
  // previews before previews turned off to stop loop.
  RetryForHistogramUntilCountReached(
      &histogram_tester, "Navigation.ClientRedirectCycle.RedirectToReferrer",
      2);
  RetryForHistogramUntilCountReached(
      &histogram_tester, "Previews.PageEndReason.DeferAllScript", 3);

  histogram_tester.ExpectTotalCount(
      "Navigation.ClientRedirectCycle.RedirectToReferrer", 2);
  histogram_tester.ExpectTotalCount("Previews.PageEndReason.DeferAllScript", 3);
  EXPECT_FALSE(previews_service->IsUrlEligibleForDeferAllScriptPreview(
      server_redirect_url()));
  EXPECT_FALSE(previews_service->IsUrlEligibleForDeferAllScriptPreview(
      client_redirect_url()));
  EXPECT_FALSE(previews_service->IsUrlEligibleForDeferAllScriptPreview(
      client_redirect_url_target_url()));
  // https_url() is not in redirect chain and should still be eligible for the
  // preview.
  EXPECT_TRUE(
      previews_service->IsUrlEligibleForDeferAllScriptPreview(https_url()));
}

// server_redirect_base_redirect_to_final_server_redirect()
// performs a server redirect which does a client redirect followed
// by another client redirect (only when defer is enabled) to
// server_redirect_base_redirect_to_final_server_redirect().
IN_PROC_BROWSER_TEST_F(
    DeferAllScriptBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(
        DeferAllScriptServerClientServerClientServerRedirectLoopStopped)) {
  PreviewsService* previews_service = PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetBrowserContext()));
  EXPECT_TRUE(previews_service->IsUrlEligibleForDeferAllScriptPreview(
      server_redirect_base_redirect_to_final_server_redirect()));
  EXPECT_TRUE(
      previews_service->IsUrlEligibleForDeferAllScriptPreview(https_url()));

  // Whitelist DeferAllScript for any path for the url's host.
  SetDeferAllScriptHintWithPageWithPattern(https_url(), "*");

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  ui_test_utils::NavigateToURL(
      browser(), server_redirect_base_redirect_to_final_server_redirect());

  // If there is a redirect loop, call to NavigateToURL() would never finish.
  // The checks belows are additional checks to ensure that the logic to detect
  // redirect loops is being called.
  RetryForHistogramUntilCountReached(
      &histogram_tester, "Navigation.ClientRedirectCycle.RedirectToReferrer",
      1);
  RetryForHistogramUntilCountReached(
      &histogram_tester, "Previews.PageEndReason.DeferAllScript", 3);

  histogram_tester.ExpectTotalCount(
      "Navigation.ClientRedirectCycle.RedirectToReferrer", 1);
  histogram_tester.ExpectTotalCount("Previews.PageEndReason.DeferAllScript", 3);
  EXPECT_FALSE(previews_service->IsUrlEligibleForDeferAllScriptPreview(
      server_redirect_base_redirect_to_final_server_redirect()));
  // https_url() is not in redirect chain and should still be eligible for the
  // preview.
  EXPECT_TRUE(
      previews_service->IsUrlEligibleForDeferAllScriptPreview(https_url()));

  // Verify UKM entry.
  using UkmEntry = ukm::builders::Previews;
  auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(4u, entries.size());
  auto* entry = entries.at(3);
  test_ukm_recorder.ExpectEntryMetric(
      entry, UkmEntry::kdefer_all_script_eligibility_reasonName,
      static_cast<int>(
          previews::PreviewsEligibilityReason::REDIRECT_LOOP_DETECTED));
}
