// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_database_helper.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/db/safebrowsing.pb.h"
#include "components/safe_browsing/db/v4_embedded_test_server_util.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/db/v4_test_util.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
namespace subresource_filter {

// This test harness intercepts URLRequests going to the SafeBrowsing V4 server.
// It allows the tests to mock out proto responses.
class SubresourceFilterInterceptingBrowserTest
    : public SubresourceFilterBrowserTest {
 public:
  SubresourceFilterInterceptingBrowserTest()
      : safe_browsing_test_server_(
            std::make_unique<net::test_server::EmbeddedTestServer>()) {}
  ~SubresourceFilterInterceptingBrowserTest() override {}

  net::test_server::EmbeddedTestServer* safe_browsing_test_server() {
    return safe_browsing_test_server_.get();
  }

  safe_browsing::ThreatMatch GetBetterAdsMatch(const GURL& url,
                                               const std::string& bas_value) {
    safe_browsing::ThreatMatch threat_match;
    threat_match.set_threat_type(safe_browsing::SUBRESOURCE_FILTER);
    threat_match.set_platform_type(
        safe_browsing::GetUrlSubresourceFilterId().platform_type());
    threat_match.set_threat_entry_type(safe_browsing::URL);

    safe_browsing::FullHash enforce_full_hash =
        safe_browsing::V4ProtocolManagerUtil::GetFullHash(url);
    threat_match.mutable_threat()->set_hash(enforce_full_hash);
    threat_match.mutable_cache_duration()->set_seconds(300);

    safe_browsing::ThreatEntryMetadata::MetadataEntry* threat_meta =
        threat_match.mutable_threat_entry_metadata()->add_entries();
    threat_meta->set_key("sf_bas");
    threat_meta->set_value(bas_value);
    return threat_match;
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
    // from the v4 server.
    database_helper()->LocallyMarkPrefixAsBad(
        url, safe_browsing::GetUrlSubresourceFilterId());
    database_helper()->LocallyMarkPrefixAsBad(
        redirect_url, safe_browsing::GetUrlSubresourceFilterId());

    // Map URLs to policies, enforce on the initial, and warn on the redirect.
    std::map<GURL, safe_browsing::ThreatMatch> response_map{
        {url, GetBetterAdsMatch(url, "enforce")},
        {redirect_url, safe_browsing::ThreatMatch()}};
    std::map<GURL, base::TimeDelta> delay_map{{url, initial_delay}};
    // Delay the initial response , so it arrives after the final.
    safe_browsing::StartRedirectingV4RequestsForTesting(
        response_map, safe_browsing_test_server(), delay_map);
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
  // This class needs some specific test server managing to intercept V4 hash
  // requests, so just use another server for that rather than try to use the
  // parent class' server.
  std::unique_ptr<net::test_server::EmbeddedTestServer>
      safe_browsing_test_server_;
  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterInterceptingBrowserTest);
};

IN_PROC_BROWSER_TEST_F(SubresourceFilterInterceptingBrowserTest,
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
  // from the v4 server.
  database_helper()->LocallyMarkPrefixAsBad(
      enforce_url, safe_browsing::GetUrlSubresourceFilterId());
  database_helper()->LocallyMarkPrefixAsBad(
      warn_url, safe_browsing::GetUrlSubresourceFilterId());

  // Register the V4 server to handle full hash requests for the two URLs, with
  // the given ThreatMatches, then start accepting connections on the v4 server.
  std::map<GURL, safe_browsing::ThreatMatch> response_map{
      {enforce_url, GetBetterAdsMatch(enforce_url, "enforce")},
      {warn_url, GetBetterAdsMatch(warn_url, "warn")}};
  safe_browsing::StartRedirectingV4RequestsForTesting(
      response_map, safe_browsing_test_server());
  safe_browsing_test_server()->StartAcceptingConnections();

  content::ConsoleObserverDelegate enforce_console_observer(
      web_contents(), kActivationConsoleMessage);
  web_contents()->SetDelegate(&enforce_console_observer);
  ui_test_utils::NavigateToURL(browser(), enforce_url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
  EXPECT_EQ(enforce_console_observer.message(), kActivationConsoleMessage);

  content::ConsoleObserverDelegate warn_console_observer(
      web_contents(), kActivationWarningConsoleMessage);
  web_contents()->SetDelegate(&warn_console_observer);
  ui_test_utils::NavigateToURL(browser(), warn_url);
  warn_console_observer.Wait();
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
  EXPECT_EQ(warn_console_observer.message(), kActivationWarningConsoleMessage);
}

// Verify that the navigation waits on all safebrowsing results to be retrieved,
// and doesn't just return after the final (used) result.
IN_PROC_BROWSER_TEST_F(SubresourceFilterInterceptingBrowserTest,
                       SafeBrowsingNotificationsWaitOnAllRedirects) {
  // TODO(ericrobinson): If servers are slow for this test, the test will pass
  //   by default (the delay will be high due to server time rather than due
  //   to waiting on safebrowsing results).  While this won't cause flakiness,
  //   it's not ideal.  Look into using a ControllableHttpResponse for each
  //   request, and completing the first after we know the second got to
  //   the activation throttle and check that it didn't call NotifyResults.
  base::TimeDelta delay = base::TimeDelta::FromSeconds(2);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  GURL redirect_url(embedded_test_server()->GetURL(
      "b.com", "/subresource_filter/frame_with_included_script.html"));
  GURL url = InitializeSafeBrowsingForOutOfOrderResponses("a.com", redirect_url,
                                                          delay);
  base::ElapsedTimer timer;
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_GE(timer.Elapsed(), delay);
}

class SubresourceFilterInterceptingBrowserTestConsiderRedirects
    : public SubresourceFilterInterceptingBrowserTest {
 public:
  SubresourceFilterInterceptingBrowserTestConsiderRedirects() {
    feature_list_.InitAndEnableFeature(
        kSafeBrowsingSubresourceFilterConsiderRedirects);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verify that the correct safebrowsing result is reported when there is a
// redirect chain. With kSafeBrowsingSubresourceFilterConsiderRedirects, the
// result with the highest priority should be returned.
IN_PROC_BROWSER_TEST_F(
    SubresourceFilterInterceptingBrowserTestConsiderRedirects,
    SafeBrowsingNotificationsCheckBest) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  GURL redirect_url(embedded_test_server()->GetURL(
      "b.com", "/subresource_filter/frame_with_included_script.html"));
  GURL url = InitializeSafeBrowsingForOutOfOrderResponses(
      "a.com", redirect_url, base::TimeDelta::FromSeconds(0));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
}

class SubresourceFilterInterceptingBrowserTestDontConsiderRedirects
    : public SubresourceFilterInterceptingBrowserTest {
 public:
  SubresourceFilterInterceptingBrowserTestDontConsiderRedirects() {
    feature_list_.InitAndDisableFeature(
        kSafeBrowsingSubresourceFilterConsiderRedirects);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verify that the correct safebrowsing result is reported when there is a
// redirect chain. Without kSafeBrowsingSubresourceFilterConsiderRedirects, the
// last result should be used.
IN_PROC_BROWSER_TEST_F(
    SubresourceFilterInterceptingBrowserTestDontConsiderRedirects,
    SafeBrowsingNotificationsCheckLastResult) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  GURL redirect_url(embedded_test_server()->GetURL(
      "b.com", "/subresource_filter/frame_with_included_script.html"));
  GURL url = InitializeSafeBrowsingForOutOfOrderResponses(
      "a.com", redirect_url, base::TimeDelta::FromSeconds(0));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
}

}  // namespace subresource_filter
