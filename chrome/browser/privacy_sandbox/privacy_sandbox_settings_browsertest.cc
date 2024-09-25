// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_settings.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/metrics/statistics_recorder.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations_mixin.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/fenced_frame_reporter_observer.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/shared_storage_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"

namespace {

// The registerAdBeacon call in
// `chrome/test/data/interest_group/bidding_logic.js` will send
// "reserved.top_navigation" and "click" events to this URL.
constexpr char kReportingURL[] = "/_report_event_server.html";
// Used for event reporting to custom destination URLs.
constexpr char kCustomReportingURL[] = "/_custom_report_event_server.html";

// Used for reportWin() destination.
constexpr char kBidderReportURL[] = "/bidder_report";

// Used for reportResult() destination.
constexpr char kSellerReportURL[] = "/seller_report";

constexpr char kPrivateAggregationHostPipeResultHistogram[] =
    "PrivacySandbox.PrivateAggregation.Host.PipeResult";

// Used to pattern match console error message emitted from
// `FencedFrameReporter::SendReportInternal()`. This error applies to
// `reportEvent()` and automatic beacons.
constexpr char kFencedFrameReportingDestinationNotAttested[] =
    "The reporting destination * is not attested for *";

// Used to pattern match console error message emitted from
// `InterestGroupAuctionReporter::CheckReportUrl()`. This error applies to
// `reportWin()` and `reportResult()`.
constexpr char kInterestGroupReportingDestinationNotAttested[] =
    "Worklet error: The reporting destination * is not attested for Protected "
    "Audience.";

// The template to run `navigator.joinAdInterestGroup()`.
// $1: The ad render url.
// $2: The bidding logic url.
// $3: Name of the interest group.
// $4: Url that a custom macro reporting beacon is allowed to be sent to.
constexpr char kJoinAdInterestGroupScript[] = R"(
  (async() => {
    const page_origin = new URL($1).origin;
    const bidding_url = new URL($2, page_origin);
    const interest_group = {
      name: $3,
      owner: page_origin,
      biddingLogicUrl: bidding_url,
      ads: [{renderURL: $1, bid: 1, allowedReportingOrigins: [$4]}],
    };

    // Pick an arbitrarily high duration to guarantee that we never leave the
    // ad interest group while the test runs. This join will fail silently
    // because of attestations failure.
    await navigator.joinAdInterestGroup(
        interest_group, /*durationSeconds=*/3000000);
  })()
)";

// The template to run `navigator.runAdAuction()`. Upon success, it also
// navigates the existing fenced frame with id "fenced_frame" in the page to the
// winning ad url.
// $1: The ad render url.
// $2: The decision logic url.
// $3: The url a reportResult beacon will be sent to if the decision logic
// is `decision_logic_report_to_seller_signals.js`.
constexpr char kRunAdAuctionAndNavigateFencedFrameScript[] = R"(
  (async() => {
    const page_origin = new URL($1).origin;
    const auction_config = {
      seller: page_origin,
      interestGroupBuyers: [page_origin],
      decisionLogicURL: new URL($2, page_origin),
      sellerSignals: {reportTo: $3}
    };
    auction_config.resolveToConfig = true;

    const fenced_frame_config = await navigator.runAdAuction(auction_config);
    if (fenced_frame_config === null) {
      return "null auction result";
    } else if (!(fenced_frame_config instanceof FencedFrameConfig)) {
      return "did not return a FencedFrameConfig";
    } else {
      document.getElementById("fenced_frame").config = fenced_frame_config;
      return "success";
    }
  })()
)";

// Print more readable logs for PrivacySandboxSettingsEventReportingBrowserTest.
auto describe_params = [](const auto& info) {
  return base::StrCat(
      {"AttestedFor_", ConvertAttestedApiStatusToString(info.param)});
};

auto console_error_filter =
    [](const content::WebContentsConsoleObserver::Message& message) {
      return message.log_level == blink::mojom::ConsoleMessageLevel::kError;
    };

}  // namespace

class PrivacySandboxSettingsBrowserTest
    : public MixinBasedInProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());

    FinishSetUp();
  }

  // TODO(crbug.com/40285326): This fails with the field trial testing config.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
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

namespace {

enum class AttestedApiStatus {
  kSharedStorage,
  kProtectedAudience,
  kProtectedAudienceAndPrivateAggregation,
  kAttributionReporting,
  kNone,
};

std::string ConvertAttestedApiStatusToString(
    AttestedApiStatus attested_api_status) {
  switch (attested_api_status) {
    case AttestedApiStatus::kSharedStorage:
      return "SharedStorage";
    case AttestedApiStatus::kProtectedAudience:
      return "ProtectedAudience";
    case AttestedApiStatus::kProtectedAudienceAndPrivateAggregation:
      return "ProtectedAudience_and_PrivateAggregation";
    case AttestedApiStatus::kAttributionReporting:
      return "AttributionReporting";
    case AttestedApiStatus::kNone:
      return "None";
    default:
      NOTREACHED();
  }
}

}  // namespace

class PrivacySandboxSettingsAttestationsBrowserTestBase
    : public PrivacySandboxSettingsBrowserTest {
 public:
  PrivacySandboxSettingsAttestationsBrowserTestBase() = default;

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
      case AttestedApiStatus::kAttributionReporting:
        return {privacy_sandbox::PrivacySandboxAttestationsGatedAPI::
                    kAttributionReporting};
      case AttestedApiStatus::kNone:
        return {};
      default:
        NOTREACHED();
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
  // Note: The bidding and decision urls used in
  // `NavigateFencedFrameUsingFledge()` are pointing to files under
  // "content/test/data". This test file is in "browser/", so if the script is
  // copied and directly executed here, the bidding and decision urls will be
  // referring to files under "chrome/test/data".
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
    auto* fenced_frame_rfh =
        fenced_frame_test_helper().GetMostRecentlyAddedFencedFrame(
            web_contents()->GetPrimaryMainFrame());
    content::TestFrameNavigationObserver observer(fenced_frame_rfh);
    fenced_frame_test_helper().NavigateFencedFrameUsingFledge(
        web_contents()->GetPrimaryMainFrame(), fenced_frame_url,
        "fenced_frame");
    observer.Wait();

    // Embedder-initiated fenced frame navigation uses a new browsing instance.
    // Fenced frame RenderFrameHost is a new one after navigation, so we need
    // to retrieve it.
    return fenced_frame_test_helper().GetMostRecentlyAddedFencedFrame(
        web_contents()->GetPrimaryMainFrame());
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
  privacy_sandbox::PrivacySandboxAttestationsMixin
      privacy_sandbox_attestations_mixin_{&mixin_host_};
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
};

class PrivacySandboxSettingsEventReportingBrowserTest
    : public PrivacySandboxSettingsAttestationsBrowserTestBase,
      public testing::WithParamInterface<AttestedApiStatus> {
 public:
  PrivacySandboxSettingsEventReportingBrowserTest() = default;

  void FinishSetUp() override {
    // Do not start the https server at this point to allow the tests to set up
    // response listeners.
  }

  void SetUpOnMainThread() override {
    // Allows all Privacy Sandbox prefs for testing.
    privacy_sandbox_settings()->SetAllPrivacySandboxAllowedForTesting();
    EXPECT_TRUE(
        privacy_sandbox_settings()->IsAttributionReportingEverAllowed());

    // Set up the observer to listen for console error messages.
    PrivacySandboxSettingsAttestationsBrowserTestBase::SetUpOnMainThread();
    console_error_observer_ =
        std::make_unique<content::WebContentsConsoleObserver>(web_contents());
    console_error_observer_->SetFilter(
        base::BindRepeating(console_error_filter));
  }

  AttestedApiStatus GetReportingDestinationAttestationStatus() {
    return GetParam();
  }

  bool IsReportingDestinationEnrolled() {
    return GetReportingDestinationAttestationStatus() ==
           AttestedApiStatus::kProtectedAudience;
  }

 protected:
  std::unique_ptr<content::WebContentsConsoleObserver> console_error_observer_;
};

IN_PROC_BROWSER_TEST_P(PrivacySandboxSettingsEventReportingBrowserTest,
                       AutomaticBeaconDestinationEnrollment) {
  // In order to check events reported over the network, we register an HTTP
  // response interceptor for each reportEvent request we expect.
  net::test_server::ControllableHttpResponse response(&https_server_,
                                                      kReportingURL);

  ASSERT_TRUE(https_server_.Start());

  // Set automatic beacon reporting destination to be attested according to the
  // test parameter.
  SetAttestations(
      {std::make_pair("a.test", AttestedApiStatus::kProtectedAudience),
       std::make_pair("d.test", GetReportingDestinationAttestationStatus())});

  content::RenderFrameHost* fenced_frame_rfh =
      LoadPageThenLoadAndNavigateFencedFrameViaAdAuctionForEventReporting();
  ASSERT_NE(fenced_frame_rfh, nullptr);

  // Set the automatic beacon.
  constexpr char kBeaconMessage[] = "this is the message";

  // Install the beacon observer to observe whether the beacon is queued to be
  // sent later.
  std::unique_ptr<content::test::FencedFrameReporterObserverForTesting>
      beacon_observer = content::test::InstallFencedFrameReporterObserver(
          fenced_frame_rfh,
          content::AutomaticBeaconEvent(
              blink::mojom::AutomaticBeaconType::kDeprecatedTopNavigation,
              kBeaconMessage));

  // Listen to the console error message from
  // `FencedFrameReporter::SendReportInternal()`.
  console_error_observer_->SetPattern(
      kFencedFrameReportingDestinationNotAttested);

  EXPECT_TRUE(ExecJs(
      fenced_frame_rfh,
      content::JsReplace(R"(
      window.fence.setReportEventDataForAutomaticBeacons({
        eventType: $1,
        eventData: $2,
        destination: ['buyer']
      });
    )",
                         blink::kDeprecatedFencedFrameTopNavigationBeaconType,
                         kBeaconMessage)));

  // Commit a top-level navigation.
  GURL navigation_url(https_server_.GetURL("a.test", "/title2.html"));
  EXPECT_TRUE(
      ExecJs(fenced_frame_rfh,
             content::JsReplace("window.open($1, '_blank');", navigation_url)));

  if (IsReportingDestinationEnrolled()) {
    // The automatic beacon destination is considered enrolled if attested for
    // Protected Audience.
    response.WaitForRequest();
    EXPECT_EQ(response.http_request()->content, kBeaconMessage);
  } else {
    // The console error is ignored if the reporting event is queued to be sent
    // later when fenced frame url mapping is ready.
    if (!beacon_observer->IsReportingEventQueued()) {
      // The console message should state different attestation requirement
      // based on the feature toggle.
      ASSERT_TRUE(console_error_observer_->Wait());
      EXPECT_EQ(console_error_observer_->messages().size(), 1u);
      EXPECT_TRUE(base::Contains(console_error_observer_->GetMessageAt(0u),
                                 "Protected Audience"));
    }

    // Verify the automatic beacon was not sent.
    fenced_frame_test_helper().SendBasicRequest(
        web_contents(), https_server_.GetURL("d.test", kReportingURL),
        "response");
    response.WaitForRequest();
    EXPECT_EQ(response.http_request()->content, "response");
  }
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxSettingsEventReportingBrowserTest,
                       ReportEventDestinationEnrollment) {
  // In order to check events reported over the network, we register an HTTP
  // response interceptor for each reportEvent request we expect.
  net::test_server::ControllableHttpResponse response(&https_server_,
                                                      kReportingURL);

  ASSERT_TRUE(https_server_.Start());

  // Set reportEvent reporting destination to be attested according to the
  // test parameter.
  SetAttestations(
      {std::make_pair("a.test", AttestedApiStatus::kProtectedAudience),
       std::make_pair("d.test", GetReportingDestinationAttestationStatus())});

  content::RenderFrameHost* fenced_frame_rfh =
      LoadPageThenLoadAndNavigateFencedFrameViaAdAuctionForEventReporting();
  ASSERT_NE(fenced_frame_rfh, nullptr);

  // Send the report to an enum destination.
  constexpr char kBeaconMessage[] = "this is the message";
  constexpr char kEventType[] = "click";

  // Install the beacon observer to observe whether the beacon is queued to be
  // sent later.
  std::unique_ptr<content::test::FencedFrameReporterObserverForTesting>
      beacon_observer = content::test::InstallFencedFrameReporterObserver(
          fenced_frame_rfh,
          content::DestinationEnumEvent(kEventType, kBeaconMessage));

  // Listen to the console error message from
  // `FencedFrameReporter::SendReportInternal()`.
  console_error_observer_->SetPattern(
      kFencedFrameReportingDestinationNotAttested);

  EXPECT_TRUE(
      ExecJs(fenced_frame_rfh, content::JsReplace(R"(
      window.fence.reportEvent({
        eventType: $1,
        eventData: $2,
        destination: ['buyer']
      });
    )",
                                                  kEventType, kBeaconMessage)));

  if (IsReportingDestinationEnrolled()) {
    // The reportEvent beacon destination is considered enrolled if attested for
    // Protected Audience.
    response.WaitForRequest();
    EXPECT_EQ(response.http_request()->content, kBeaconMessage);
  } else {
    // The console error is ignored if the reporting event is queued to be sent
    // later when fenced frame url mapping is ready.
    if (!beacon_observer->IsReportingEventQueued()) {
      // The console message should state different attestation requirement
      // based on the feature toggle.
      ASSERT_TRUE(console_error_observer_->Wait());
      EXPECT_EQ(console_error_observer_->messages().size(), 1u);
      EXPECT_TRUE(base::Contains(console_error_observer_->GetMessageAt(0u),
                                 "Protected Audience"));
    }

    // Verify the reportEvent beacon was not sent.
    fenced_frame_test_helper().SendBasicRequest(
        web_contents(), https_server_.GetURL("d.test", kReportingURL),
        "response");
    response.WaitForRequest();
    EXPECT_EQ(response.http_request()->content, "response");
  }
}

IN_PROC_BROWSER_TEST_P(PrivacySandboxSettingsEventReportingBrowserTest,
                       ReportEventCustomURLDestinationEnrollment) {
  // In order to check events reported over the network, we register an HTTP
  // response interceptor for each reportEvent request we expect.
  net::test_server::ControllableHttpResponse response(&https_server_,
                                                      kCustomReportingURL);

  ASSERT_TRUE(https_server_.Start());

  // Set custom url reporting destination to be attested according to the
  // test parameter.
  SetAttestations(
      {std::make_pair("a.test", AttestedApiStatus::kProtectedAudience),
       std::make_pair("d.test", GetReportingDestinationAttestationStatus())});

  GURL initial_url(https_server_.GetURL("a.test", "/empty.html"));
  GURL fenced_frame_url(
      https_server_.GetURL("a.test", "/fenced_frames/title1.html"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Create the fenced frame.
  EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                     "var fenced_frame = document.createElement('fencedframe');"
                     "fenced_frame.id = 'fenced_frame';"
                     "document.body.appendChild(fenced_frame);"));
  content::RenderFrameHost* fenced_frame_rfh =
      fenced_frame_test_helper().GetMostRecentlyAddedFencedFrame(
          web_contents()->GetPrimaryMainFrame());
  content::TestFrameNavigationObserver observer(fenced_frame_rfh);

  // Send the report to a custom URL destination.
  GURL destination_url = https_server_.GetURL("d.test", kCustomReportingURL);

  EXPECT_TRUE(
      ExecJs(web_contents()->GetPrimaryMainFrame(),
             content::JsReplace(kJoinAdInterestGroupScript, fenced_frame_url,
                                "/interest_group/bidding_logic.js", "testAd",
                                destination_url)));

  content::EvalJsResult auction_result =
      EvalJs(web_contents()->GetPrimaryMainFrame(),
             content::JsReplace(kRunAdAuctionAndNavigateFencedFrameScript,
                                fenced_frame_url,
                                "/interest_group/decision_logic.js", ""));

  if (IsReportingDestinationEnrolled()) {
    // The custom url destination is considered enrolled if attested for
    // Protected Audience.
    ASSERT_EQ(auction_result.ExtractString(), "success");

    observer.Wait();
    ASSERT_EQ(observer.last_committed_url(), fenced_frame_url);

    // Embedder-initiated fenced frame navigation uses a new browsing instance.
    // Fenced frame RenderFrameHost is a new one after navigation, so we need
    // to retrieve it.
    fenced_frame_rfh =
        fenced_frame_test_helper().GetMostRecentlyAddedFencedFrame(
            web_contents()->GetPrimaryMainFrame());
    ASSERT_NE(fenced_frame_rfh, nullptr);

    // Send the beacon.
    EXPECT_TRUE(ExecJs(fenced_frame_rfh, content::JsReplace(R"(
      window.fence.reportEvent({destinationURL: $1});
    )",
                                                            destination_url)));

    // Verify the beacon was sent as a GET request.
    response.WaitForRequest();
    EXPECT_EQ(response.http_request()->method, net::test_server::METHOD_GET);
  } else {
    // Joining ad interest group an url that is not enrolled in
    // `allowedReportingOrigins` will fail silently. The auction later will not
    // return a winning ad.
    // TODO(xiaochenzh): The current behavior for `joinAdInterestGroup()` when
    // urls in `allowedReportingOrigins` are not attested is to fail silently.
    // Soon an error message will be added. This test should be updated to
    // listen to this error then.
    ASSERT_EQ(auction_result, "null auction result");

    // Verify the beacon was not sent.
    fenced_frame_test_helper().SendBasicRequest(
        web_contents(), https_server_.GetURL("d.test", kCustomReportingURL),
        "response");
    response.WaitForRequest();
    EXPECT_EQ(response.http_request()->content, "response");
  }
}

// For beacons from `reportWin()`, the reporting destination is considered
// enrolled if and only if it is attested for Protected Audience. The relaxed
// attestations requirement of either Protected Audience or Attribution
// Reporting for post-impression beacons from M120 should not apply to beacons
// from `reportWin()`.
IN_PROC_BROWSER_TEST_P(PrivacySandboxSettingsEventReportingBrowserTest,
                       ReportWinDestinationEnrollment) {
  // In order to check events reported over the network, we register an HTTP
  // response interceptor for each reporting beacon we expect.
  net::test_server::ControllableHttpResponse response(&https_server_,
                                                      kBidderReportURL);
  ASSERT_TRUE(https_server_.Start());

  // Set `reportWin()` reporting destination to be attested according to the
  // test parameter.
  SetAttestations(
      {std::make_pair("a.test", AttestedApiStatus::kProtectedAudience),
       std::make_pair("b.test", GetReportingDestinationAttestationStatus())});

  GURL initial_url(https_server_.GetURL("a.test", "/empty.html"));
  GURL fenced_frame_url(
      https_server_.GetURL("a.test", "/fenced_frames/title1.html"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Create the fenced frame.
  EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                     "var fenced_frame = document.createElement('fencedframe');"
                     "fenced_frame.id = 'fenced_frame';"
                     "document.body.appendChild(fenced_frame);"));
  content::RenderFrameHost* fenced_frame_rfh =
      fenced_frame_test_helper().GetMostRecentlyAddedFencedFrame(
          web_contents()->GetPrimaryMainFrame());
  content::TestFrameNavigationObserver observer(fenced_frame_rfh);

  // The `reportWin()` beacon will be attempted to send to this url.
  GURL bidder_report_to_url = https_server_.GetURL("b.test", kBidderReportURL);

  // Listen to the console error message from
  // `InterestGroupAuctionReporter::CheckReportUrl()`.
  console_error_observer_->SetPattern(
      kInterestGroupReportingDestinationNotAttested);

  // Run the ad auction with the bidding url `bidding_logic_report_to_name.js`.
  // This auction will attempt to send a `reportWin()` to the url in the
  // InterestGroup name.
  EXPECT_TRUE(ExecJs(
      web_contents()->GetPrimaryMainFrame(),
      content::JsReplace(kJoinAdInterestGroupScript, fenced_frame_url,
                         "/interest_group/bidding_logic_report_to_name.js",
                         bidder_report_to_url, fenced_frame_url)));

  content::EvalJsResult auction_result =
      EvalJs(web_contents()->GetPrimaryMainFrame(),
             content::JsReplace(kRunAdAuctionAndNavigateFencedFrameScript,
                                fenced_frame_url,
                                "/interest_group/decision_logic.js", ""));

  if (IsReportingDestinationEnrolled()) {
    // For beacons from `reportWin()`, the reporting destination is considered
    // enrolled if and only if it is attested for Protected Audience.
    ASSERT_EQ(auction_result.ExtractString(), "success");

    observer.Wait();
    ASSERT_EQ(observer.last_committed_url(), fenced_frame_url);

    // Verify the `reportWin()` beacon was sent.
    response.WaitForRequest();
    EXPECT_FALSE(response.http_request()->has_content);
    EXPECT_EQ(response.http_request()->method,
              net::test_server::HttpMethod::METHOD_GET);
  } else {
    // Verify the console messages states to require Protected Audience only.
    // Note that two console messages are sent due to debug and normal
    // reporting.
    ASSERT_TRUE(console_error_observer_->Wait());
    EXPECT_EQ(console_error_observer_->messages().size(), 2u);
    EXPECT_TRUE(base::Contains(console_error_observer_->GetMessageAt(0u),
                               "Protected Audience"));
    EXPECT_FALSE(base::Contains(console_error_observer_->GetMessageAt(0u),
                                "Attribution Reporting"));
    EXPECT_TRUE(base::Contains(console_error_observer_->GetMessageAt(1u),
                               "Protected Audience"));
    EXPECT_FALSE(base::Contains(console_error_observer_->GetMessageAt(1u),
                                "Attribution Reporting"));

    // Verify the `reportWin()` beacon was not sent.
    fenced_frame_test_helper().SendBasicRequest(
        web_contents(), bidder_report_to_url, "response");
    response.WaitForRequest();
    EXPECT_EQ(response.http_request()->content, "response");
  }
}

// For beacons from `reportResult()`, the reporting destination is considered
// enrolled if and only if it is attested for Protected Audience. The relaxed
// attestations requirement of either Protected Audience or Attribution
// Reporting for post-impression beacons from M120 should not apply to beacons
// from `reportResult()`.
IN_PROC_BROWSER_TEST_P(PrivacySandboxSettingsEventReportingBrowserTest,
                       ReportResultDestinationEnrollment) {
  // In order to check events reported over the network, we register an HTTP
  // response interceptor for each reporting beacon we expect.
  net::test_server::ControllableHttpResponse response(&https_server_,
                                                      kSellerReportURL);
  ASSERT_TRUE(https_server_.Start());

  // Set `reportResult()` reporting destination to be attested according to the
  // test parameter.
  SetAttestations(
      {std::make_pair("a.test", AttestedApiStatus::kProtectedAudience),
       std::make_pair("b.test", GetReportingDestinationAttestationStatus())});

  GURL initial_url(https_server_.GetURL("a.test", "/empty.html"));
  GURL fenced_frame_url(
      https_server_.GetURL("a.test", "/fenced_frames/title1.html"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Create the fenced frame.
  EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                     "var fenced_frame = document.createElement('fencedframe');"
                     "fenced_frame.id = 'fenced_frame';"
                     "document.body.appendChild(fenced_frame);"));
  content::RenderFrameHost* fenced_frame_rfh =
      fenced_frame_test_helper().GetMostRecentlyAddedFencedFrame(
          web_contents()->GetPrimaryMainFrame());
  content::TestFrameNavigationObserver observer(fenced_frame_rfh);

  // The `ReportResult()` beacon will be attempted to send to this url.
  GURL seller_report_to_url = https_server_.GetURL("b.test", kSellerReportURL);

  // Listen to the console error message from
  // `InterestGroupAuctionReporter::CheckReportUrl()`.
  console_error_observer_->SetPattern(
      kInterestGroupReportingDestinationNotAttested);

  // Run the ad auction with the decision url
  // `decision_logic_report_to_seller_signals.js`. This auction will attempt to
  // send a `ReportResult()` beacon to the url in the sellerSignals.
  EXPECT_TRUE(
      ExecJs(web_contents()->GetPrimaryMainFrame(),
             content::JsReplace(kJoinAdInterestGroupScript, fenced_frame_url,
                                "/interest_group/bidding_logic.js", "testAd",
                                fenced_frame_url)));

  content::EvalJsResult auction_result =
      EvalJs(web_contents()->GetPrimaryMainFrame(),
             content::JsReplace(
                 kRunAdAuctionAndNavigateFencedFrameScript, fenced_frame_url,
                 "/interest_group/decision_logic_report_to_seller_signals.js",
                 seller_report_to_url));

  if (IsReportingDestinationEnrolled()) {
    // For beacons from `reportResult()`, the reporting destination is
    // considered enrolled if and only if it is attested for Protected Audience.
    ASSERT_EQ(auction_result.ExtractString(), "success");

    observer.Wait();
    ASSERT_EQ(observer.last_committed_url(), fenced_frame_url);

    // Verify the `reportResult()` beacon was sent.
    response.WaitForRequest();
    EXPECT_FALSE(response.http_request()->has_content);
    EXPECT_EQ(response.http_request()->method,
              net::test_server::HttpMethod::METHOD_GET);
  } else {
    // Verify the console message states to require Protected Audience only.
    // Note that two console messages are sent due to debug and normal
    // reporting.
    ASSERT_TRUE(console_error_observer_->Wait());
    EXPECT_EQ(console_error_observer_->messages().size(), 2u);
    EXPECT_TRUE(base::Contains(console_error_observer_->GetMessageAt(0u),
                               "Protected Audience"));
    EXPECT_FALSE(base::Contains(console_error_observer_->GetMessageAt(0u),
                                "Attribution Reporting"));
    EXPECT_TRUE(base::Contains(console_error_observer_->GetMessageAt(1u),
                               "Protected Audience"));
    EXPECT_FALSE(base::Contains(console_error_observer_->GetMessageAt(1u),
                                "Attribution Reporting"));

    // Verify the `reportResult()` beacon was not sent.
    fenced_frame_test_helper().SendBasicRequest(
        web_contents(), seller_report_to_url, "response");
    response.WaitForRequest();
    EXPECT_EQ(response.http_request()->content, "response");
  }
}

INSTANTIATE_TEST_SUITE_P(
    PrivacySandboxSettingsEventReportingBrowserTest,
    PrivacySandboxSettingsEventReportingBrowserTest,
    testing::Values(AttestedApiStatus::kProtectedAudience,
                    AttestedApiStatus::kAttributionReporting,
                    AttestedApiStatus::kNone),
    describe_params);

class PrivacySandboxSettingsAttestProtectedAudienceBrowserTest
    : public PrivacySandboxSettingsAttestationsBrowserTestBase {
 public:
  PrivacySandboxSettingsAttestProtectedAudienceBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {blink::features::kInterestGroupStorage,
         blink::features::kAdInterestGroupAPI, blink::features::kFledge,
         blink::features::kFledgeBiddingAndAuctionServer,
         blink::features::kFencedFrames,
         blink::features::kFencedFramesAPIChanges,
         privacy_sandbox::kOverridePrivacySandboxSettingsLocalTesting},
        /*disabled_features=*/{});
  }

  void FinishSetUp() override {
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &PrivacySandboxSettingsAttestProtectedAudienceBrowserTest::
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
    CHECK(accept_header != request.headers.end());
    EXPECT_EQ(accept_header->second, "application/json");

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content_type("application/json");
    response->set_content(R"({"joinAdInterestGroup" : true})");
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    return response;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class
    PrivacySandboxSettingsAttestPrivateAggregationInProtectedAudienceBrowserTest
    : public PrivacySandboxSettingsAttestProtectedAudienceBrowserTest {
 public:
  PrivacySandboxSettingsAttestPrivateAggregationInProtectedAudienceBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {blink::features::kPrivateAggregationApi},
        /*disabled_features=*/{});
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
  SetAttestations({std::make_pair(
      "a.test", AttestedApiStatus::kProtectedAudienceAndPrivateAggregation)});

  content::RenderFrameHost* fenced_frame_rfh =
      LoadPageThenLoadAndNavigateFencedFrameViaAdAuctionWithPrivateAggregation(
          /*primary_main_frame_hostname=*/"a.test",
          /*fenced_frame_hostname=*/"a.test");
  ASSERT_NE(fenced_frame_rfh, nullptr);

  WaitForHistogram(kPrivateAggregationHostPipeResultHistogram, 2);
  histogram_tester_.ExpectUniqueSample(
      kPrivateAggregationHostPipeResultHistogram,
      content::GetPrivateAggregationHostPipeReportSuccessValue(), 2);
}

IN_PROC_BROWSER_TEST_F(
    PrivacySandboxSettingsAttestPrivateAggregationInProtectedAudienceBrowserTest,
    SameOrigin_NotEnrolled_Failure) {
  SetAttestations(
      {std::make_pair("a.test", AttestedApiStatus::kProtectedAudience)});

  content::RenderFrameHost* fenced_frame_rfh =
      LoadPageThenLoadAndNavigateFencedFrameViaAdAuctionWithPrivateAggregation(
          /*primary_main_frame_hostname=*/"a.test",
          /*fenced_frame_hostname=*/"a.test");
  ASSERT_NE(fenced_frame_rfh, nullptr);

  WaitForHistogram(kPrivateAggregationHostPipeResultHistogram, 2);
  histogram_tester_.ExpectUniqueSample(
      kPrivateAggregationHostPipeResultHistogram,
      content::GetPrivateAggregationHostPipeApiDisabledValue(), 2);
}

// Verifies that joining interest groups and running auctions in the Protected
// Audience API are subject to attestation checks.
//
// navigtor.joinAdInterestGroup() doesn't have a separate attestation from
// navigator.runAdAuction() -- they both check the same kProtectedAudience
// attestation.
IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsAttestProtectedAudienceBrowserTest,
                       Join_RunAdAuction_Enrollment) {

  struct TestCase {
    AttestedApiStatus join_origin_attestation;
    AttestedApiStatus run_origin_attestation;
    bool expect_auction_succeeds;
  } kTestCases[] = {
      {/*join_origin_attestation=*/AttestedApiStatus::kProtectedAudience,
       /*run_origin_attestation=*/AttestedApiStatus::kProtectedAudience,
       /*expect_auction_succeeds=*/true},
      {/*join_origin_attestation=*/AttestedApiStatus::kSharedStorage,
       AttestedApiStatus::kProtectedAudience,
       /*expect_auction_succeeds=*/false},
      {/*join_origin_attestation=*/AttestedApiStatus::kProtectedAudience,
       /*run_origin_attestation=*/AttestedApiStatus::kSharedStorage,
       /*expect_auction_succeeds=*/false},
      {/*join_origin_attestation=*/AttestedApiStatus::kSharedStorage,
       /*run_origin_attestation=*/AttestedApiStatus::kSharedStorage,
       /*expect_auction_succeeds=*/false},
  };
  for (const auto test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "Join origin attestation "
                 << static_cast<int>(test_case.join_origin_attestation)
                 << " run origin attestation "
                 << static_cast<int>(test_case.run_origin_attestation));
    SetAttestations(
        {std::make_pair("a.test", test_case.join_origin_attestation),
         std::make_pair("b.test", test_case.run_origin_attestation)});

    const GURL join_page = https_server_.GetURL("a.test", "/echo");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), join_page));

    EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                       content::JsReplace(R"(
      (async() => {
        const FLEDGE_BIDDING_URL = "/interest_group/bidding_logic.js";

        const page_origin = new URL($1).origin;
        const bidding_url = new URL(FLEDGE_BIDDING_URL, page_origin);
        const interest_group = {
          name: 'testAd1',
          owner: page_origin,
          biddingLogicUrl: bidding_url,
          ads: [{renderURL: $1, bid: 1}],
        };

        // Pick an arbitrarily high duration to guarantee that we never leave
        // the ad interest group while the test runs.
        await navigator.joinAdInterestGroup(
            interest_group, /*durationSeconds=*/3000000);
      })())",
                                          join_page)));

    const GURL auction_page = https_server_.GetURL("b.test", "/echo");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), auction_page));

    content::EvalJsResult result =
        EvalJs(web_contents()->GetPrimaryMainFrame(),
               content::JsReplace(R"(
      (async() => {
          const FLEDGE_DECISION_URL = "/interest_group/decision_logic.js";

          const page_origin = new URL($1).origin;
          const join_origin = new URL($2).origin;
          const auction_config = {
            seller: page_origin,
            interestGroupBuyers: [join_origin],
            decisionLogicURL: new URL(FLEDGE_DECISION_URL, page_origin),
          };

          return await navigator.runAdAuction(auction_config);
      })())",
                                  auction_page, join_page));
    if (test_case.expect_auction_succeeds) {
      EXPECT_NE(nullptr, result);
    } else {
      EXPECT_EQ(nullptr, result);
    }
  }
}

IN_PROC_BROWSER_TEST_F(
    PrivacySandboxSettingsAttestPrivateAggregationInProtectedAudienceBrowserTest,
    CrossOrigin_Enrolled_Success) {
  SetAttestations(
      {std::make_pair(
           "a.test",
           AttestedApiStatus::kProtectedAudienceAndPrivateAggregation),
       std::make_pair(
           "b.test",
           AttestedApiStatus::kProtectedAudienceAndPrivateAggregation)});

  content::RenderFrameHost* fenced_frame_rfh =
      LoadPageThenLoadAndNavigateFencedFrameViaAdAuctionWithPrivateAggregation(
          /*primary_main_frame_hostname=*/"a.test",
          /*fenced_frame_hostname=*/"b.test");
  ASSERT_NE(fenced_frame_rfh, nullptr);

  WaitForHistogram(kPrivateAggregationHostPipeResultHistogram, 2);
  histogram_tester_.ExpectUniqueSample(
      kPrivateAggregationHostPipeResultHistogram,
      content::GetPrivateAggregationHostPipeReportSuccessValue(), 2);
}

IN_PROC_BROWSER_TEST_F(
    PrivacySandboxSettingsAttestPrivateAggregationInProtectedAudienceBrowserTest,
    CrossOrigin_NotEnrolled_Failure) {
  SetAttestations(
      {std::make_pair(
           "a.test",
           AttestedApiStatus::kProtectedAudienceAndPrivateAggregation),
       std::make_pair("b.test", AttestedApiStatus::kProtectedAudience)});

  content::RenderFrameHost* fenced_frame_rfh =
      LoadPageThenLoadAndNavigateFencedFrameViaAdAuctionWithPrivateAggregation(
          /*primary_main_frame_hostname=*/"a.test",
          /*fenced_frame_hostname=*/"b.test");
  ASSERT_NE(fenced_frame_rfh, nullptr);

  WaitForHistogram(kPrivateAggregationHostPipeResultHistogram, 2);
  histogram_tester_.ExpectUniqueSample(
      kPrivateAggregationHostPipeResultHistogram,
      content::GetPrivateAggregationHostPipeApiDisabledValue(), 2);
}
