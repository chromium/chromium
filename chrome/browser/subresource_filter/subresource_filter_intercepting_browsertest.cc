// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_database_helper.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/core/browser/db/safebrowsing.pb.h"
#include "components/safe_browsing/core/browser/db/v4_embedded_test_server_util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v4_test_util.h"
#include "components/safe_browsing/core/browser/db/v5_embedded_test_server_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
namespace subresource_filter {

// This test harness intercepts URLRequests going to the SafeBrowsing server.
// It allows the tests to mock out proto responses.
class SubresourceFilterInterceptingBrowserTest
    : public SubresourceFilterBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  SubresourceFilterInterceptingBrowserTest()
      : safe_browsing_test_server_(
            std::make_unique<net::test_server::EmbeddedTestServer>()) {
    if (UseV5()) {
      scoped_feature_list_.InitAndEnableFeature(
          safe_browsing::kLocalListsUseSBv5);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          safe_browsing::kLocalListsUseSBv5);
    }
  }

  SubresourceFilterInterceptingBrowserTest(
      const SubresourceFilterInterceptingBrowserTest&) = delete;
  SubresourceFilterInterceptingBrowserTest& operator=(
      const SubresourceFilterInterceptingBrowserTest&) = delete;

  ~SubresourceFilterInterceptingBrowserTest() override = default;

  bool UseV5() const { return GetParam(); }

  net::test_server::EmbeddedTestServer* safe_browsing_test_server() {
    return safe_browsing_test_server_.get();
  }

  safe_browsing::ThreatMatch GetBetterAdsMatchV4(const GURL& url,
                                                 const std::string& bas_value) {
    safe_browsing::ThreatMatch threat_match;
    threat_match.set_threat_type(safe_browsing::SUBRESOURCE_FILTER);
    threat_match.set_platform_type(
        safe_browsing::GetUrlSubresourceFilterId().platform_type());
    threat_match.set_threat_entry_type(safe_browsing::URL);

    safe_browsing::FullHashStr enforce_full_hash =
        safe_browsing::SBProtocolManagerUtil::GetFullHash(url);
    threat_match.mutable_threat()->set_hash(enforce_full_hash);
    threat_match.mutable_cache_duration()->set_seconds(300);

    safe_browsing::ThreatEntryMetadata::MetadataEntry* threat_meta =
        threat_match.mutable_threat_entry_metadata()->add_entries();
    threat_meta->set_key("sf_bas");
    threat_meta->set_value(bas_value);
    return threat_match;
  }

  safe_browsing::V5::FullHash GetBetterAdsMatchV5(const GURL& url,
                                                  bool is_warn_only) {
    safe_browsing::V5::FullHash full_hash;
    full_hash.set_full_hash(
        safe_browsing::SBProtocolManagerUtil::GetFullHash(url));
    auto* detail = full_hash.add_full_hash_details();
    detail->set_threat_type(
        safe_browsing::V5::ThreatType::BETTER_ADS_VIOLATION);
    if (is_warn_only) {
      detail->add_attributes(safe_browsing::V5::ThreatAttribute::CANARY);
    }
    return full_hash;
  }

  // Creates a redirect chain to the final redirect_url from the initial host
  // where the SafeBrowsing result from the intial host is delayed. Returns
  // the initial url.
  GURL InitializeSafeBrowsingForOutOfOrderResponses(
      const std::string& initial_host,
      const GURL& redirect_url,
      base::TimeDelta initial_delay) {
    GURL url(embedded_test_server()->GetURL(
        initial_host, "/server-redirect?" + redirect_url.spec()));

    // Mark the prefixes as bad so that safe browsing will request full hashes
    // from the server.
    database_helper()->LocallyMarkPrefixAsBad(
        url, safe_browsing::GetUrlSubresourceFilterId());
    database_helper()->LocallyMarkPrefixAsBad(
        redirect_url, safe_browsing::GetUrlSubresourceFilterId());

    std::map<GURL, base::TimeDelta> delay_map{{url, initial_delay}};
    if (UseV5()) {
      // Map URLs to policies: enforce on the initial URL, and safe (no match)
      // on the redirect URL.
      std::map<GURL, safe_browsing::V5::FullHash> response_map{
          {url, GetBetterAdsMatchV5(url, /*is_warn_only=*/false)},
          {redirect_url, safe_browsing::V5::FullHash()}};
      // Delay the initial response, so it arrives after the final.
      safe_browsing::StartRedirectingV5RequestsForTesting(
          response_map, safe_browsing_test_server(), delay_map);
    } else {
      // Map URLs to policies: enforce on the initial URL, and safe (no match)
      // on the redirect URL.
      std::map<GURL, safe_browsing::ThreatMatch> response_map{
          {url, GetBetterAdsMatchV4(url, "enforce")},
          {redirect_url, safe_browsing::ThreatMatch()}};
      // Delay the initial response, so it arrives after the final.
      safe_browsing::StartRedirectingV4RequestsForTesting(
          response_map, safe_browsing_test_server(), delay_map);
    }
    safe_browsing_test_server()->StartAcceptingConnections();
    return url;
  }

 private:
  // SubresourceFilterBrowserTest:
  std::unique_ptr<TestSafeBrowsingDatabaseHelper> CreateTestDatabase()
      override {
    std::vector<safe_browsing::ListIdentifier> list_ids = {
        safe_browsing::GetUrlSubresourceFilterId()};
    return std::make_unique<TestSafeBrowsingDatabaseHelper>(
        nullptr, std::move(list_ids));
  }
  void SetUp() override {
    ASSERT_TRUE(safe_browsing_test_server()->InitializeAndListen());
    SubresourceFilterBrowserTest::SetUp();
  }
  // This class needs some specific test server managing to intercept hash
  // requests, so just use another server for that rather than try to use the
  // parent class' server.
  std::unique_ptr<net::test_server::EmbeddedTestServer>
      safe_browsing_test_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(SubresourceFilterInterceptingBrowserTest,
                       BetterAdsMetadata) {
  ResetConfiguration(Configuration::MakePresetForLiveRunForBetterAds());
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  GURL enforce_url(embedded_test_server()->GetURL(
      "enforce.example.com",
      "/subresource_filter/frame_with_included_script.html"));
  GURL warn_url(embedded_test_server()->GetURL(
      "warn.example.com",
      "/subresource_filter/frame_with_included_script.html"));

  // Mark the prefixes as bad so that safe browsing will request full hashes
  // from the server.
  database_helper()->LocallyMarkPrefixAsBad(
      enforce_url, safe_browsing::GetUrlSubresourceFilterId());
  database_helper()->LocallyMarkPrefixAsBad(
      warn_url, safe_browsing::GetUrlSubresourceFilterId());

  // Register the test server to handle full hash requests for the two URLs
  // with the given matches, then start accepting connections on the server.
  if (UseV5()) {
    std::map<GURL, safe_browsing::V5::FullHash> response_map{
        {enforce_url, GetBetterAdsMatchV5(enforce_url, /*is_warn_only=*/false)},
        {warn_url, GetBetterAdsMatchV5(warn_url, /*is_warn_only=*/true)}};
    safe_browsing::StartRedirectingV5RequestsForTesting(
        response_map, safe_browsing_test_server());
  } else {
    std::map<GURL, safe_browsing::ThreatMatch> response_map{
        {enforce_url, GetBetterAdsMatchV4(enforce_url, "enforce")},
        {warn_url, GetBetterAdsMatchV4(warn_url, "warn")}};
    safe_browsing::StartRedirectingV4RequestsForTesting(
        response_map, safe_browsing_test_server());
  }
  safe_browsing_test_server()->StartAcceptingConnections();

  {
    content::WebContentsConsoleObserver enforce_console_observer(
        web_contents());
    enforce_console_observer.SetPattern(kActivationConsoleMessage);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), enforce_url));
    EXPECT_FALSE(
        WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
    EXPECT_EQ(kActivationConsoleMessage,
              enforce_console_observer.GetMessageAt(0u));
  }

  {
    content::WebContentsConsoleObserver warn_console_observer(web_contents());
    warn_console_observer.SetPattern(kActivationWarningConsoleMessage);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), warn_url));
    ASSERT_TRUE(warn_console_observer.Wait());
    EXPECT_TRUE(
        WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
    EXPECT_EQ(kActivationWarningConsoleMessage,
              warn_console_observer.GetMessageAt(0u));
  }
}

// Verify that the navigation waits on all safebrowsing results to be retrieved,
// and doesn't just return after the final (used) result.
IN_PROC_BROWSER_TEST_P(SubresourceFilterInterceptingBrowserTest,
                       SafeBrowsingNotificationsWaitOnAllRedirects) {
  // TODO(ericrobinson): If servers are slow for this test, the test will pass
  //   by default (the delay will be high due to server time rather than due
  //   to waiting on safebrowsing results).  While this won't cause flakiness,
  //   it's not ideal.  Look into using a ControllableHttpResponse for each
  //   request, and completing the first after we know the second got to
  //   the activation throttle and check that it didn't call NotifyResults.
  base::TimeDelta delay = base::Seconds(2);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  GURL redirect_url(embedded_test_server()->GetURL(
      "b.com", "/subresource_filter/frame_with_included_script.html"));
  GURL url = InitializeSafeBrowsingForOutOfOrderResponses("a.com", redirect_url,
                                                          delay);
  base::ElapsedTimer timer;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_GE(timer.Elapsed(), delay);
}

// Verify that the correct safebrowsing result is reported when there is a
// redirect chain. The
// last result should be used.
IN_PROC_BROWSER_TEST_P(SubresourceFilterInterceptingBrowserTest,
                       SafeBrowsingNotificationsCheckLastResult) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  GURL redirect_url(embedded_test_server()->GetURL(
      "b.com", "/subresource_filter/frame_with_included_script.html"));
  GURL url = InitializeSafeBrowsingForOutOfOrderResponses("a.com", redirect_url,
                                                          base::Seconds(0));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
}

INSTANTIATE_TEST_SUITE_P(All,
                         SubresourceFilterInterceptingBrowserTest,
                         ::testing::Bool());

}  // namespace subresource_filter
