// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/metrics/statistics_recorder.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/shared_storage_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/features.h"

namespace {

// The registerAdBeacon call in
// `chrome/test/data/interest_group/bidding_logic.js` will send
// "reserved.top_navigation" and "click" events to this URL.
constexpr char kReportingURL[] = "/_report_event_server.html";
// Used for event reporting to custom destination URLs.
constexpr char kCustomReportingURL[] = "/_custom_report_event_server.html";

constexpr char kPrivateAggregationSendHistogramReportHistogram[] =
    "PrivacySandbox.PrivateAggregation.Host.SendHistogramReportResult";

}  // namespace

class PrivacySandboxSettingsBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());

    FinishSetUp();
  }

  // Virtual so that derived classes can delay starting the server and/or
  // register different handlers.
  virtual void FinishSetUp() {
    https_server_.RegisterRequestHandler(
        base::BindRepeating(&PrivacySandboxSettingsBrowserTest::HandleRequest,
                            base::Unretained(this)));

    content::SetupCrossSiteRedirector(&https_server_);
    ASSERT_TRUE(https_server_.Start());
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    const GURL& url = request.GetURL();

    if (url.path() == "/clear_site_data_header_cookies") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->AddCustomHeader("Clear-Site-Data", "\"cookies\"");
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->set_content(std::string());
      return std::move(response);
    }

    // Use the default handler for unrelated requests.
    return nullptr;
  }

  void ClearAllCookies() {
    content::BrowsingDataRemover* remover =
        browser()->profile()->GetBrowsingDataRemover();
    content::BrowsingDataRemoverCompletionObserver observer(remover);
    remover->RemoveAndReply(
        base::Time(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_COOKIES,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, &observer);
    observer.BlockUntilCompletion();
  }

  privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings() {
    return PrivacySandboxSettingsFactory::GetForProfile(browser()->profile());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  net::EmbeddedTestServer https_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};
};

// Test that cookie clearings triggered by "Clear browsing data" will trigger
// an update to topics-data-accessible-since and invoke the corresponding
// observer method.
IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsBrowserTest, ClearAllCookies) {
  EXPECT_EQ(base::Time(),
            privacy_sandbox_settings()->TopicsDataAccessibleSince());

  privacy_sandbox_test_util::MockPrivacySandboxObserver observer;
  privacy_sandbox_settings()->AddObserver(&observer);
  EXPECT_CALL(observer, OnTopicsDataAccessibleSinceUpdated());

  ClearAllCookies();

  EXPECT_NE(base::Time(),
            privacy_sandbox_settings()->TopicsDataAccessibleSince());
}

// Test that cookie clearings triggered by Clear-Site-Data header won't trigger
// an update to topics-data-accessible-since or invoke the corresponding
// observer method.
IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsBrowserTest,
                       ClearSiteDataCookies) {
  EXPECT_EQ(base::Time(),
            privacy_sandbox_settings()->TopicsDataAccessibleSince());

  privacy_sandbox_test_util::MockPrivacySandboxObserver observer;
  privacy_sandbox_settings()->AddObserver(&observer);
  EXPECT_CALL(observer, OnTopicsDataAccessibleSinceUpdated()).Times(0);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server_.GetURL("a.test", "/clear_site_data_header_cookies")));

  EXPECT_EQ(base::Time(),
            privacy_sandbox_settings()->TopicsDataAccessibleSince());
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsBrowserTest,
                       SettingsAreNotOverridden) {
  privacy_sandbox_settings()->SetPrivacySandboxEnabled(false);
  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());
  EXPECT_FALSE(base::FeatureList::IsEnabled(
      privacy_sandbox::kOverridePrivacySandboxSettingsLocalTesting));
}

class PrivacySandboxSettingsAdsApisFlagBrowserTest
    : public PrivacySandboxSettingsBrowserTest {
 public:
  PrivacySandboxSettingsAdsApisFlagBrowserTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnablePrivacySandboxAdsApis);
  }
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsAdsApisFlagBrowserTest,
                       FollowsOverrideBehavior) {
  privacy_sandbox_settings()->SetPrivacySandboxEnabled(false);
  EXPECT_TRUE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());

  // The flag should enable this feature.
  EXPECT_TRUE(base::FeatureList::IsEnabled(
      privacy_sandbox::kOverridePrivacySandboxSettingsLocalTesting));
}

namespace {

enum class AttestedApiStatus {
  kSharedStorage,
  kProtectedAudience,
  kProtectedAudienceAndPrivateAggregation,
};

}  // namespace

class PrivacySandboxSettingsAttestationsBrowserTestBase
    : public PrivacySandboxSettingsBrowserTest {
 public:
  PrivacySandboxSettingsAttestationsBrowserTestBase() {
    attestations_feature_.InitAndEnableFeature(
        privacy_sandbox::kEnforcePrivacySandboxAttestations);
  }

  void SetUpOnMainThread() override {
    // `PrivacySandboxAttestations` has a member of type
    // `scoped_refptr<base::SequencedTaskRunner>`, its initialization must be
    // done after a browser process is created.
    PrivacySandboxSettingsBrowserTest::SetUpOnMainThread();
    scoped_attestations_ =
        std::make_unique<privacy_sandbox::ScopedPrivacySandboxAttestations>(
            privacy_sandbox::PrivacySandboxAttestations::CreateForTesting());
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

  privacy_sandbox::PrivacySandboxAttestationsGatedAPISet
  GetAttestationsGatedAPISet(AttestedApiStatus attested_api_status) {
    switch (attested_api_status) {
      case AttestedApiStatus::kSharedStorage:
        return {privacy_sandbox::PrivacySandboxAttestationsGatedAPI::
                    kSharedStorage};
      case AttestedApiStatus::kProtectedAudience:
        return {privacy_sandbox::PrivacySandboxAttestationsGatedAPI::
                    kProtectedAudience};
      case AttestedApiStatus::kProtectedAudienceAndPrivateAggregation:
        return {privacy_sandbox::PrivacySandboxAttestationsGatedAPI::
                    kProtectedAudience,
                privacy_sandbox::PrivacySandboxAttestationsGatedAPI::
                    kPrivateAggregation};
      default:
        NOTREACHED_NORETURN();
    }
  }

  void SetAttestations(std::vector<std::pair<std::string, AttestedApiStatus>>
                           hostname_strings_with_attestation_statuses) {
    privacy_sandbox::PrivacySandboxAttestationsMap attestations_map;
    for (const auto& hostname_and_status :
         hostname_strings_with_attestation_statuses) {
      attestations_map[net::SchemefulSite(
          https_server_.GetOrigin(hostname_and_status.first))] =
          GetAttestationsGatedAPISet(hostname_and_status.second);
    }
    privacy_sandbox::PrivacySandboxAttestations::GetInstance()
        ->SetAttestationsForTesting(std::move(attestations_map));
  }

  // Navigates the main frame, loads a fenced frame, then navigates the fenced
  // frame by joining an ad interest group, running an ad auction, and setting
  // the fenced frame's config to be the result of the auction.
  content::RenderFrameHost* LoadPageThenLoadAndNavigateFencedFrameViaAdAuction(
      const GURL& initial_url,
      const GURL& fenced_frame_url) {
    if (!ui_test_utils::NavigateToURL(browser(), initial_url)) {
      return nullptr;
    }

    EXPECT_TRUE(
        ExecJs(web_contents()->GetPrimaryMainFrame(),
               "var fenced_frame = document.createElement('fencedframe');"
               "fenced_frame.id = 'fenced_frame';"
               "document.body.appendChild(fenced_frame);"));
    auto* fenced_frame_node =
        fenced_frame_test_helper().GetMostRecentlyAddedFencedFrame(
            web_contents()->GetPrimaryMainFrame());
    content::TestFrameNavigationObserver observer(fenced_frame_node);
    fenced_frame_test_helper().NavigateFencedFrameUsingFledge(
        web_contents()->GetPrimaryMainFrame(), fenced_frame_url,
        "fenced_frame");
    observer.Wait();

    return fenced_frame_node;
  }

  content::RenderFrameHost*
  LoadPageThenLoadAndNavigateFencedFrameViaAdAuctionWithPrivateAggregation(
      const std::string& primary_main_frame_hostname,
      const std::string& fenced_frame_hostname) {
    GURL initial_url(https_server_.GetURL(
        primary_main_frame_hostname,
        "/allow-all-join-ad-interest-group-run-ad-auction.html"));
    GURL fenced_frame_url(https_server_.GetURL(
        fenced_frame_hostname,
        "/fenced_frames/"
        "ad_with_fenced_frame_private_aggregation_reporting.html"));

    return LoadPageThenLoadAndNavigateFencedFrameViaAdAuction(initial_url,
                                                              fenced_frame_url);
  }

  content::RenderFrameHost*
  LoadPageThenLoadAndNavigateFencedFrameViaAdAuctionForEventReporting() {
    GURL initial_url(https_server_.GetURL("a.test", "/empty.html"));
    GURL fenced_frame_url(
        https_server_.GetURL("a.test", "/fenced_frames/title1.html"));

    return LoadPageThenLoadAndNavigateFencedFrameViaAdAuction(initial_url,
                                                              fenced_frame_url);
  }

 private:
  std::unique_ptr<privacy_sandbox::ScopedPrivacySandboxAttestations>
      scoped_attestations_;
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
  base::test::ScopedFeatureList attestations_feature_;
};

class PrivacySandboxSettingsEventReportingBrowserTest
    : public PrivacySandboxSettingsAttestationsBrowserTestBase {
 public:
  void FinishSetUp() override {
    // Do not start the https server at this point to allow the tests to set up
    // response listeners.
  }
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsEventReportingBrowserTest,
                       AutomaticBeaconDestinationEnrolled) {
  privacy_sandbox_settings()->SetAllPrivacySandboxAllowedForTesting();
  EXPECT_TRUE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());

  // In order to check events reported over the network, we register an HTTP
  // response interceptor for each reportEvent request we expect.
  net::test_server::ControllableHttpResponse response(&https_server_,
                                                      kReportingURL);

  ASSERT_TRUE(https_server_.Start());

  SetAttestations(
      {std::make_pair("a.test", AttestedApiStatus::kProtectedAudience),
       std::make_pair("d.test", AttestedApiStatus::kProtectedAudience)});

  content::RenderFrameHost* fenced_frame_node =
      LoadPageThenLoadAndNavigateFencedFrameViaAdAuctionForEventReporting();
  ASSERT_NE(fenced_frame_node, nullptr);

  // Set the automatic beacon
  constexpr char kBeaconMessage[] = "this is the message";
  EXPECT_TRUE(ExecJs(fenced_frame_node, content::JsReplace(R"(
      window.fence.setReportEventDataForAutomaticBeacons({
        eventType: 'reserved.top_navigation',
        eventData: $1,
        destination: ['buyer']
      });
    )",
                                                           kBeaconMessage)));

  GURL navigation_url(https_server_.GetURL("a.test", "/title2.html"));
  EXPECT_TRUE(
      ExecJs(fenced_frame_node,
             content::JsReplace("window.open($1, '_blank');", navigation_url)));

  // Verify the automatic beacon was sent and has the correct data.
  response.WaitForRequest();
  EXPECT_EQ(response.http_request()->content, kBeaconMessage);
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsEventReportingBrowserTest,
                       AutomaticBeaconDestinationNotEnrolled) {
  privacy_sandbox_settings()->SetAllPrivacySandboxAllowedForTesting();
  EXPECT_TRUE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());

  // In order to check events reported over the network, we register an HTTP
  // response interceptor for each reportEvent request we expect.
  net::test_server::ControllableHttpResponse response(&https_server_,
                                                      kReportingURL);

  ASSERT_TRUE(https_server_.Start());

  SetAttestations(
      {std::make_pair("a.test", AttestedApiStatus::kProtectedAudience)});

  content::RenderFrameHost* fenced_frame_node =
      LoadPageThenLoadAndNavigateFencedFrameViaAdAuctionForEventReporting();
  ASSERT_NE(fenced_frame_node, nullptr);

  // Set the automatic beacon
  constexpr char kBeaconMessage[] = "this is the message";
  EXPECT_TRUE(ExecJs(fenced_frame_node, content::JsReplace(R"(
      window.fence.setReportEventDataForAutomaticBeacons({
        eventType: 'reserved.top_navigation',
        eventData: $1,
        destination: ['buyer']
      });
    )",
                                                           kBeaconMessage)));

  GURL navigation_url(https_server_.GetURL("a.test", "/title2.html"));
  EXPECT_TRUE(
      ExecJs(fenced_frame_node,
             content::JsReplace("window.open($1, '_blank');", navigation_url)));

  // Verify the automatic beacon was not sent.
  fenced_frame_test_helper().SendBasicRequest(
      web_contents(), https_server_.GetURL("d.test", kReportingURL),
      "response");
  response.WaitForRequest();
  EXPECT_EQ(response.http_request()->content, "response");
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsEventReportingBrowserTest,
                       ReportEventDestinationEnrolled) {
  privacy_sandbox_settings()->SetAllPrivacySandboxAllowedForTesting();
  EXPECT_TRUE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());

  // In order to check events reported over the network, we register an HTTP
  // response interceptor for each reportEvent request we expect.
  net::test_server::ControllableHttpResponse response(&https_server_,
                                                      kReportingURL);

  ASSERT_TRUE(https_server_.Start());

  SetAttestations(
      {std::make_pair("a.test", AttestedApiStatus::kProtectedAudience),
       std::make_pair("d.test", AttestedApiStatus::kProtectedAudience)});

  content::RenderFrameHost* fenced_frame_node =
      LoadPageThenLoadAndNavigateFencedFrameViaAdAuctionForEventReporting();
  ASSERT_NE(fenced_frame_node, nullptr);

  // Send the report to an enum destination.
  constexpr char kBeaconMessage[] = "this is the message";
  EXPECT_TRUE(
      ExecJs(fenced_frame_node, content::JsReplace(R"(
      window.fence.reportEvent({
        eventType: $1,
        eventData: $2,
        destination: ['buyer']
      });
    )",
                                                   "click", kBeaconMessage)));

  // Verify the beacon was sent and has the correct data.
  response.WaitForRequest();
  EXPECT_EQ(response.http_request()->content, kBeaconMessage);
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsEventReportingBrowserTest,
                       ReportEventCustomURLDestinationEnrolled) {
  privacy_sandbox_settings()->SetAllPrivacySandboxAllowedForTesting();
  EXPECT_TRUE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());

  // In order to check events reported over the network, we register an HTTP
  // response interceptor for each reportEvent request we expect.
  net::test_server::ControllableHttpResponse response(&https_server_,
                                                      kCustomReportingURL);

  ASSERT_TRUE(https_server_.Start());

  SetAttestations(
      {std::make_pair("a.test", AttestedApiStatus::kProtectedAudience),
       std::make_pair("d.test", AttestedApiStatus::kProtectedAudience)});

  content::RenderFrameHost* fenced_frame_node =
      LoadPageThenLoadAndNavigateFencedFrameViaAdAuctionForEventReporting();
  ASSERT_NE(fenced_frame_node, nullptr);

  // Send the report to a custom URL destination.
  GURL destinationURL = https_server_.GetURL("a.test", kCustomReportingURL);
  EXPECT_TRUE(
      ExecJs(fenced_frame_node, content::JsReplace(R"(
      window.fence.reportEvent({destinationURL: $1});
    )",
                                                   destinationURL.spec())));

  // Verify the beacon was sent as a GET request.
  response.WaitForRequest();
  EXPECT_EQ(response.http_request()->method, net::test_server::METHOD_GET);
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsEventReportingBrowserTest,
                       ReportEventDestinationNotEnrolled) {
  privacy_sandbox_settings()->SetAllPrivacySandboxAllowedForTesting();
  EXPECT_TRUE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());

  // In order to check events reported over the network, we register an HTTP
  // response interceptor for each reportEvent request we expect.
  net::test_server::ControllableHttpResponse response(&https_server_,
                                                      kReportingURL);

  ASSERT_TRUE(https_server_.Start());

  SetAttestations(
      {std::make_pair("a.test", AttestedApiStatus::kProtectedAudience),
       std::make_pair("d.test", AttestedApiStatus::kSharedStorage)});

  content::RenderFrameHost* fenced_frame_node =
      LoadPageThenLoadAndNavigateFencedFrameViaAdAuctionForEventReporting();
  ASSERT_NE(fenced_frame_node, nullptr);

  // Send the report to an enum destination.
  constexpr char kBeaconMessage[] = "this is the message";
  EXPECT_TRUE(
      ExecJs(fenced_frame_node, content::JsReplace(R"(
      window.fence.reportEvent({
        eventType: $1,
        eventData: $2,
        destination: ['buyer']
      });
    )",
                                                   "click", kBeaconMessage)));

  // Verify the beacon was not sent.
  fenced_frame_test_helper().SendBasicRequest(
      web_contents(), https_server_.GetURL("d.test", kReportingURL),
      "response");
  response.WaitForRequest();
  EXPECT_EQ(response.http_request()->content, "response");
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsEventReportingBrowserTest,
                       ReportEventCustomURLDestinationNotEnrolled) {
  privacy_sandbox_settings()->SetAllPrivacySandboxAllowedForTesting();
  EXPECT_TRUE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());

  // In order to check events reported over the network, we register an HTTP
  // response interceptor for each reportEvent request we expect.
  net::test_server::ControllableHttpResponse response(&https_server_,
                                                      kCustomReportingURL);

  ASSERT_TRUE(https_server_.Start());

  SetAttestations(
      {std::make_pair("a.test", AttestedApiStatus::kProtectedAudience),
       std::make_pair("d.test", AttestedApiStatus::kSharedStorage)});

  content::RenderFrameHost* fenced_frame_node =
      LoadPageThenLoadAndNavigateFencedFrameViaAdAuctionForEventReporting();
  ASSERT_NE(fenced_frame_node, nullptr);

  // Send the report to a custom URL destination.
  GURL destinationURL = https_server_.GetURL("d.test", kCustomReportingURL);
  EXPECT_TRUE(
      ExecJs(fenced_frame_node, content::JsReplace(R"(
      window.fence.reportEvent({destinationURL: $1});
    )",
                                                   destinationURL.spec())));

  // Verify the beacon was not sent.
  fenced_frame_test_helper().SendBasicRequest(
      web_contents(), https_server_.GetURL("d.test", kCustomReportingURL),
      "response");
  response.WaitForRequest();
  EXPECT_EQ(response.http_request()->content, "response");
}

class
    PrivacySandboxSettingsAttestPrivateAggregationInProtectedAudienceBrowserTest
    : public PrivacySandboxSettingsAttestationsBrowserTestBase {
 public:
  PrivacySandboxSettingsAttestPrivateAggregationInProtectedAudienceBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {blink::features::kPrivateAggregationApi,
         blink::features::kInterestGroupStorage,
         blink::features::kAdInterestGroupAPI, blink::features::kFledge,
         blink::features::kFledgeBiddingAndAuctionServer,
         blink::features::kFencedFrames,
         blink::features::kFencedFramesAPIChanges,
         privacy_sandbox::kOverridePrivacySandboxSettingsLocalTesting},
        /*disabled_features=*/{});
  }

  void FinishSetUp() override {
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &PrivacySandboxSettingsAttestPrivateAggregationInProtectedAudienceBrowserTest::
            HandleWellKnownRequest,
        base::Unretained(this)));
    content::SetupCrossSiteRedirector(&https_server_);
    ASSERT_TRUE(https_server_.Start());
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleWellKnownRequest(
      const net::test_server::HttpRequest& request) {
    if (!base::StartsWith(request.relative_url,
                          "/.well-known/interest-group/permissions/?origin=")) {
      return nullptr;
    }

    // .well-known requests should advertise they accept JSON responses.
    const auto accept_header =
        request.headers.find(net::HttpRequestHeaders::kAccept);
    DCHECK(accept_header != request.headers.end());
    EXPECT_EQ(accept_header->second, "application/json");

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content_type("application/json");
    response->set_content(R"({"joinAdInterestGroup" : true})");
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    return response;
  }

  size_t GetTotalSampleCount(const std::string& histogram_name) {
    auto buckets = histogram_tester_.GetAllSamples(histogram_name);
    size_t count = 0;
    for (const auto& bucket : buckets) {
      count += bucket.count;
    }
    return count;
  }

  void WaitForHistogram(const std::string& histogram_name,
                        size_t expected_sample_count) {
    // Continue if histogram was already recorded and has at least the expected
    // number of samples.
    if (base::StatisticsRecorder::FindHistogram(histogram_name) &&
        GetTotalSampleCount(histogram_name) >= expected_sample_count) {
      return;
    }

    // Else, wait until the histogram is recorded with enough samples.
    base::RunLoop run_loop;
    auto histogram_observer = std::make_unique<
        base::StatisticsRecorder::ScopedHistogramSampleObserver>(
        histogram_name,
        base::BindLambdaForTesting([&](const char* histogram_name,
                                       uint64_t name_hash,
                                       base::HistogramBase::Sample sample) {
          if (GetTotalSampleCount(histogram_name) >= expected_sample_count) {
            run_loop.Quit();
          }
        }));
    run_loop.Run();
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    PrivacySandboxSettingsAttestPrivateAggregationInProtectedAudienceBrowserTest,
    SameOrigin_Enrolled_Success) {
  privacy_sandbox_settings()->SetAllPrivacySandboxAllowedForTesting();
  EXPECT_TRUE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());

  SetAttestations({std::make_pair(
      "a.test", AttestedApiStatus::kProtectedAudienceAndPrivateAggregation)});

  content::RenderFrameHost* fenced_frame_node =
      LoadPageThenLoadAndNavigateFencedFrameViaAdAuctionWithPrivateAggregation(
          /*primary_main_frame_hostname=*/"a.test",
          /*fenced_frame_hostname=*/"a.test");
  ASSERT_NE(fenced_frame_node, nullptr);

  WaitForHistogram(kPrivateAggregationSendHistogramReportHistogram, 2);
  histogram_tester_.ExpectUniqueSample(
      kPrivateAggregationSendHistogramReportHistogram,
      content::GetPrivateAggregationSendHistogramSuccessValue(), 2);
}

IN_PROC_BROWSER_TEST_F(
    PrivacySandboxSettingsAttestPrivateAggregationInProtectedAudienceBrowserTest,
    SameOrigin_NotEnrolled_Failure) {
  privacy_sandbox_settings()->SetAllPrivacySandboxAllowedForTesting();
  EXPECT_TRUE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());

  SetAttestations(
      {std::make_pair("a.test", AttestedApiStatus::kProtectedAudience)});

  content::RenderFrameHost* fenced_frame_node =
      LoadPageThenLoadAndNavigateFencedFrameViaAdAuctionWithPrivateAggregation(
          /*primary_main_frame_hostname=*/"a.test",
          /*fenced_frame_hostname=*/"a.test");
  ASSERT_NE(fenced_frame_node, nullptr);

  WaitForHistogram(kPrivateAggregationSendHistogramReportHistogram, 2);
  histogram_tester_.ExpectUniqueSample(
      kPrivateAggregationSendHistogramReportHistogram,
      content::GetPrivateAggregationSendHistogramApiDisabledValue(), 2);
}

IN_PROC_BROWSER_TEST_F(
    PrivacySandboxSettingsAttestPrivateAggregationInProtectedAudienceBrowserTest,
    CrossOrigin_Enrolled_Success) {
  privacy_sandbox_settings()->SetAllPrivacySandboxAllowedForTesting();
  EXPECT_TRUE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());

  SetAttestations(
      {std::make_pair(
           "a.test",
           AttestedApiStatus::kProtectedAudienceAndPrivateAggregation),
       std::make_pair(
           "b.test",
           AttestedApiStatus::kProtectedAudienceAndPrivateAggregation)});

  content::RenderFrameHost* fenced_frame_node =
      LoadPageThenLoadAndNavigateFencedFrameViaAdAuctionWithPrivateAggregation(
          /*primary_main_frame_hostname=*/"a.test",
          /*fenced_frame_hostname=*/"b.test");
  ASSERT_NE(fenced_frame_node, nullptr);

  WaitForHistogram(kPrivateAggregationSendHistogramReportHistogram, 2);
  histogram_tester_.ExpectUniqueSample(
      kPrivateAggregationSendHistogramReportHistogram,
      content::GetPrivateAggregationSendHistogramSuccessValue(), 2);
}

IN_PROC_BROWSER_TEST_F(
    PrivacySandboxSettingsAttestPrivateAggregationInProtectedAudienceBrowserTest,
    CrossOrigin_NotEnrolled_Failure) {
  privacy_sandbox_settings()->SetAllPrivacySandboxAllowedForTesting();
  EXPECT_TRUE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());

  SetAttestations(
      {std::make_pair(
           "a.test",
           AttestedApiStatus::kProtectedAudienceAndPrivateAggregation),
       std::make_pair("b.test", AttestedApiStatus::kProtectedAudience)});

  content::RenderFrameHost* fenced_frame_node =
      LoadPageThenLoadAndNavigateFencedFrameViaAdAuctionWithPrivateAggregation(
          /*primary_main_frame_hostname=*/"a.test",
          /*fenced_frame_hostname=*/"b.test");
  ASSERT_NE(fenced_frame_node, nullptr);

  WaitForHistogram(kPrivateAggregationSendHistogramReportHistogram, 2);
  histogram_tester_.ExpectUniqueSample(
      kPrivateAggregationSendHistogramReportHistogram,
      content::GetPrivateAggregationSendHistogramApiDisabledValue(), 2);
}
