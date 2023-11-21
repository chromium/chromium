// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>

#include "base/files/file_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/net/storage_test_utils.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/browser/tpcd/experiment/tpcd_pref_names.h"
#include "chrome/browser/tpcd/experiment/tpcd_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/tpcd/metadata/parser.h"
#include "components/tpcd/metadata/parser_test_helper.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace {

constexpr char kHostA[] = "a.test";
constexpr char kHostB[] = "b.test";
constexpr char kHostC[] = "c.test";

const char kThirdPartyCookieAccessBlockedHistogram[] =
    "PageLoad.Clients.TPCD.ThirdPartyCookieAccessBlockedByExperiment2";

const char kThirdPartyCookieAllowMechanismHistogram[] =
    "PageLoad.Clients.TPCD.CookieAccess.ThirdPartyCookieAllowMechanism";

const char kThirdPartyCookieIsAdOrNonAdHistogram[] =
    "PageLoad.Clients.TPCD.TPCAccess.BlockedByExperiment.IsAdOrNonAd2";

const char kThirdPartyCookieAdBlockedByExperimentHistogram[] =
    "PageLoad.Clients.TPCD.AdTPCAccess.BlockedByExperiment2";

struct Allow3PCMechanismBrowserTestCase {
  bool explicit_setting_allow_3p_cookie = false;
  bool global_setting_allow_3p_cookie = false;
  bool tpcd_metadata_allow_3p_cookie = false;
  bool tracking_protection_onboard = false;
};

// Skip complex test cases setting for TPCD_SUPPORT and TPCD_HEURISTICS since
// they have similar logics to TPCD_METADATA.
const Allow3PCMechanismBrowserTestCase kAllowMechanismTestCases[] = {
    {
        .explicit_setting_allow_3p_cookie = true,
    },
    {
        .global_setting_allow_3p_cookie = true,
    },
    {
        .tpcd_metadata_allow_3p_cookie = true,
    },
    {
        .explicit_setting_allow_3p_cookie = true,
        .tracking_protection_onboard = true,
    },
    {
        .global_setting_allow_3p_cookie = true,
        .tracking_protection_onboard = true,
    },
    {
        .tpcd_metadata_allow_3p_cookie = true,
        .tracking_protection_onboard = true,
    },
    {
        // test cases for all value as default.
    },
};

}  // namespace

class ThirdPartyCookieDeprecationObserverBaseBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  ThirdPartyCookieDeprecationObserverBaseBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ThirdPartyCookieDeprecationObserverBaseBrowserTest(
      const ThirdPartyCookieDeprecationObserverBaseBrowserTest&) = delete;
  ThirdPartyCookieDeprecationObserverBaseBrowserTest& operator=(
      const ThirdPartyCookieDeprecationObserverBaseBrowserTest&) = delete;

  ~ThirdPartyCookieDeprecationObserverBaseBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->AddDefaultHandlers(GetChromeTestDataDir());
    https_server()->ServeFilesFromSourceDirectory("components/test/data");
    ASSERT_TRUE(https_server()->Start());
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("isad=1"),
         subresource_filter::testing::CreateSuffixRule("ad_script.js")});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for 127.0.0.1 or localhost, so this
    // is needed to load pages from other hosts (b.test, c.test) without an
    // error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  GURL GetURL(const std::string& host) {
    return https_server()->GetURL(host, "/");
  }

  void NavigateToUntrackedUrl() {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  }

  void NavigateToPageWithFrame(const std::string& host) {
    GURL main_url(https_server()->GetURL(host, "/iframe.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  }

  void NavigateToPageWithAdFrameFactory(const std::string& host) {
    GURL main_url(
        https_server()->GetURL(host, "/ad_tagging/frame_factory.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  }

  void NavigateFrameTo(const std::string& host, const std::string& path) {
    GURL page = https_server()->GetURL(host, path);
    NavigateFrameToUrl(page);
  }

  void NavigateFrameToUrl(const GURL& url) {
    EXPECT_TRUE(NavigateIframeToURL(web_contents(), "test", url));
  }

  void Wait() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        tpcd::experiment::kDecisionDelayTime.Get());
    run_loop.Run();
  }

  void SetUpTrackingProtectionOnboard() {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kTrackingProtectionOnboardingStatus,
        static_cast<int>(privacy_sandbox::TrackingProtectionOnboarding::
                             OnboardingStatus::kOnboarded));
    EXPECT_EQ(onboarding_service()->GetOnboardingStatus(),
              privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
                  kOnboarded);
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  privacy_sandbox::TrackingProtectionOnboarding* onboarding_service() {
    return TrackingProtectionOnboardingFactory::GetForProfile(
        browser()->profile());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  // This is needed because third party cookies must be marked SameSite=None and
  // Secure, so they must be accessed over HTTPS.
  net::EmbeddedTestServer https_server_;
};

class ThirdPartyCookieDeprecationObserverBrowserTest
    : public ThirdPartyCookieDeprecationObserverBaseBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  ThirdPartyCookieDeprecationObserverBrowserTest()
      : is_experiment_cookies_disabled_(std::get<0>(GetParam())),
        is_client_eligible_(std::get<1>(GetParam())) {}

  ThirdPartyCookieDeprecationObserverBrowserTest(
      const ThirdPartyCookieDeprecationObserverBrowserTest&) = delete;
  ThirdPartyCookieDeprecationObserverBrowserTest& operator=(
      const ThirdPartyCookieDeprecationObserverBrowserTest&) = delete;

  ~ThirdPartyCookieDeprecationObserverBrowserTest() override = default;

  void SetUp() override {
    SetUpThirdPartyCookieExperiment();
    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpThirdPartyCookieExperiment() {
    // Experiment feature param requests 3PCs blocked.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kCookieDeprecationFacilitatedTesting,
          {{tpcd::experiment::kDisable3PCookiesName,
            is_experiment_cookies_disabled_ ? "true" : "false"}}},
         {subresource_filter::kTPCDAdHeuristicSubframeRequestTagging, {}}},
        {content_settings::features::kTrackingProtection3pcd});
  }

  void SetUpThirdPartyCookieExperimentWithClientState() {
    Wait();
    g_browser_process->local_state()->SetInteger(
        tpcd::experiment::prefs::kTPCDExperimentClientState,
        static_cast<int>(
            is_client_eligible_
                ? tpcd::experiment::utils::ExperimentState::kEligible
                : tpcd::experiment::utils::ExperimentState::kIneligible));
  }

  void SetUpTrackingProtectionOnboardWith3PCAllow(
      const std::vector<GURL>& third_party_urls) {
    SetUpTrackingProtectionOnboard();
    // If tracking protection is onboard, observer's OnCookieAccess won't be
    // triggered without any re-enable mechanisms. For testing purpose, we
    // explicitly set allowing third party cookie access for test URLs.
    for (auto third_party_url : third_party_urls) {
      HostContentSettingsMapFactory::GetForProfile(browser()->profile())
          ->SetContentSettingDefaultScope(third_party_url, GURL(),
                                          ContentSettingsType::COOKIES,
                                          CONTENT_SETTING_ALLOW);
    }
  }

  void FetchCookies(const std::string& host, const std::string& path) {
    // Fetch a subresrouce.
    std::string fetch_subresource_script = R"(
        const imgElement = document.createElement('img');
        imgElement.src = $1;
        document.body.appendChild(imgElement);
  )";

    content::CookieChangeObserver observer(web_contents());
    std::ignore =
        ExecJs(web_contents(),
               content::JsReplace(fetch_subresource_script,
                                  https_server()->GetURL(host, path).spec()));
    observer.Wait();
  }

  bool IsRecordThirdPartyCookiesExperimentMetrics() {
    return is_experiment_cookies_disabled_ && is_client_eligible_;
  }

 private:
  bool is_experiment_cookies_disabled_;
  bool is_client_eligible_;
};

INSTANTIATE_TEST_SUITE_P(,
                         ThirdPartyCookieDeprecationObserverBrowserTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverBrowserTest,
                       FirstPartyCookiesReadAndWrite) {
  SetUpThirdPartyCookieExperimentWithClientState();

  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame(kHostA);  // Should read a same-origin cookie.
  NavigateFrameTo(kHostA, "/set-cookie?same-origin");  // same-origin write
  NavigateToUntrackedUrl();

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieRead, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieWrite, 0);

  // Expect no third party metrics records for first party cases.
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieAccessBlockByExperiment, 0);
  histogram_tester.ExpectBucketCount(kThirdPartyCookieAccessBlockedHistogram,
                                     false, 0);
  histogram_tester.ExpectBucketCount(kThirdPartyCookieAccessBlockedHistogram,
                                     true, 0);
  histogram_tester.ExpectTotalCount(kThirdPartyCookieIsAdOrNonAdHistogram, 0);

  // Should not record allow mechanism metrics on first party.
  histogram_tester.ExpectUniqueSample(kThirdPartyCookieAllowMechanismHistogram,
                                      /*kAllowByExplicitSetting*/ 1, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::
          kThirdPartyCookieDeprecation_AllowByExplicitSetting,
      0);
}

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverBrowserTest,
                       ThirdPartyCookiesReadAndWrite) {
  SetUpThirdPartyCookieExperimentWithClientState();
  SetUpTrackingProtectionOnboardWith3PCAllow({GetURL(kHostB)});

  content::CookieChangeObserver observer(web_contents(), 2);
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame(kHostA);  // Same origin cookie read.
  // 3p cookie write
  NavigateFrameTo(kHostB, "/set-cookie?thirdparty=1;SameSite=None;Secure");
  // 3p cookie read
  NavigateFrameTo(kHostB, "/");
  observer.Wait();
  NavigateToUntrackedUrl();

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieRead, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieWrite, 1);

  if (IsRecordThirdPartyCookiesExperimentMetrics()) {
    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kThirdPartyCookieAccessBlockByExperiment, 1);
    histogram_tester.ExpectUniqueSample(kThirdPartyCookieAccessBlockedHistogram,
                                        true, 2);
  } else {
    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kThirdPartyCookieAccessBlockByExperiment, 0);
    histogram_tester.ExpectUniqueSample(kThirdPartyCookieAccessBlockedHistogram,
                                        false, 2);
  }
}

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverBrowserTest,
                       FirstPartyJavaScriptCookieReadAndWrite) {
  SetUpThirdPartyCookieExperimentWithClientState();

  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame(kHostA);  // Same origin cookie read.
  NavigateFrameTo(kHostB, "/empty.html");
  content::RenderFrameHost* frame =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);

  // Write a first-party cookie.
  EXPECT_TRUE(content::ExecJs(frame, "document.cookie = 'foo=bar';"));

  // Read a first-party cookie.
  EXPECT_TRUE(content::ExecJs(frame, "let x = document.cookie;"));
  NavigateToUntrackedUrl();

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieRead, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieWrite, 0);

  // Expect no third party metrics records for first party cases.
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieAccessBlockByExperiment, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieAdAccessBlockByExperiment, 0);
  histogram_tester.ExpectBucketCount(kThirdPartyCookieAccessBlockedHistogram,
                                     false, 0);
  histogram_tester.ExpectBucketCount(kThirdPartyCookieAccessBlockedHistogram,
                                     true, 0);
  // Should not record allow mechanism metrics on first party.
  histogram_tester.ExpectUniqueSample(kThirdPartyCookieAllowMechanismHistogram,
                                      /*kAllowByExplicitSetting*/ 1, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::
          kThirdPartyCookieDeprecation_AllowByExplicitSetting,
      0);
}

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverBrowserTest,
                       ThirdPartyJavaScriptCookieReadAndWrite) {
  SetUpThirdPartyCookieExperimentWithClientState();
  SetUpTrackingProtectionOnboardWith3PCAllow({GetURL(kHostB)});

  content::CookieChangeObserver observer(web_contents(), 2);
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame(kHostA);  // Same origin cookie read.
  NavigateFrameTo(kHostB, "/empty.html");
  content::RenderFrameHost* frame =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);

  // Write a third-party cookie.
  EXPECT_TRUE(content::ExecJs(
      frame, "document.cookie = 'foo=bar;SameSite=None;Secure';"));

  // Read a third-party cookie.
  EXPECT_TRUE(content::ExecJs(frame, "let x = document.cookie;"));
  observer.Wait();
  NavigateToUntrackedUrl();

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieRead, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieWrite, 1);

  if (IsRecordThirdPartyCookiesExperimentMetrics()) {
    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kThirdPartyCookieAccessBlockByExperiment, 1);
    histogram_tester.ExpectUniqueSample(kThirdPartyCookieAccessBlockedHistogram,
                                        true, 2);
    histogram_tester.ExpectUniqueSample(kThirdPartyCookieIsAdOrNonAdHistogram,
                                        false, 1);
  } else {
    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kThirdPartyCookieAccessBlockByExperiment, 0);
    histogram_tester.ExpectUniqueSample(kThirdPartyCookieAccessBlockedHistogram,
                                        false, 2);
  }
}

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverBrowserTest,
                       ThirdPartyAdCookieRead) {
  SetUpThirdPartyCookieExperimentWithClientState();
  SetUpTrackingProtectionOnboardWith3PCAllow({GetURL(kHostB)});

  content::CookieChangeObserver observer(web_contents());
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame(kHostA);  // Same origin cookie read.
  // 3p cookie write
  FetchCookies(kHostB, "/set-cookie?thirdparty=1;SameSite=None;Secure&isad=1");
  // 3p cookie read
  FetchCookies(kHostB, "/empty.html?isad=1");

  NavigateToUntrackedUrl();

  if (IsRecordThirdPartyCookiesExperimentMetrics()) {
    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kThirdPartyCookieAdAccessBlockByExperiment,
        1);
    histogram_tester.ExpectUniqueSample(kThirdPartyCookieIsAdOrNonAdHistogram,
                                        true, 1);
    histogram_tester.ExpectUniqueSample(
        kThirdPartyCookieAdBlockedByExperimentHistogram, true, 1);
  } else {
    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kThirdPartyCookieAdAccessBlockByExperiment,
        0);
    histogram_tester.ExpectUniqueSample(
        kThirdPartyCookieAdBlockedByExperimentHistogram, false, 1);
    histogram_tester.ExpectTotalCount(kThirdPartyCookieIsAdOrNonAdHistogram, 0);
  }
}

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverBrowserTest,
                       ThirdPartyNonAdCookieRead) {
  SetUpThirdPartyCookieExperimentWithClientState();
  SetUpTrackingProtectionOnboardWith3PCAllow({GetURL(kHostB)});

  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame(kHostA);  // Same origin cookie read.
  // 3p cookie write
  FetchCookies(kHostB, "/set-cookie?thirdparty=1;SameSite=None;Secure&isad=0");
  // 3p cookie read
  FetchCookies(kHostB, "/empty.html?isad=0");

  NavigateToUntrackedUrl();

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieAdAccessBlockByExperiment, 0);
  histogram_tester.ExpectTotalCount(
      kThirdPartyCookieAdBlockedByExperimentHistogram, 0);

  if (IsRecordThirdPartyCookiesExperimentMetrics()) {
    histogram_tester.ExpectUniqueSample(kThirdPartyCookieIsAdOrNonAdHistogram,
                                        false, 1);
  }
}

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverBrowserTest,
                       FirstPartyAdCookieRead) {
  SetUpThirdPartyCookieExperimentWithClientState();

  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame(kHostA);  // Same origin cookie read.
  // 1p cookie write
  FetchCookies(kHostA, "/set-cookie?thirdparty=1;SameSite=None;Secure&isad=0");
  FetchCookies(kHostA, "/set-cookie?thirdparty=1;SameSite=None;Secure&isad=1");
  // 1p cookie read
  FetchCookies(kHostA, "/empty.html?isad=0");
  FetchCookies(kHostA, "/empty.html?isad=1");

  NavigateToUntrackedUrl();

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieAdAccessBlockByExperiment, 0);
  histogram_tester.ExpectTotalCount(
      kThirdPartyCookieAdBlockedByExperimentHistogram, 0);
  histogram_tester.ExpectTotalCount(kThirdPartyCookieIsAdOrNonAdHistogram, 0);
}

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverBrowserTest,
                       ThirdPartyAdCookieReadSubframe) {
  SetUpThirdPartyCookieExperimentWithClientState();
  SetUpTrackingProtectionOnboardWith3PCAllow({GetURL(kHostB)});

  content::CookieChangeObserver observer(web_contents(), 2);
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame(kHostA);  // Same origin cookie read.
  // 3p cookie write
  NavigateFrameTo(kHostB,
                  "/set-cookie?thirdparty=1;SameSite=None;Secure&isad=1");
  // 3p cookie read
  NavigateFrameTo(kHostB, "/empty.html?isad=1");
  observer.Wait();

  NavigateToUntrackedUrl();
  histogram_tester.ExpectUniqueSample(
      kThirdPartyCookieAdBlockedByExperimentHistogram,
      IsRecordThirdPartyCookiesExperimentMetrics(), 1);
  if (IsRecordThirdPartyCookiesExperimentMetrics()) {
    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kThirdPartyCookieAdAccessBlockByExperiment,
        1);
    histogram_tester.ExpectUniqueSample(kThirdPartyCookieIsAdOrNonAdHistogram,
                                        true, 1);
  }
}

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverBrowserTest,
                       ThirdPartyAdCookieReadScriptTaggedSubframe) {
  SetUpThirdPartyCookieExperimentWithClientState();
  SetUpTrackingProtectionOnboardWith3PCAllow({GetURL(kHostB)});

  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame(kHostA);  // Same origin cookie read.
  // 3p cookie write
  NavigateFrameTo(kHostB,
                  "/set-cookie?thirdparty=1;SameSite=None;Secure&isad=1");

  // Create a frame tagged by ad script heuristic.
  NavigateToPageWithAdFrameFactory(kHostA);  // Same origin cookie read.

  content::CookieChangeObserver observer(web_contents(), 1);
  GURL ad_url = https_server()->GetURL(kHostB, "/empty.html");
  EXPECT_TRUE(ExecJs(web_contents(),
                     "createAdFrame('" + ad_url.spec() + "', '');",
                     content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  observer.Wait();

  NavigateToUntrackedUrl();
  histogram_tester.ExpectUniqueSample(
      kThirdPartyCookieAdBlockedByExperimentHistogram,
      IsRecordThirdPartyCookiesExperimentMetrics(), 1);
  if (IsRecordThirdPartyCookiesExperimentMetrics()) {
    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kThirdPartyCookieAdAccessBlockByExperiment,
        1);
    histogram_tester.ExpectUniqueSample(kThirdPartyCookieIsAdOrNonAdHistogram,
                                        true, 1);
  }
}

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverBrowserTest,
                       ThirdPartyAdCookieReadOnRedirect) {
  SetUpThirdPartyCookieExperimentWithClientState();
  SetUpTrackingProtectionOnboardWith3PCAllow({GetURL(kHostB), GetURL(kHostC)});

  content::CookieChangeObserver observer(web_contents());
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame(kHostA);  // Same origin cookie read.
  // 3p cookie write
  FetchCookies(kHostB, "/set-cookie?thirdparty=1;SameSite=None;Secure&isad=1");
  // 3p cookie read
  FetchCookies(kHostC,
               "/server-redirect?" +
                   https_server()->GetURL(kHostB, "/empty.html?isad=1").spec());

  NavigateToUntrackedUrl();

  if (IsRecordThirdPartyCookiesExperimentMetrics()) {
    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kThirdPartyCookieAdAccessBlockByExperiment,
        1);
    histogram_tester.ExpectUniqueSample(kThirdPartyCookieIsAdOrNonAdHistogram,
                                        true, 1);
    histogram_tester.ExpectUniqueSample(
        kThirdPartyCookieAdBlockedByExperimentHistogram, true, 1);
  } else {
    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kThirdPartyCookieAdAccessBlockByExperiment,
        0);
    histogram_tester.ExpectUniqueSample(
        kThirdPartyCookieAdBlockedByExperimentHistogram, false, 1);
    histogram_tester.ExpectTotalCount(kThirdPartyCookieIsAdOrNonAdHistogram, 0);
  }
}

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverBrowserTest,
                       ThirdPartyAdCookieReadOnNonAdRedirect) {
  auto https_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/do-redirect");
  https_server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server->AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(https_server->Start());

  SetUpThirdPartyCookieExperimentWithClientState();
  SetUpTrackingProtectionOnboardWith3PCAllow(
      {GetURL(kHostB), https_server->GetURL(kHostB, "/"),
       https_server->GetURL(kHostC, "/")});

  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame(kHostA);  // Same origin cookie read.
  // 3p cookie write
  FetchCookies(kHostB, "/set-cookie?thirdparty=1;SameSite=None;Secure&isad=1");

  // Fetch the controllable response manually and set the location header,
  // /set-redirect cannot be used as the ad suffix will still be tagged.
  std::string fetch_subresource_script = R"(
        const imgElement = document.createElement('img');
        imgElement.src = $1;
        document.body.appendChild(imgElement);
  )";

  content::CookieChangeObserver observer(web_contents());
  std::ignore = ExecJs(
      web_contents(),
      content::JsReplace(fetch_subresource_script,
                         https_server->GetURL(kHostC, "/do-redirect").spec()));

  register_response->WaitForRequest();
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader(
      "Location", https_server->GetURL(kHostB, "/empty.html?isad=1").spec());
  register_response->Send(http_response->ToResponseString());
  register_response->Done();

  observer.Wait();

  NavigateToUntrackedUrl();

  // For redirect chains, cookie access is tagged as an ad only depending on the
  // ad status of the initial request in the redirect chain, so this cookie
  // access will count as a non-ad cookie access.
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieAdAccessBlockByExperiment, 0);
  histogram_tester.ExpectTotalCount(
      kThirdPartyCookieAdBlockedByExperimentHistogram, 0);
  if (IsRecordThirdPartyCookiesExperimentMetrics()) {
    histogram_tester.ExpectUniqueSample(kThirdPartyCookieIsAdOrNonAdHistogram,
                                        false, 1);
  }
}

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverBrowserTest,
                       ThirdPartyAdJavaScriptCookieRead) {
  SetUpThirdPartyCookieExperimentWithClientState();
  SetUpTrackingProtectionOnboardWith3PCAllow({GetURL(kHostB)});

  content::CookieChangeObserver observer(web_contents(),
                                         /*num_expected_calls=*/2);
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/empty.html?isad=1");
  content::RenderFrameHost* frame =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);

  // Write a third-party cookie.
  EXPECT_TRUE(content::ExecJs(
      frame, "document.cookie = 'foo=bar;SameSite=None;Secure';"));

  // Read a third-party cookie.
  EXPECT_TRUE(content::ExecJs(frame, "let x = document.cookie;"));
  observer.Wait();
  NavigateToUntrackedUrl();

  if (IsRecordThirdPartyCookiesExperimentMetrics()) {
    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kThirdPartyCookieAdAccessBlockByExperiment,
        1);
    histogram_tester.ExpectUniqueSample(kThirdPartyCookieIsAdOrNonAdHistogram,
                                        true, 1);
    histogram_tester.ExpectUniqueSample(
        kThirdPartyCookieAdBlockedByExperimentHistogram, true, 1);
  } else {
    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kThirdPartyCookieAdAccessBlockByExperiment,
        0);
    histogram_tester.ExpectUniqueSample(
        kThirdPartyCookieAdBlockedByExperimentHistogram, false, 1);
    histogram_tester.ExpectTotalCount(kThirdPartyCookieIsAdOrNonAdHistogram, 0);
  }
}

class ThirdPartyCookieDeprecationObserverMechanismBrowserTest
    : public ThirdPartyCookieDeprecationObserverBaseBrowserTest,
      public testing::WithParamInterface<Allow3PCMechanismBrowserTestCase> {
 public:
  ThirdPartyCookieDeprecationObserverMechanismBrowserTest()
      : test_case_(GetParam()) {
    CHECK(fake_install_dir_.CreateUniqueTempDir());
    CHECK(fake_install_dir_.IsValid());
  }

  ThirdPartyCookieDeprecationObserverMechanismBrowserTest(
      const ThirdPartyCookieDeprecationObserverMechanismBrowserTest&) = delete;
  ThirdPartyCookieDeprecationObserverMechanismBrowserTest& operator=(
      const ThirdPartyCookieDeprecationObserverMechanismBrowserTest&) = delete;

  ~ThirdPartyCookieDeprecationObserverMechanismBrowserTest() override = default;

  void SetUp() override {
    // Experiment feature param requests 3PCs blocked.
    if (test_case_.tpcd_metadata_allow_3p_cookie) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {{features::kCookieDeprecationFacilitatedTesting,
            {{tpcd::experiment::kDisable3PCookiesName, "true"}}},
           {content_settings::features::kTrackingProtection3pcd, {}},
           {net::features::kTpcdMetadataGrants, {}}},
          {});
    } else {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {{features::kCookieDeprecationFacilitatedTesting,
            {{tpcd::experiment::kDisable3PCookiesName, "true"}}}},
          {content_settings::features::kTrackingProtection3pcd});
    }
    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpThirdPartyCookieAllowMechanism(const GURL& first_party_url,
                                           const GURL& third_party_url) {
    Wait();
    g_browser_process->local_state()->SetInteger(
        tpcd::experiment::prefs::kTPCDExperimentClientState,
        static_cast<int>(tpcd::experiment::utils::ExperimentState::kEligible));

    // Set up tracking protection onboard status.
    if (test_case_.tracking_protection_onboard) {
      SetUpTrackingProtectionOnboard();
    }

    if (test_case_.explicit_setting_allow_3p_cookie) {
      HostContentSettingsMapFactory::GetForProfile(browser()->profile())
          ->SetContentSettingDefaultScope(third_party_url, GURL(),
                                          ContentSettingsType::COOKIES,
                                          CONTENT_SETTING_ALLOW);
    } else if (test_case_.global_setting_allow_3p_cookie) {
      browser()->profile()->GetPrefs()->SetInteger(
          prefs::kCookieControlsMode,
          static_cast<int>(content_settings::CookieControlsMode::kOff));
    } else if (test_case_.tpcd_metadata_allow_3p_cookie) {
      base::ScopedAllowBlockingForTesting allow_blocking;

      //  Simulate tracking protection settings.
      browser()->profile()->GetPrefs()->SetBoolean(
          prefs::kBlockAll3pcToggleEnabled, false);

      // Set up tpcd metadata, make sure both the primary pattern and secondary
      // pattern match.
      const std::string primary_pattern_spec =
          ContentSettingsPattern::FromURL(third_party_url).ToString();
      const std::string secondary_pattern_spec =
          ContentSettingsPattern::FromURL(first_party_url).ToString();
      tpcd::metadata::Metadata metadata =
          tpcd::metadata::MakeMetadataProtoFromVectorOfPair(
              {{primary_pattern_spec, secondary_pattern_spec}});
      EXPECT_EQ(metadata.metadata_entries_size(), 1);
      MockComponentInstallation(metadata);
      EXPECT_EQ(CookieSettingsFactory::GetForProfile(browser()->profile())
                    ->GetCookieSetting(third_party_url, first_party_url,
                                       net::CookieSettingOverrides()),
                ContentSetting::CONTENT_SETTING_ALLOW);
    }
  }

  void MockComponentInstallation(tpcd::metadata::Metadata metadata) {
    base::FilePath path =
        fake_install_dir_.GetPath().Append(FILE_PATH_LITERAL("metadata.pb"));
    CHECK(base::WriteFile(path, metadata.SerializeAsString()));

    CHECK(base::PathExists(path));
    std::string raw_metadata;
    CHECK(base::ReadFileToString(path, &raw_metadata));

    tpcd::metadata::Parser::GetInstance()->ParseMetadata(raw_metadata);
  }

  void VerifyThirdPartyCookieAllowMechanism(
      const base::HistogramTester& histogram_tester) {
    // Notes: All the blink feature usage metric only record when tracking
    // protection is onboard.
    if (test_case_.explicit_setting_allow_3p_cookie) {
      histogram_tester.ExpectUniqueSample(
          kThirdPartyCookieAllowMechanismHistogram,
          /*kAllowByExplicitSetting*/ 1, 2);
      histogram_tester.ExpectBucketCount(
          "Blink.UseCounter.Features",
          blink::mojom::WebFeature::
              kThirdPartyCookieDeprecation_AllowByExplicitSetting,
          test_case_.tracking_protection_onboard ? 1 : 0);
    } else if (test_case_.global_setting_allow_3p_cookie) {
      // If tracking protection is onboard, the global setting is not allow
      // third party to access cookie. Also, it blocks re-enable third-party
      // cookies access through pref key prefs::kCookieControlsMode used by
      // enterprise policy BlockThirdPartyCookies. In this case, page load
      // metrics observers' OnCookie(Read|Change) won't be triggered.
      histogram_tester.ExpectUniqueSample(
          kThirdPartyCookieAllowMechanismHistogram,
          /*kAllowByGlobalSetting*/ 2,
          test_case_.tracking_protection_onboard ? 0 : 2);
      histogram_tester.ExpectBucketCount(
          "Blink.UseCounter.Features",
          blink::mojom::WebFeature::
              kThirdPartyCookieDeprecation_AllowByGlobalSetting,
          0);
    } else if (test_case_.tpcd_metadata_allow_3p_cookie) {
      histogram_tester.ExpectUniqueSample(
          kThirdPartyCookieAllowMechanismHistogram,
          /*kAllowBy3PCDMetadata*/ 3, 2);
      histogram_tester.ExpectBucketCount(
          "Blink.UseCounter.Features",
          blink::mojom::WebFeature::
              kThirdPartyCookieDeprecation_AllowBy3PCDMetadata,
          test_case_.tracking_protection_onboard ? 1 : 0);
    } else {
      histogram_tester.ExpectUniqueSample(
          kThirdPartyCookieAllowMechanismHistogram,
          /*kAllowByGlobalSetting*/ 2,
          test_case_.tracking_protection_onboard ? 0 : 2);
      // If tracking protection is onboard, the global setting is not allow
      // third party to access cookie.
      histogram_tester.ExpectBucketCount(
          "Blink.UseCounter.Features",
          blink::mojom::WebFeature::
              kThirdPartyCookieDeprecation_AllowByGlobalSetting,
          0);
    }
  }

  int CanTriggerCookieChangeObserver() {
    // If tracking protection is onboard, the CookieChangeObserver is trigger
    // only when some mechanisms re-enable the third party cookie access
    if (test_case_.tracking_protection_onboard) {
      return test_case_.explicit_setting_allow_3p_cookie ||
             test_case_.tpcd_metadata_allow_3p_cookie;
    }
    return true;
  }

 private:
  Allow3PCMechanismBrowserTestCase test_case_;
  base::ScopedTempDir fake_install_dir_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    ThirdPartyCookieDeprecationObserverMechanismBrowserTest,
    testing::ValuesIn(kAllowMechanismTestCases));

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverMechanismBrowserTest,
                       ThirdPartyCookiesReadAndWrite) {
  SetUpThirdPartyCookieAllowMechanism(/*first_party_url=*/GetURL(kHostA),
                                      /*third_party_url=*/GetURL(kHostB));

  content::CookieChangeObserver observer(web_contents(), 2);
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame(kHostA);
  // 3p cookie write
  NavigateFrameTo(kHostB, "/set-cookie?thirdparty=1;SameSite=None;Secure");
  // 3p cookie read
  NavigateFrameTo(kHostB, "/");
  if (CanTriggerCookieChangeObserver()) {
    observer.Wait();
  }
  NavigateToUntrackedUrl();

  VerifyThirdPartyCookieAllowMechanism(histogram_tester);
}

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverMechanismBrowserTest,
                       ThirdPartyJavaScriptCookieReadAndWrite) {
  SetUpThirdPartyCookieAllowMechanism(/*first_party_url=*/GetURL(kHostA),
                                      /*third_party_url=*/GetURL(kHostB));

  content::CookieChangeObserver observer(web_contents(), 2);
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/empty.html");
  content::RenderFrameHost* frame =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);

  // Write a third-party cookie.
  EXPECT_TRUE(content::ExecJs(
      frame, "document.cookie = 'foo=bar;SameSite=None;Secure';"));

  // Read a third-party cookie.
  EXPECT_TRUE(content::ExecJs(frame, "let x = document.cookie;"));
  if (CanTriggerCookieChangeObserver()) {
    observer.Wait();
  }
  NavigateToUntrackedUrl();
  VerifyThirdPartyCookieAllowMechanism(histogram_tester);
}

class ThirdPartyCookieDeprecationObserverSSABrowserTest
    : public ThirdPartyCookieDeprecationObserverBaseBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ThirdPartyCookieDeprecationObserverSSABrowserTest() = default;

  ThirdPartyCookieDeprecationObserverSSABrowserTest(
      const ThirdPartyCookieDeprecationObserverSSABrowserTest&) = delete;
  ThirdPartyCookieDeprecationObserverSSABrowserTest& operator=(
      const ThirdPartyCookieDeprecationObserverSSABrowserTest&) = delete;

  ~ThirdPartyCookieDeprecationObserverSSABrowserTest() override = default;

  static constexpr char kVerifyHasStorageAccessPermission[] =
      "navigator.permissions.query({name: 'storage-access'}).then("
      "  (permission) => permission.name === 'storage-access' && "
      "permission.state === 'granted');";
  static constexpr char kRequestStorageAccess[] =
      "document.requestStorageAccess()";

  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kCookieDeprecationFacilitatedTesting,
          {{tpcd::experiment::kDisable3PCookiesName, "true"}}},
         {content_settings::features::kTrackingProtection3pcd, {}},
         {blink::features::kStorageAccessAPI, {}}},
        {});
    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ThirdPartyCookieDeprecationObserverBaseBrowserTest::SetUpCommandLine(
        command_line);
    command_line->AppendSwitchASCII(
        network::switches::kUseRelatedWebsiteSet,
        base::StrCat({R"({"primary": "https://)", kHostA,
                      R"(", "associatedSites": ["https://)", kHostC, R"("])",
                      R"(, "serviceSites": ["https://)", kHostB, R"("]})"}));
  }

  void SetUpThirdPartyCookieExperiment() {
    Wait();
    g_browser_process->local_state()->SetInteger(
        tpcd::experiment::prefs::kTPCDExperimentClientState,
        static_cast<int>(tpcd::experiment::utils::ExperimentState::kEligible));

    // Set up tracking protection onboard status.
    if (GetParam()) {
      SetUpTrackingProtectionOnboard();
    }
  }

  void SetCrossSiteCookieOnHost(const std::string& host) {
    GURL host_url = GetURL(host);
    std::string cookie = base::StrCat({"cross-site=", host});
    content::SetCookie(browser()->profile(), host_url,
                       base::StrCat({cookie, ";SameSite=None;Secure"}));
    ASSERT_THAT(content::GetCookies(browser()->profile(), host_url),
                testing::HasSubstr(cookie));
  }

  void SetStorageAccessAPIPermission(content::RenderFrameHost* frame) {
    // See comments in RequestStorageAccessForBaseBrowserTest.
    EXPECT_FALSE(content::ExecJs(frame, kRequestStorageAccess,
                                 content::EXECUTE_SCRIPT_NO_USER_GESTURE));
    EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
        web_contents()->GetPrimaryMainFrame(), GetURL(kHostB).spec()));
    EXPECT_TRUE(content::ExecJs(frame, kRequestStorageAccess,
                                content::EXECUTE_SCRIPT_NO_USER_GESTURE));
    EXPECT_TRUE(storage::test::HasStorageAccessForFrame(frame));
    EXPECT_TRUE(content::EvalJs(frame, kVerifyHasStorageAccessPermission)
                    .ExtractBool());
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         ThirdPartyCookieDeprecationObserverSSABrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverSSABrowserTest,
                       ThirdPartyCookiesReadAndWrite) {
  SetUpThirdPartyCookieExperiment();
  SetCrossSiteCookieOnHost(kHostB);

  content::CookieChangeObserver observer(web_contents(), 2);
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/");

  content::RenderFrameHost* frame =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  SetStorageAccessAPIPermission(frame);

  // 3p cookie write
  NavigateFrameTo(kHostB, "/set-cookie?thirdparty=1;SameSite=None;Secure");
  // 3p cookie read
  NavigateFrameTo(kHostB, "/");
  observer.Wait();
  NavigateToUntrackedUrl();

  // TODO(https://crbug.com/1494080) In this case, url_loader can't get correct
  // cookie_setting_overrides value when creating CookieAccessDetails object. It
  // fails to get the correct enabling mechanism of storage access API. Confirm
  // with storage access API owner.
  histogram_tester.ExpectUniqueSample(kThirdPartyCookieAllowMechanismHistogram,
                                      /*kAllowByStorageAccess*/ 6, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::
          kThirdPartyCookieDeprecation_AllowByStorageAccess,
      0);
}

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverSSABrowserTest,
                       ThirdPartyJavaScriptCookieReadAndWrite) {
  SetUpThirdPartyCookieExperiment();
  SetCrossSiteCookieOnHost(kHostB);

  content::CookieChangeObserver observer(web_contents(), 2);
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/empty.html");

  content::RenderFrameHost* frame =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  SetStorageAccessAPIPermission(frame);

  // Write a third-party cookie.
  EXPECT_TRUE(content::ExecJs(
      frame, "document.cookie = 'foo=bar;SameSite=None;Secure';"));
  // Read a third-party cookie.
  EXPECT_TRUE(content::ExecJs(frame, "let x = document.cookie;"));
  observer.Wait();
  NavigateToUntrackedUrl();

  histogram_tester.ExpectUniqueSample(kThirdPartyCookieAllowMechanismHistogram,
                                      /*kAllowByStorageAccess*/ 6, 2);
  // Only record blink usage when tracking protection is onboard.
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::
          kThirdPartyCookieDeprecation_AllowByStorageAccess,
      GetParam() ? 1 : 0);
}
