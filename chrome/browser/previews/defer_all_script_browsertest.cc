// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/previews/previews_test_util.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/optimization_guide/core/hints_component_info.h"
#include "components/optimization_guide/core/hints_component_util.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_service.h"
#include "components/optimization_guide/core/test_hints_component_creator.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/pref_service.h"
#include "components/previews/core/previews_block_list.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "third_party/blink/public/common/features.h"

class DeferAllScriptBrowserTest : public InProcessBrowserTest {
 public:
  DeferAllScriptBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {previews::features::kPreviews,
         previews::features::kDeferAllScriptPreviews,
         optimization_guide::features::kOptimizationHints,
         features::kBackForwardCache},
        {});
  }

  ~DeferAllScriptBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->OverrideTargetDecisionForTesting(
            optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
            optimization_guide::OptimizationGuideDecision::kTrue);
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory("chrome/test/data/previews");
    ASSERT_TRUE(https_server_->Start());

    https_url_ = https_server_->GetURL("/defer_all_script_test.html");
    ASSERT_TRUE(https_url_.SchemeIs(url::kHttpsScheme));

    https_url_with_iframe_ =
        https_server_->GetURL("/defer_all_script_test_with_iframe.html");

    client_redirect_url_ = https_server_->GetURL("/client_redirect_base.html");
    client_redirect_url_target_url_ = https_server_->GetURL(
        "/client_redirect_loop_with_defer_all_script.html");
    server_redirect_url_ = https_server_->GetURL("/server_redirect_base.html");
    server_redirect_base_redirect_to_final_server_redirect_url_ =
        https_server_->GetURL(
            "/server_redirect_base_redirect_to_final_server_redirect.html");
    server_denylist_url_ = https_server_->GetURL("/login.html");
    another_host_url_ =
        https_server_->GetURL("anotherhost.com", "/search_results_page.html");

    https_no_transform_url_ = https_server_->GetURL(
        "/defer_all_script_with_no_transform_header.html");

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    // For using an HTTPS server.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kIgnoreCertificateErrors);

    cmd->AppendSwitch("enable-spdy-proxy-auth");

    cmd->AppendSwitch("optimization-guide-disable-installer");
    cmd->AppendSwitch("purge_hint_cache_store");

    // Due to race conditions, it's possible that blocklist data is not loaded
    // at the time of first navigation. That may prevent Preview from
    // triggering, and causing the test to flake.
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlocklist);

    InProcessBrowserTest::SetUpCommandLine(cmd);
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
            {hint_setup_url.host()}, page_pattern));
    LoadHintsForUrl(hint_setup_url);
  }

  void SetDataSaverEnabled(content::BrowserContext* browser_context,
                           bool enabled) {
    Profile* profile = Profile::FromBrowserContext(browser_context);

    data_reduction_proxy::DataReductionProxySettings::
        SetDataSaverEnabledForTesting(profile->GetPrefs(), enabled);
    base::RunLoop().RunUntilIdle();
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  virtual const GURL& https_url() const { return https_url_; }

  const GURL& https_url_with_iframe() const { return https_url_with_iframe_; }

  const GURL& client_redirect_url() const { return client_redirect_url_; }

  const GURL& client_redirect_url_target_url() const {
    return client_redirect_url_target_url_;
  }

  const GURL& server_redirect_url() const { return server_redirect_url_; }

  const GURL& server_redirect_base_redirect_to_final_server_redirect() const {
    return server_redirect_base_redirect_to_final_server_redirect_url_;
  }

  const GURL& server_denylist_url() const { return server_denylist_url_; }

  const GURL& another_host_url() const { return another_host_url_; }

  const GURL& https_no_transform_url() const { return https_no_transform_url_; }

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
  GURL https_url_with_iframe_;
  GURL client_redirect_url_;
  GURL client_redirect_url_target_url_;
  GURL server_redirect_url_;
  GURL server_denylist_url_;
  GURL another_host_url_;
  GURL https_no_transform_url_;

  GURL server_redirect_base_redirect_to_final_server_redirect_url_;

  DISALLOW_COPY_AND_ASSIGN(DeferAllScriptBrowserTest);
};

namespace {

GURL SetQuery(GURL url, const std::string& query) {
  url::Replacements<char> repls;
  repls.SetQuery(query.c_str(), url::Component(0, query.length()));
  return url.ReplaceComponents(repls);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(
    DeferAllScriptBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(DeferAllScriptHttpsWhitelisted)) {
  GURL url = https_url();

  // Whitelist DeferAllScript for any path for the url's host.
  SetDeferAllScriptHintWithPageWithPattern(url, "*");

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  // Set query to ensure that it's not treated as a reload as preview metrics
  // are not recorded for reloads.
  ui_test_utils::NavigateToURL(browser(), SetQuery(url, "foo"));

  RetryForHistogramUntilCountReached(
      &histogram_tester, "PageLoad.DocumentTiming.NavigationToLoadEventFired",
      1);

  EXPECT_EQ(kDeferredPageExpectedOutput, GetScriptLog(browser()));

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

  // Opt out of the Preview page.
  PreviewsUITabHelper::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents())
      ->ReloadWithoutPreviews();

  histogram_tester.ExpectBucketCount(
      "Previews.OptOut.UserOptedOut.DeferAllScript", 1, 1);
}

IN_PROC_BROWSER_TEST_F(
    DeferAllScriptBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(DeferAllScriptHttpsWhitelistedNoTransform)) {
  GURL url = https_no_transform_url();

  // Whitelist DeferAllScript for any path for the url's host.
  SetDeferAllScriptHintWithPageWithPattern(url, "*");

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  ui_test_utils::NavigateToURL(browser(), url);

  RetryForHistogramUntilCountReached(
      &histogram_tester, "PageLoad.DocumentTiming.NavigationToLoadEventFired",
      1);

  EXPECT_EQ(kNonDeferredPageExpectedOutput, GetScriptLog(browser()));

  histogram_tester.ExpectTotalCount("Previews.PreviewShown.DeferAllScript", 0);
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

  EXPECT_EQ(kNonDeferredPageExpectedOutput, GetScriptLog(incognito));
}

// Defer should not be used on a webpage whose URL matches the denylist regex.
IN_PROC_BROWSER_TEST_F(
    DeferAllScriptBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(DeferAllScriptHttpsWhitelistedDenylistURL)) {
  // Whitelist DeferAllScript for any path for the url's host.
  SetDeferAllScriptHintWithPageWithPattern(server_denylist_url(), "*");

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  // Set query to ensure that it's not treated as a reload as preview metrics
  // are not recorded for reloads.
  ui_test_utils::NavigateToURL(browser(),
                               SetQuery(server_denylist_url(), "foo"));

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

  EXPECT_EQ(kNonDeferredPageExpectedOutput, GetScriptLog(browser()));
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
  // Set query to ensure that it's not treated as a reload as preview metrics
  // are not recorded for reloads.
  ui_test_utils::NavigateToURL(browser(), SetQuery(url, "foo"));

  RetryForHistogramUntilCountReached(
      &histogram_tester, "PageLoad.DocumentTiming.NavigationToLoadEventFired",
      1);

  EXPECT_EQ(kNonDeferredPageExpectedOutput, GetScriptLog(browser()));

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.DeferAllScript",
      static_cast<int>(previews::PreviewsEligibilityReason::
                           NOT_ALLOWED_BY_OPTIMIZATION_GUIDE),
      1);
  histogram_tester.ExpectTotalCount("Previews.PreviewShown.DeferAllScript", 0);
  histogram_tester.ExpectTotalCount("Previews.PageEndReason.DeferAllScript", 0);
}

IN_PROC_BROWSER_TEST_F(DeferAllScriptBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(DisableDataSaver)) {
  GURL url = https_url();

  // Allow DeferAllScript for any path for the url's host.
  SetDeferAllScriptHintWithPageWithPattern(url, "*");

  base::HistogramTester histogram_tester;

  // Set query to ensure that it's not treated as a reload as preview metrics
  // are not recorded for reloads.
  ui_test_utils::NavigateToURL(browser(), SetQuery(url, "foo"));
  RetryForHistogramUntilCountReached(
      &histogram_tester, "PageLoad.DocumentTiming.NavigationToLoadEventFired",
      1);
  EXPECT_EQ(kDeferredPageExpectedOutput, GetScriptLog(browser()));
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.DeferAllScript",
      static_cast<int>(previews::PreviewsEligibilityReason::COMMITTED), 1);

  // Load another webpage. Previews should be triggerd.
  ui_test_utils::NavigateToURL(browser(), SetQuery(url, "bar"));
  RetryForHistogramUntilCountReached(
      &histogram_tester, "PageLoad.DocumentTiming.NavigationToLoadEventFired",
      2);
  EXPECT_EQ(kDeferredPageExpectedOutput, GetScriptLog(browser()));
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.DeferAllScript",
      static_cast<int>(previews::PreviewsEligibilityReason::COMMITTED), 2);

  // Load another webpage with data saver disabled. Previews should not trigger.
  SetDataSaverEnabled(browser()->profile(), false);
  ui_test_utils::NavigateToURL(browser(), SetQuery(url, "baz"));
  RetryForHistogramUntilCountReached(
      &histogram_tester, "PageLoad.DocumentTiming.NavigationToLoadEventFired",
      3);
  EXPECT_EQ(kNonDeferredPageExpectedOutput, GetScriptLog(browser()));
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.DeferAllScript",
      static_cast<int>(previews::PreviewsEligibilityReason::COMMITTED), 2);
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
        {});
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

  // Set query to ensure that it's not treated as a reload as preview metrics
  // are not recorded for reloads.
  ui_test_utils::NavigateToURL(browser(), SetQuery(url, "foo"));

  RetryForHistogramUntilCountReached(
      &histogram_tester, "PageLoad.DocumentTiming.NavigationToLoadEventFired",
      1);

  EXPECT_EQ(kNonDeferredPageExpectedOutput, GetScriptLog(browser()));

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
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  EXPECT_TRUE(previews_service->IsUrlEligibleForDeferAllScriptPreview(
      client_redirect_url()));
  EXPECT_TRUE(
      previews_service->IsUrlEligibleForDeferAllScriptPreview(https_url()));

  // Whitelist DeferAllScript for any path for the url's host.
  SetDeferAllScriptHintWithPageWithPattern(https_url(), "*");

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  ui_test_utils::NavigateToURL(browser(), client_redirect_url());

  // If there is a redirect loop, call to NavigateToURL() would never finish.
  // The checks belows are additional checks to ensure that the logic to detect
  // redirect loops is being called.
  //
  // Client redirect loop is broken on 2nd pass around the loop so expect 3
  // previews before previews turned off to stop loop.
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "Previews.DeferAllScript.RedirectLoopDetectedUsingCache", 2);
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
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
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
      &histogram_tester,
      "Previews.DeferAllScript.RedirectLoopDetectedUsingCache", 2);
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
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
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
      &histogram_tester,
      "Previews.DeferAllScript.RedirectLoopDetectedUsingCache", 1);
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
  ASSERT_EQ(5u, entries.size());
  auto* entry = entries.at(3);
  test_ukm_recorder.ExpectEntryMetric(
      entry, UkmEntry::kdefer_all_script_eligibility_reasonName,
      static_cast<int>(
          previews::PreviewsEligibilityReason::REDIRECT_LOOP_DETECTED));
}

IN_PROC_BROWSER_TEST_F(DeferAllScriptBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(
                           DeferAllScriptRestoredPreviewWithBackForwardCache)) {
  GURL url = https_url();

  // Whitelist DeferAllScript for any path for the url's host.
  SetDeferAllScriptHintWithPageWithPattern(url, "*");

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  // Wait for initial page load to complete.
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // Navigate to DeferAllScript url expecting a DeferAllScript preview.
  // Set query to ensure that it's not treated as a reload as preview metrics
  // are not recorded for reloads.
  ui_test_utils::NavigateToURL(browser(), SetQuery(url, "foo"));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // Verify good DeferAllScript preview.
  EXPECT_EQ(kDeferredPageExpectedOutput, GetScriptLog(browser()));
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.DeferAllScript",
      static_cast<int>(previews::PreviewsEligibilityReason::COMMITTED), 1);
  histogram_tester.ExpectBucketCount("Previews.PreviewShown.DeferAllScript",
                                     true, 1);

  // Override the target decision to |kFalse| to not trigger a preview for the
  // new decision.
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetDecisionForTesting(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
          optimization_guide::OptimizationGuideDecision::kFalse);

  // Navigate to another host on same tab (to cause previous navigation
  // to be saved in BackForward cache).
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), another_host_url(), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // Verify preview UI not shown.
  EXPECT_FALSE(PreviewsUITabHelper::FromWebContents(web_contents())
                   ->displayed_preview_ui());

  // Verify that no BackForwardCache restore made yet.
  histogram_tester.ExpectTotalCount("BackForwardCache.HistoryNavigationOutcome",
                                    0);

  // Navigate back to exercise that with kBackForwardCache enabled, the preview
  // page will be restored (even though ECT is now 4G and a new preview would
  // not trigger).
  web_contents()->GetController().GoBack();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // Verify that the page was restored from BackForwardCache.
  histogram_tester.ExpectUniqueSample(
      "BackForwardCache.HistoryNavigationOutcome", 0 /* Restored */, 1);

  // Verify the restored page has the DeferAllScript preview page contents.
  EXPECT_EQ(kDeferredPageExpectedOutput, GetScriptLog(browser()));

  // Verify preview UI shown.
  EXPECT_TRUE(PreviewsUITabHelper::FromWebContents(web_contents())
                  ->displayed_preview_ui());

  // Verify preview was triggered.
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.DeferAllScript",
      static_cast<int>(previews::PreviewsEligibilityReason::COMMITTED), 1);
  histogram_tester.ExpectBucketCount("Previews.PreviewShown.DeferAllScript",
                                     true, 2);
}

IN_PROC_BROWSER_TEST_F(
    DeferAllScriptBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(
        DeferAllScriptNonPreviewRestoredWithBackForwardCache)) {
  GURL url = https_url();

  // Whitelist DeferAllScript for any path for the url's host.
  SetDeferAllScriptHintWithPageWithPattern(url, "*");

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  // Wait for initial page load to complete.
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // Override the target decision to |kFalse| to choose not to trigger a
  // preview this navigation.
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetDecisionForTesting(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
          optimization_guide::OptimizationGuideDecision::kFalse);

  // Navigate to DeferAllScript url.
  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // Verify non-DeferAllScript page load.
  EXPECT_EQ(kNonDeferredPageExpectedOutput, GetScriptLog(browser()));
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.DeferAllScript",
      static_cast<int>(previews::PreviewsEligibilityReason::COMMITTED), 0);

  // Now override the model decision to |kTrue| to allow a preview for a
  // new decision.
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetDecisionForTesting(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
          optimization_guide::OptimizationGuideDecision::kTrue);

  // Navigate to another host on same tab (to cause previous navigation
  // to be saved in BackForward cache).
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), another_host_url(), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.DeferAllScript",
      static_cast<int>(previews::PreviewsEligibilityReason::COMMITTED), 0);

  // Verify that no BackForwardCache restore made yet.
  histogram_tester.ExpectTotalCount("BackForwardCache.HistoryNavigationOutcome",
                                    0);

  // Navigate back to exercise that with kBackForwardCache enabled, the preview
  // page will be restored (even though ECT is now 4G and a new preview would
  // not trigger).
  web_contents()->GetController().GoBack();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // Verify that the page was restored from BackForwardCache.
  histogram_tester.ExpectUniqueSample(
      "BackForwardCache.HistoryNavigationOutcome", 0 /* Restored */, 1);

  // Verify the restored page has the normal page contents.
  EXPECT_EQ(kNonDeferredPageExpectedOutput, GetScriptLog(browser()));

  // Verify no new preview was triggered - same counts as before.
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.DeferAllScript",
      static_cast<int>(previews::PreviewsEligibilityReason::COMMITTED), 0);
  histogram_tester.ExpectBucketCount("Previews.PreviewShown.DeferAllScript",
                                     true, 0);
}

class DeferAllScriptIframesBrowserTest
    : public ::testing::WithParamInterface<bool>,
      public DeferAllScriptBrowserTest {
 public:
  DeferAllScriptIframesBrowserTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          blink::features::kDisableForceDeferInChildFrames);
    } else {
      feature_list_.InitAndDisableFeature(
          blink::features::kDisableForceDeferInChildFrames);
    }
  }

  bool is_force_defer_disabled_in_child_frames() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(ShouldSkipPreview,
                         DeferAllScriptIframesBrowserTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(
    DeferAllScriptIframesBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(DeferAllScriptHttpsWhitelisted_Iframe)) {
  GURL url = https_url_with_iframe();

  // When defer is disabled
  // in iframes which should delay execution of SyncScript.
  static const char kDeferEnabledIframes[] =
      "ScriptLog:_InlineMainFrameScript_ScriptLogFromIframe:_BodyEnd_"
      "InlineScript_"
      "SyncScript_DeveloperDeferScript";
  // When defer is enabled
  // in iframes which should execute scripts in regular order.
  static const char kDeferDisabledIframes[] =
      "ScriptLog:_InlineMainFrameScript_ScriptLogFromIframe:_InlineScript_"
      "SyncScript_"
      "BodyEnd_DeveloperDeferScript";

  // Whitelist DeferAllScript for any path for the url's host.
  SetDeferAllScriptHintWithPageWithPattern(url, "*");

  base::HistogramTester histogram_tester;

  // Set query to ensure that it's not treated as a reload as preview metrics
  // are not recorded for reloads.
  ui_test_utils::NavigateToURL(browser(), SetQuery(url, "foo"));

  RetryForHistogramUntilCountReached(
      &histogram_tester, "PageLoad.DocumentTiming.NavigationToLoadEventFired",
      1);

  if (is_force_defer_disabled_in_child_frames()) {
    EXPECT_EQ(kDeferDisabledIframes, GetScriptLog(browser()));
  } else {
    EXPECT_EQ(kDeferEnabledIframes, GetScriptLog(browser()));
  }

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.DeferAllScript",
      static_cast<int>(previews::PreviewsEligibilityReason::COMMITTED), 1);
  histogram_tester.ExpectBucketCount("Previews.PreviewShown.DeferAllScript",
                                     true, 1);
  histogram_tester.ExpectTotalCount("Previews.PageEndReason.DeferAllScript", 1);

  histogram_tester.ExpectUniqueSample(
      "Blink.Script.ForceDeferredScripts.Mainframe", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "Blink.Script.ForceDeferredScripts.Mainframe.External", 0, 1);

  if (!is_force_defer_disabled_in_child_frames()) {
    histogram_tester.ExpectUniqueSample(
        "Blink.Script.ForceDeferredScripts.Subframe", 2, 1);
    histogram_tester.ExpectTotalCount(
        "Blink.Script.ForceDeferredScripts.Subframe.External", 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Blink.Script.ForceDeferredScripts.Subframe", 0);
    histogram_tester.ExpectTotalCount(
        "Blink.Script.ForceDeferredScripts.Subframe.External", 0);
  }
}
