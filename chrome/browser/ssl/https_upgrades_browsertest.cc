// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "build/build_config.h"
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/api/settings_private/generated_prefs.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/generated_https_first_mode_pref.h"
#include "chrome/browser/ssl/https_first_mode_settings_tracker.h"
#include "chrome/browser/ssl/https_upgrades_interceptor.h"
#include "chrome/browser/ssl/https_upgrades_navigation_throttle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/captive_portal/content/captive_portal_service.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/embedder_support/pref_names.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/security_interstitials/core/https_only_mode_metrics.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/hashing.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/url_constants.h"

using chrome_browser_interstitials::HFMInterstitialType;
using security_interstitials::https_only_mode::Event;
using security_interstitials::https_only_mode::InterstitialReason;
using security_interstitials::https_only_mode::kEventHistogram;
using security_interstitials::https_only_mode::
    kEventHistogramWithEngagementHeuristic;
using security_interstitials::https_only_mode::kInterstitialReasonHistogram;
using security_interstitials::https_only_mode::
    kNavigationRequestSecurityLevelHistogram;
using security_interstitials::https_only_mode::
    kSiteEngagementHeuristicAccumulatedHostCountHistogram;
using security_interstitials::https_only_mode::
    kSiteEngagementHeuristicEnforcementDurationHistogram;
using security_interstitials::https_only_mode::
    kSiteEngagementHeuristicHostCountHistogram;
using security_interstitials::https_only_mode::
    kSiteEngagementHeuristicStateHistogram;
using security_interstitials::https_only_mode::NavigationRequestSecurityLevel;
using security_interstitials::https_only_mode::SiteEngagementHeuristicState;

// Many of the following tests have only minor variations for HTTPS-First Mode
// vs. HTTPS-Upgrades. These get parameterized so the tests run under both
// versions on their own as well as when both HTTPS Upgrades and HTTPS-First
// Mode are enabled (to test any interactions between the two upgrade modes).
// HTTPS-Upgrades is now enabled by default, so all of these variations build
// on top of that baseline.
//
// Quick summary of all features tested here:
// * HTTPS-Upgrades:
//     Automatically upgrades main frame navigations to HTTPS. Silently falls
//     back to HTTP on failure.
// * HTTPS First Mode:
//     Automatically upgrades main frame navigations to HTTPS. Shows an
//     interstitial on failure.
// * HTTPS First Mode With Site Engagement:
//     Automatically enables HTTPS First Mode for sites that are visited mainly
//     over HTTPS.
// * HTTPS First Mode for Typically Secure Users
//     Automatically enables HTTPS First Mode for users that mainly visit HTTPS
//     sites.
// * HTTPS First Mode in Incognito:
//     Automatically enables HTTPS First Mode in Incognito windows.
// * HTTPS First Balanced Mode:
//     Enables HTTPS First Mode like full HFM, but exempt navigations that are
//     likely to fail.
//
enum class HttpsUpgradesTestType {
  // Enables the HFM pref.
  kHttpsFirstModeOnly,

  // Enables HFM with Site Engagement heuristic.
  kHttpsFirstModeWithSiteEngagement,

  // Enables HFM for Typically Secure Users.
  kHttpsFirstModeForTypicallySecureUsers,

  // Enables HFM with Site Engagement and HFM for Typically Secure Users (both
  // automatically enable HFM).
  kAllAutoHFM,

  // Enables HFM in Incognito mode. Runs testcases inside an Incognito
  // window.
  kHttpsFirstModeIncognito,

  // Enables HFM in balanced mode.
  kHttpsFirstBalancedMode,

  // Enables HFM pref, HFM with Site Engagement heuristic, HFM for typically
  // secure users, HFM in incognito, and balanced HFM feature flags.
  kAll,

  // Disables HFM pref, HFM with Site Engagement heuristic, the HFM for
  // typically secure users feature, and the HFM in Incognito feature.
  kNone,
};

// Stores the number of times the HTTPS-First Mode interstitial is shown for the
// given reason.
struct ExpectedInterstitialReasons {
  // The number of times the interstitial was shown because the HFM pref was
  // enabled.
  size_t pref = 0;
  // The number of times the interstitial was shown because of the Typically
  // Secure User heuristic.
  size_t typically_secure_user = 0;
  // The number of times the interstitial was shown because of being in balanced
  // mode.
  size_t balanced = 0;
};

// A very low site engagement score.
constexpr int kLowSiteEngagementScore = 2;
// A very high site engagement score.
constexpr int kHighSiteEnagementScore = 95;

// Tests for HTTPS-Upgrades and the v2 implementation of HTTPS-First Mode.
class HttpsUpgradesBrowserTest
    : public testing::WithParamInterface<HttpsUpgradesTestType>,
      public InProcessBrowserTest {
 public:
  HttpsUpgradesBrowserTest() = default;
  ~HttpsUpgradesBrowserTest() override = default;

  void SetUp() override {
    // HFM is controlled by a pref (configured in SetUpOnMainThread).
    switch (https_upgrades_test_type()) {
      case HttpsUpgradesTestType::kHttpsFirstModeOnly:
        feature_list_.InitWithFeatures(
            /*enabled_features=*/{},
            /*disabled_features=*/{
                features::kHttpsFirstModeV2ForEngagedSites,
                features::kHttpsFirstModeV2ForTypicallySecureUsers,
                features::kHttpsFirstModeIncognito,
                features::kHttpsFirstBalancedMode});
        break;

      case HttpsUpgradesTestType::kHttpsFirstModeWithSiteEngagement:
        // HFM pref is disabled in SetUpOnMainThread.
        feature_list_.InitWithFeatures(
            /*enabled_features=*/{features::kHttpsFirstModeV2ForEngagedSites,
                                  features::kHttpsFirstBalancedMode},
            /*disabled_features=*/{
                features::kHttpsFirstModeV2ForTypicallySecureUsers});
        break;

      case HttpsUpgradesTestType::kHttpsFirstModeForTypicallySecureUsers:
        // HFM pref is disabled in SetUpOnMainThread.
        feature_list_.InitWithFeatures(
            /*enabled_features=*/{features::
                                      kHttpsFirstModeV2ForTypicallySecureUsers,
                                  features::kHttpsFirstBalancedMode},
            /*disabled_features=*/{features::kHttpsFirstModeV2ForEngagedSites});
        break;

      case HttpsUpgradesTestType::kAllAutoHFM:
        // HFM pref is disabled in SetUpOnMainThread.
        feature_list_.InitWithFeatures(
            /*enabled_features=*/{features::
                                      kHttpsFirstModeV2ForTypicallySecureUsers,
                                  features::kHttpsFirstModeV2ForEngagedSites,
                                  features::kHttpsFirstBalancedMode},
            /*disabled_features=*/{});
        break;

      case HttpsUpgradesTestType::kHttpsFirstModeIncognito:
        feature_list_.InitWithFeatures(
            /*enabled_features=*/{features::kHttpsFirstModeIncognito},
            /*disabled_features=*/{});
        break;

      case HttpsUpgradesTestType::kHttpsFirstBalancedMode:
        feature_list_.InitWithFeatures(
            /*enabled_features=*/{features::kHttpsFirstBalancedMode},
            /*disabled_features=*/{
                features::kHttpsFirstModeV2ForTypicallySecureUsers,
                features::kHttpsFirstModeV2ForEngagedSites});
        break;

      // Enable HFM, HFM with Site Engagement heuristic, HFM for typically
      // secure users, and HFM in Incognito.
      case HttpsUpgradesTestType::kAll:
        // HFM pref is enabled in SetUpOnMainThread.
        feature_list_.InitWithFeatures(
            /*enabled_features=*/
            {
                features::kHttpsFirstModeV2ForEngagedSites,
                features::kHttpsFirstModeV2ForTypicallySecureUsers,
                features::kHttpsFirstModeForAdvancedProtectionUsers,
                features::kHttpsFirstModeIncognito,
                features::kHttpsFirstBalancedMode,
            },
            /*disabled_features=*/{});
        break;

      // Disable HFM, HFM with Site Engagement heuristic, HFM for Typically
      // Secure Users, and HFM in Incognito. (HFM pref is disabled in
      // SetUpOnMainThread.) This is equivalent to the baseline default of
      // HTTPS-Upgrades.
      case HttpsUpgradesTestType::kNone:
        feature_list_.InitWithFeatures(
            /*enabled_features=*/{},
            /*disabled_features=*/{
                features::kHttpsFirstModeV2ForEngagedSites,
                features::kHttpsFirstModeV2ForTypicallySecureUsers,
                features::kHttpsFirstBalancedMode});
        break;
    }

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // By default allow all hosts on HTTPS.
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    host_resolver()->AddRule("*", "127.0.0.1");

    // Set up "bad-https.com", "bad-https2.com" and
    // "nonunique-hostname-bad-https" as hostnames with an SSL error. HTTPS
    // upgrades to these hosts will fail.
    scoped_refptr<net::X509Certificate> cert(https_server_.GetCertificate());
    net::CertVerifyResult verify_result;
    verify_result.is_issued_by_known_root = false;
    verify_result.verified_cert = cert;
    verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;
    mock_cert_verifier_.mock_cert_verifier()->AddResultForCertAndHost(
        cert, "bad-https.com", verify_result,
        net::ERR_CERT_COMMON_NAME_INVALID);
    mock_cert_verifier_.mock_cert_verifier()->AddResultForCertAndHost(
        cert, "www.bad-https.com", verify_result,
        net::ERR_CERT_COMMON_NAME_INVALID);
    mock_cert_verifier_.mock_cert_verifier()->AddResultForCertAndHost(
        cert, "bad-https2.com", verify_result,
        net::ERR_CERT_COMMON_NAME_INVALID);
    mock_cert_verifier_.mock_cert_verifier()->AddResultForCertAndHost(
        cert, "nonunique-hostname-bad-https", verify_result,
        net::ERR_CERT_COMMON_NAME_INVALID);

    http_server_.AddDefaultHandlers(GetChromeTestDataDir());
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(http_server_.Start());
    ASSERT_TRUE(https_server_.Start());

    HttpsUpgradesInterceptor::SetHttpsPortForTesting(https_server()->port());
    HttpsUpgradesInterceptor::SetHttpPortForTesting(http_server()->port());

    // Incognito tests swap out the default Browser instance for an Incognito
    // window, and then should behave like kHttpsFirstMode type tests but
    // without enabling the full HFM pref.
    if (https_upgrades_test_type() ==
        HttpsUpgradesTestType::kHttpsFirstModeIncognito) {
      UseIncognitoBrowser();
      SetPref(false);
    }

    // Only enable the HTTPS-First Mode pref when the test config calls for it.
    // Some of the HFM heuristics check that the preference wasn't set so as
    // not to override user preference (e.g. if the user changed the pref by
    // turning it off from the UI, we don't want to override it).
    if (IsHttpsFirstModePrefEnabled()) {
      SetPref(true);
    }

    if (InBalancedMode()) {
      SetBalancedPref(true);
    }
  }

  void TearDownOnMainThread() override {
    browser()->profile()->GetPrefs()->ClearPref(prefs::kHttpsOnlyModeEnabled);
    browser()->profile()->GetPrefs()->ClearPref(
        prefs::kHttpsOnlyModeAutoEnabled);
    browser()->profile()->GetPrefs()->ClearPref(prefs::kHttpsUpgradeFallbacks);
    browser()->profile()->GetPrefs()->ClearPref(
        prefs::kHttpsUpgradeNavigations);
    browser()->profile()->GetPrefs()->ClearPref(prefs::kHttpsFirstBalancedMode);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  // Incognito testing support
  //
  // Returns the active Browser for the test type being run.
  Browser* GetBrowser() const {
    return incognito_browser_ ? incognito_browser_.get() : browser();
  }
  // Call to use an Incognito browser rather than the default.
  void UseIncognitoBrowser() {
    ASSERT_EQ(nullptr, incognito_browser_.get());
    incognito_browser_ = CreateIncognitoBrowser();
  }
  bool IsIncognito() const { return incognito_browser_ != nullptr; }
  bool OnlyInBalancedMode() const {
    return https_upgrades_test_type() ==
           HttpsUpgradesTestType::kHttpsFirstBalancedMode;
  }
  bool InBalancedMode() const {
    return https_upgrades_test_type() ==
               HttpsUpgradesTestType::kHttpsFirstBalancedMode ||
           https_upgrades_test_type() == HttpsUpgradesTestType::kAll;
  }

 protected:
  HttpsUpgradesTestType https_upgrades_test_type() const { return GetParam(); }

  void SetPref(bool enabled) {
    auto* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(prefs::kHttpsOnlyModeEnabled, enabled);
  }

  bool GetPref() const {
    auto* prefs = browser()->profile()->GetPrefs();
    return prefs->GetBoolean(prefs::kHttpsOnlyModeEnabled);
  }

  void SetBalancedPref(bool enabled) {
    auto* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(prefs::kHttpsFirstBalancedMode, enabled);
  }

  bool GetBalancedPref() const {
    auto* prefs = browser()->profile()->GetPrefs();
    return prefs->GetBoolean(prefs::kHttpsFirstBalancedMode);
  }

  void ProceedThroughInterstitial(content::WebContents* tab) {
    content::TestNavigationObserver nav_observer(tab, 1);
    std::string javascript = "window.certificateErrorPageController.proceed();";
    ASSERT_TRUE(content::ExecJs(tab, javascript));
    nav_observer.Wait();
  }

  void DontProceedThroughInterstitial(content::WebContents* tab) {
    content::TestNavigationObserver nav_observer(tab, 1);
    std::string javascript =
        "window.certificateErrorPageController.dontProceed();";
    ASSERT_TRUE(content::ExecJs(tab, javascript));
    nav_observer.Wait();
  }

  void NavigateAndWaitForFallback(content::WebContents* tab, const GURL& url) {
    // TODO(crbug.com/40248833): With fallback as part of the same navigation,
    // this helper is no longer particularly useful. Consider updating callers.
    content::NavigateToURLBlockUntilNavigationsComplete(tab, url, 1);
  }

  // Whether HFM is enabled by the UI setting.
  bool IsHttpsFirstModePrefEnabled() const {
    return https_upgrades_test_type() ==
               HttpsUpgradesTestType::kHttpsFirstModeOnly ||
           https_upgrades_test_type() == HttpsUpgradesTestType::kAll;
  }

  // Whether HFM is enabled for many sites, and thus the tests should run steps
  // that assume the HTTP interstitial will trigger (i.e., for fallback HTTP
  // navigations when HTTPS-First Mode is enabled).
  bool IsHttpsFirstModeInterstitialEnabledAcrossSites() const {
    return IsHttpsFirstModePrefEnabled() || InBalancedMode() || IsIncognito();
  }

  // Whether HTTPS-First Mode with Site Engagement Heuristic is enabled. When
  // enabled, this feature will enable HFM on sites that have high Site
  // Engagement scores on their HTTPS URLs. HFM with Site Engagement requires
  // HTTPS-Upgrades to be enabled.
  bool IsSiteEngagementHeuristicEnabled() const {
    bool enabled =
        https_upgrades_test_type() ==
            HttpsUpgradesTestType::kHttpsFirstModeWithSiteEngagement ||
        https_upgrades_test_type() == HttpsUpgradesTestType::kAllAutoHFM ||
        https_upgrades_test_type() == HttpsUpgradesTestType::kAll;
    return enabled;
  }

  // Whether automatic HTTPS-First Mode for typically secure users is enabled.
  // When enabled, this feature will enable HFM for users who would see HFM
  // warnings very rarely. HFM for typically secure users requires
  // HTTPS-Upgrades to be enabled.
  bool IsTypicallySecureUserFeatureEnabled() const {
    bool enabled =
        https_upgrades_test_type() ==
            HttpsUpgradesTestType::kHttpsFirstModeForTypicallySecureUsers ||
        https_upgrades_test_type() == HttpsUpgradesTestType::kAllAutoHFM ||
        https_upgrades_test_type() == HttpsUpgradesTestType::kAll;
    return enabled;
  }

  void SetSiteEngagementScore(const GURL& url, double score) {
    site_engagement::SiteEngagementService* service =
        site_engagement::SiteEngagementService::Get(browser()->profile());
    service->ResetBaseScoreForURL(url, score);
    ASSERT_EQ(score, service->GetScore(url));
  }

  // Checks that the HTTPS-First Mode interstitial has been shown for the
  // correct reasons.
  void CheckInterstitialReasonHistogram(
      const ExpectedInterstitialReasons& expected_reasons) {
    histograms()->ExpectTotalCount(kInterstitialReasonHistogram,
                                   expected_reasons.pref +
                                       expected_reasons.typically_secure_user +
                                       expected_reasons.balanced);
    histograms()->ExpectBucketCount(kInterstitialReasonHistogram,
                                    static_cast<int>(InterstitialReason::kPref),
                                    expected_reasons.pref);
    histograms()->ExpectBucketCount(
        kInterstitialReasonHistogram,
        static_cast<int>(InterstitialReason::kBalanced),
        expected_reasons.balanced);
    histograms()->ExpectBucketCount(
        kInterstitialReasonHistogram,
        static_cast<int>(InterstitialReason::kTypicallySecureUserHeuristic),
        expected_reasons.typically_secure_user);
  }

  // Verifies that an HFM interstitial is shown.
  void ExpectInterstitial(content::WebContents* contents) {
    EXPECT_EQ(HFMInterstitialType::kStandard,
              chrome_browser_interstitials::GetHFMInterstitialType(contents));
  }

  // Verifies that an HFM interstitial is shown only if the HFM-pref is enabled
  // or we're in balanced mode.
  void ExpectInterstitialOnlyIfPrefIsSetOrInBalancedMode(
      content::WebContents* contents) {
    if (IsHttpsFirstModePrefEnabled() || InBalancedMode()) {
      ExpectInterstitial(contents);
    } else {
      EXPECT_FALSE(
          chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
              contents));
    }
  }

  net::EmbeddedTestServer* http_server() { return &http_server_; }
  net::EmbeddedTestServer* https_server() { return &https_server_; }
  base::HistogramTester* histograms() { return &histograms_; }

  void EnableCaptivePortalDetection(Browser* browser);

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer http_server_{net::EmbeddedTestServer::TYPE_HTTP};
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  content::ContentMockCertVerifier mock_cert_verifier_;
  base::HistogramTester histograms_;
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> incognito_browser_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    HttpsUpgradesBrowserTest,
    ::testing::Values(
        HttpsUpgradesTestType::kHttpsFirstModeOnly,
        HttpsUpgradesTestType::kHttpsFirstModeWithSiteEngagement,
        HttpsUpgradesTestType::kHttpsFirstModeForTypicallySecureUsers,
        HttpsUpgradesTestType::kAllAutoHFM,
        HttpsUpgradesTestType::kHttpsFirstModeIncognito,
        HttpsUpgradesTestType::kHttpsFirstBalancedMode,
        HttpsUpgradesTestType::kAll,
        HttpsUpgradesTestType::kNone),
    // Map param to a human-readable string for better test output.
    [](testing::TestParamInfo<HttpsUpgradesTestType> input_type)
        -> std::string {
      switch (input_type.param) {
        case HttpsUpgradesTestType::kHttpsFirstModeOnly:
          return "HttpsFirstModeOnly";
        case HttpsUpgradesTestType::kHttpsFirstModeWithSiteEngagement:
          return "HttpsFirstModeWithSiteEngagement";
        case HttpsUpgradesTestType::kHttpsFirstModeForTypicallySecureUsers:
          return "HttpsFirstModeForTypicallySecureUsers";
        case HttpsUpgradesTestType::kAllAutoHFM:
          return "AllAutoHFM";
        case HttpsUpgradesTestType::kHttpsFirstModeIncognito:
          return "HttpsFirstModeIncognito";
        case HttpsUpgradesTestType::kHttpsFirstBalancedMode:
          return "HttpsFirstBalancedMode";
        case HttpsUpgradesTestType::kAll:
          return "AllFeatures";
        case HttpsUpgradesTestType::kNone:
          return "None";
      }
    });

// If the user navigates to an HTTP URL for a site that supports HTTPS, the
// navigation should end up on the HTTPS version of the URL if upgrading is
// enabled.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       UrlWithHttpScheme_ShouldUpgrade) {
  GURL http_url = http_server()->GetURL("foo.com", "/simple.html");
  GURL https_url = https_server()->GetURL("foo.com", "/simple.html");

  // The NavigateToURL() call returns `false` because the navigation is
  // redirected to HTTPS.
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(contents, 1);
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  nav_observer.Wait();

  EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));

  EXPECT_EQ(https_url, contents->GetLastCommittedURL());
  histograms()->ExpectTotalCount(kEventHistogram, 2);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeSucceeded, 1);

  // Also record general request metrics.
  histograms()->ExpectTotalCount(kNavigationRequestSecurityLevelHistogram, 2);
  histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                  NavigationRequestSecurityLevel::kSecure, 1);
  histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                  NavigationRequestSecurityLevel::kUpgraded, 1);
}

// If the user navigates to an HTTPS URL for a site that supports HTTPS, the
// navigation should end up on that exact URL.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       UrlWithHttpsScheme_ShouldLoad) {
  GURL https_url = https_server()->GetURL("foo.com", "/simple.html");
  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::NavigateToURL(contents, https_url));

  // Verify that navigation event metrics were not recorded as the navigation
  // was not upgraded.
  histograms()->ExpectTotalCount(kEventHistogram, 0);

  // General navigation metrics should still be recorded.
  histograms()->ExpectTotalCount(kNavigationRequestSecurityLevelHistogram, 1);
  histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                  NavigationRequestSecurityLevel::kSecure, 1);
}

// If the user navigates to a localhost URL, the navigation should end up on
// that exact URL.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, Localhost_ShouldNotUpgrade) {
  GURL localhost_url = http_server()->GetURL("localhost", "/simple.html");
  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::NavigateToURL(contents, localhost_url));

  // Verify that navigation event metrics were not recorded as the navigation
  // was not upgraded.
  histograms()->ExpectTotalCount(kEventHistogram, 0);

  // Verify that general navigation request metrics were recorded.
  histograms()->ExpectTotalCount(kNavigationRequestSecurityLevelHistogram, 1);
  histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                  NavigationRequestSecurityLevel::kLocalhost,
                                  1);
}

// Test that HTTPS Upgrades are skipped for non-unique hostnames, such as
// non-publicly routable (RFC1918/4193) IP addresses, but HTTPS-First Mode
// should still apply.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       NonRoutableIPAddress_ShouldNotUpgrade) {
  // Disable the testing port configuration, as this test doesn't use the
  // EmbeddedTestServer.
  HttpsUpgradesInterceptor::SetHttpsPortForTesting(0);
  HttpsUpgradesInterceptor::SetHttpPortForTesting(0);

  // Set up an interceptor because the test server can't listen on private IPs.
  GURL local_ip_url("http://192.168.0.1/simple.html");
  auto url_loader_interceptor =
      content::URLLoaderInterceptor::ServeFilesFromDirectoryAtOrigin(
          GetChromeTestDataDir().MaybeAsASCII(),
          local_ip_url.GetWithEmptyPath());

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();

  if (IsHttpsFirstModePrefEnabled()) {
    // HFM should attempt the upgrade, fail, and fallback to the interstitial.
    EXPECT_FALSE(content::NavigateToURL(contents, local_ip_url));
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));

    // Verify that upgrade events were recorded because an upgrade was attempted
    // and failed.
    histograms()->ExpectTotalCount(kEventHistogram, 3);
    histograms()->ExpectBucketCount(
        kEventHistogram,
        security_interstitials::https_only_mode::Event::kUpgradeAttempted, 1);
    histograms()->ExpectBucketCount(
        kEventHistogram,
        security_interstitials::https_only_mode::Event::kUpgradeFailed, 1);
    histograms()->ExpectBucketCount(
        kEventHistogram,
        security_interstitials::https_only_mode::Event::kUpgradeTimedOut, 1);
  } else {
    // If HFM is not enabled, HTTPS-Upgrades should not attempt to upgrade the
    // navigation.
    EXPECT_TRUE(content::NavigateToURL(contents, local_ip_url));
    histograms()->ExpectTotalCount(kEventHistogram, 0);
  }

  histograms()->ExpectBucketCount(
      kNavigationRequestSecurityLevelHistogram,
      NavigationRequestSecurityLevel::kNonUniqueHostname, 1);
}

// Test that unique single-label hostnames (e.g. gTLDs) are upgraded in all
// modes, but warnings are only shown in strict and incognito modes.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       UniqueSingleLabel_NoWarnInBalancedMode) {
  // Disable the testing port configuration, as this test doesn't use the
  // EmbeddedTestServer.
  HttpsUpgradesInterceptor::SetHttpsPortForTesting(0);
  HttpsUpgradesInterceptor::SetHttpPortForTesting(0);
  GURL singlelabel_url = http_server()->GetURL("cl", "/simple.html");

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();

  if (IsHttpsFirstModePrefEnabled() || IsIncognito()) {
    // HFM should attempt the upgrade, fail, and fallback to the interstitial.
    EXPECT_FALSE(content::NavigateToURL(contents, singlelabel_url));
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    histograms()->ExpectTotalCount(kNavigationRequestSecurityLevelHistogram, 2);
  } else {
    // Otherwise, the request should attempt the upgrade, fail, and fallback to
    // HTTP _without_ an interstitial.
    NavigateAndWaitForFallback(contents, singlelabel_url);
    EXPECT_EQ(singlelabel_url, contents->GetLastCommittedURL());
    EXPECT_FALSE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kInsecure,
                                    1);
    histograms()->ExpectTotalCount(kNavigationRequestSecurityLevelHistogram, 3);
  }

  // Verify that upgrade events were recorded because an upgrade was attempted
  // and failed no matter what.
  histograms()->ExpectTotalCount(kEventHistogram, 3);
  histograms()->ExpectBucketCount(
      kEventHistogram,
      security_interstitials::https_only_mode::Event::kUpgradeAttempted, 1);
  histograms()->ExpectBucketCount(
      kEventHistogram,
      security_interstitials::https_only_mode::Event::kUpgradeFailed, 1);
  histograms()->ExpectBucketCount(
      kEventHistogram,
      security_interstitials::https_only_mode::Event::kUpgradeTimedOut, 1);

  histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                  NavigationRequestSecurityLevel::kUpgraded, 1);
  histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                  NavigationRequestSecurityLevel::kSecure, 1);
}

// If the user navigates to a non-unique hostname, the navigation should be
// upgraded, but record insecure metrics.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, NonUniqueHost_RecordsMetrics) {
  GURL nonunique_url1 = http_server()->GetURL("test.local", "/simple.html");
  GURL nonunique_url2 = http_server()->GetURL("test", "/simple.html");
  // Note that we don't test with an RFC1918 IP because the test server
  // wouldn't receive the traffic (since it relies on DNS).

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  if (IsHttpsFirstModePrefEnabled()) {
    EXPECT_FALSE(content::NavigateToURL(contents, nonunique_url1));
    EXPECT_FALSE(content::NavigateToURL(contents, nonunique_url2));
    // Other histograms are still recorded.
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kUpgraded,
                                    2);
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kSecure, 2);
  } else {
    // When HFM is not enabled but upgrading is, Chrome does NOT upgrade, so
    // other histograms are not recorded.
    EXPECT_TRUE(content::NavigateToURL(contents, nonunique_url1));
    EXPECT_TRUE(content::NavigateToURL(contents, nonunique_url2));
    histograms()->ExpectTotalCount(kNavigationRequestSecurityLevelHistogram, 2);
  }

  histograms()->ExpectBucketCount(
      kNavigationRequestSecurityLevelHistogram,
      NavigationRequestSecurityLevel::kNonUniqueHostname, 2);
}

// Test that non-default ports (e.g. not HTTP80) are upgraded in all
// modes, but warnings are only shown in strict and incognito modes.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       NonDefaultPorts_NoWarnInBalancedMode) {
  // Disable the testing port configuration, as this test doesn't use the
  // EmbeddedTestServer.
  HttpsUpgradesInterceptor::SetHttpsPortForTesting(0);
  HttpsUpgradesInterceptor::SetHttpPortForTesting(0);

  // Set up an interceptor so we can test non-default (and non-testing) ports.
  GURL non_default_http_url = GURL("http://example.com:8080/simple.html");
  auto url_loader_interceptor =
      content::URLLoaderInterceptor::ServeFilesFromDirectoryAtOrigin(
          GetChromeTestDataDir().MaybeAsASCII(),
          non_default_http_url.GetWithEmptyPath());

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();

  if (IsHttpsFirstModePrefEnabled() || IsIncognito()) {
    // HFM should attempt the upgrade, fail, and fallback to the interstitial.
    EXPECT_FALSE(content::NavigateToURL(contents, non_default_http_url));
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    histograms()->ExpectTotalCount(kNavigationRequestSecurityLevelHistogram, 2);
  } else {
    // Otherwise, the request should attempt the upgrade, fail, and fallback to
    // HTTP _without_ an interstitial.
    NavigateAndWaitForFallback(contents, non_default_http_url);
    EXPECT_EQ(non_default_http_url, contents->GetLastCommittedURL());
    EXPECT_FALSE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kInsecure,
                                    1);
    histograms()->ExpectTotalCount(kNavigationRequestSecurityLevelHistogram, 3);
  }

  // Verify that upgrade events were recorded because an upgrade was attempted
  // and failed no matter what.
  histograms()->ExpectTotalCount(kEventHistogram, 3);
  histograms()->ExpectBucketCount(
      kEventHistogram,
      security_interstitials::https_only_mode::Event::kUpgradeAttempted, 1);
  histograms()->ExpectBucketCount(
      kEventHistogram,
      security_interstitials::https_only_mode::Event::kUpgradeFailed, 1);
  histograms()->ExpectBucketCount(
      kEventHistogram,
      security_interstitials::https_only_mode::Event::kUpgradeNetError, 1);

  histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                  NavigationRequestSecurityLevel::kUpgraded, 1);
  histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                  NavigationRequestSecurityLevel::kSecure, 1);
}

// If the user navigates to an HTTPS URL, the navigation should end up on that
// exact URL, even if the site has an SSL error.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       UrlWithHttpsScheme_BrokenSSL_ShouldNotFallback) {
  GURL https_url = https_server()->GetURL("bad-https.com", "/simple.html");

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(contents, https_url));
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());

  // The SSL error should show regardless of the HFM state.
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(contents));

  // Verify that navigation event metrics were not recorded as the navigation
  // was not upgraded.
  histograms()->ExpectTotalCount(kEventHistogram, 0);
}

// If the user navigates to an HTTP URL for a site with broken HTTPS, the
// navigation should end up on the HTTPS URL and show the HTTPS-Only Mode
// interstitial.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       UrlWithHttpScheme_BrokenSSL_ShouldInterstitial) {
  GURL http_url = http_server()->GetURL("bad-https.com", "/simple.html");
  GURL https_url = https_server()->GetURL("bad-https.com", "/simple.html");

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  if (IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  }

  // Verify that navigation event metrics were correctly recorded.
  histograms()->ExpectTotalCount(kEventHistogram, 3);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeFailed, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeCertError, 1);
}

// HTTPS-First Mode in Incognito should customize the interstitial.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       IncognitoInterstitialVariation) {
  // This test only applies to fully-enabled HFM and HFM-in-Incognito.
  if (!IsHttpsFirstModePrefEnabled() && !IsIncognito()) {
    return;
  }

  GURL http_url = http_server()->GetURL("bad-https.com", "/simple.html");
  GURL https_url = https_server()->GetURL("bad-https.com", "/simple.html");

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  if (IsHttpsFirstModePrefEnabled()) {
    EXPECT_EQ(HFMInterstitialType::kStandard,
              chrome_browser_interstitials::GetHFMInterstitialType(contents));
  } else if (IsIncognito()) {
    // Test that HFM-in-Incognito overrides the default interstitial text.
    EXPECT_EQ(HFMInterstitialType::kIncognito,
              chrome_browser_interstitials::GetHFMInterstitialType(contents));
  }
}

void MaybeEnableHttpsFirstModeForEngagedSitesAndWait(
    HttpsFirstModeService* hfm_service) {
  base::RunLoop run_loop;
  hfm_service->MaybeEnableHttpsFirstModeForEngagedSites(run_loop.QuitClosure());
  run_loop.Run();
}

// Returns a URL loader interceptor that responds to HTTPS URLs with a cert
// error and to HTTP URLs with a good response.
std::unique_ptr<content::URLLoaderInterceptor>
MakeInterceptorForSiteEngagementHeuristic() {
  return std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [](content::URLLoaderInterceptor::RequestParams* params) {
            if (params->url_request.url.SchemeIs("https")) {
              // Fail with an SSL error.
              network::URLLoaderCompletionStatus status;
              status.error_code = net::ERR_CERT_COMMON_NAME_INVALID;
              status.ssl_info = net::SSLInfo();
              status.ssl_info->cert_status =
                  net::CERT_STATUS_COMMON_NAME_INVALID;
              // The cert doesn't matter.
              status.ssl_info->cert = net::ImportCertFromFile(
                  net::GetTestCertsDirectory(), "ok_cert.pem");
              status.ssl_info->unverified_cert = status.ssl_info->cert;
              params->client->OnComplete(status);
              return true;
            }
            content::URLLoaderInterceptor::WriteResponse(
                "HTTP/1.1 200 OK\nContent-type: text/html\n\n",
                "<html>Done</html>", params->client.get());
            return true;
          }));
}

// TODO(crbug.com/40904694): Fails on the linux-wayland-rel bot.
#if defined(OZONE_PLATFORM_WAYLAND)
#define MAYBE_UrlWithHttpScheme_BrokenSSL_SiteEngagementHeuristic_ShouldInterstitial \
  DISABLED_UrlWithHttpScheme_BrokenSSL_SiteEngagementHeuristic_ShouldInterstitial
#else
#define MAYBE_UrlWithHttpScheme_BrokenSSL_SiteEngagementHeuristic_ShouldInterstitial \
  UrlWithHttpScheme_BrokenSSL_SiteEngagementHeuristic_ShouldInterstitial
#endif
// Test for Site Engagement Heuristic, a feature that enables HFM on specific
// sites based on their site engagement scores.
// If the user navigates to an HTTP URL for a site with broken HTTPS, the
// navigation should end up on the HTTPS URL and show the HTTPS-Only Mode
// interstitial. It should also record a separate histogram for Site Engagement
// Heuristic if the interstitial isn't enabled.
IN_PROC_BROWSER_TEST_P(
    HttpsUpgradesBrowserTest,
    MAYBE_UrlWithHttpScheme_BrokenSSL_SiteEngagementHeuristic_ShouldInterstitial) {
  // HFM+SE is not enabled in Incognito.
  if (IsIncognito()) {
    return;
  }
  // Disable the testing port configuration, as this test doesn't use the
  // EmbeddedTestServer.
  HttpsUpgradesInterceptor::SetHttpsPortForTesting(0);
  HttpsUpgradesInterceptor::SetHttpPortForTesting(0);
  auto url_loader_interceptor = MakeInterceptorForSiteEngagementHeuristic();

  content::WebContents* contents =
      GetBrowser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = GetBrowser()->profile();
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();

  // Set test clock.
  auto clock = std::make_unique<base::SimpleTestClock>();
  auto* clock_ptr = clock.get();
  StatefulSSLHostStateDelegate* chrome_state =
      static_cast<StatefulSSLHostStateDelegate*>(state);
  chrome_state->SetClockForTesting(std::move(clock));

  // Start the clock at standard system time.
  clock_ptr->SetNow(base::Time::NowFromSystemTime());

  GURL http_url("http://bad-https.com");
  GURL https_url("https://bad-https.com");
  SetSiteEngagementScore(http_url, kLowSiteEngagementScore);
  SetSiteEngagementScore(https_url, kHighSiteEnagementScore);
  HttpsFirstModeService* hfm_service =
      HttpsFirstModeServiceFactory::GetForProfile(profile);
  MaybeEnableHttpsFirstModeForEngagedSitesAndWait(hfm_service);
  const bool is_interstitial_due_to_se_heuristic =
      IsSiteEngagementHeuristicEnabled() && !IsHttpsFirstModePrefEnabled() &&
      !InBalancedMode();

  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  if (IsHttpsFirstModeInterstitialEnabledAcrossSites() ||
      IsSiteEngagementHeuristicEnabled()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    EXPECT_EQ(is_interstitial_due_to_se_heuristic
                  ? HFMInterstitialType::kSiteEngagement
                  : HFMInterstitialType::kStandard,
              chrome_browser_interstitials::GetHFMInterstitialType(contents));
  } else {
    EXPECT_EQ(HFMInterstitialType::kNone,
              chrome_browser_interstitials::GetHFMInterstitialType(contents));
  }

  // Verify that navigation event metrics were correctly recorded.
  histograms()->ExpectTotalCount(kEventHistogram, 3);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeFailed, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeCertError, 1);

  // Check engagement heuristic metrics. These are only recorded when the
  // site engagement heuristic is enabled and the interstitial is due to this
  // heuristic and not because of prefs.
  if (is_interstitial_due_to_se_heuristic) {
    histograms()->ExpectTotalCount(kEventHistogramWithEngagementHeuristic, 3);
    histograms()->ExpectBucketCount(kEventHistogramWithEngagementHeuristic,
                                    Event::kUpgradeAttempted, 1);
    histograms()->ExpectBucketCount(kEventHistogramWithEngagementHeuristic,
                                    Event::kUpgradeFailed, 1);
    histograms()->ExpectBucketCount(kEventHistogramWithEngagementHeuristic,
                                    Event::kUpgradeCertError, 1);
    // Check the heuristic state.
    histograms()->ExpectTotalCount(kSiteEngagementHeuristicStateHistogram, 1);
    histograms()->ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                                    SiteEngagementHeuristicState::kDisabled, 0);
    histograms()->ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                                    SiteEngagementHeuristicState::kEnabled, 1);
    // Check host count.
    histograms()->ExpectTotalCount(kSiteEngagementHeuristicHostCountHistogram,
                                   1);
    histograms()->ExpectBucketCount(kSiteEngagementHeuristicHostCountHistogram,
                                    0,
                                    /*expected_count=*/0);
    histograms()->ExpectBucketCount(kSiteEngagementHeuristicHostCountHistogram,
                                    1,
                                    /*expected_count=*/1);
    // Check accumulated host count.
    histograms()->ExpectTotalCount(
        kSiteEngagementHeuristicAccumulatedHostCountHistogram, 1);
    histograms()->ExpectBucketCount(
        kSiteEngagementHeuristicAccumulatedHostCountHistogram, 0,
        /*expected_count=*/0);
    histograms()->ExpectBucketCount(
        kSiteEngagementHeuristicAccumulatedHostCountHistogram, 1,
        /*expected_count=*/1);
    // Check enforcement duration. Since the host isn't removed from HFM
    // enforcement list, no duration should be recorded yet.
    histograms()->ExpectTotalCount(
        kSiteEngagementHeuristicEnforcementDurationHistogram, 0);
  } else {
    histograms()->ExpectTotalCount(kEventHistogramWithEngagementHeuristic, 0);
  }

  // Lower HTTPS engagement score. This disables HFM on the site. Also advance
  // the clock.
  SetSiteEngagementScore(https_url, 5);
  clock_ptr->Advance(base::Hours(1));
  MaybeEnableHttpsFirstModeForEngagedSitesAndWait(hfm_service);

  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  // Should only show the interstitial if the HFM pref is enabled. Site
  // engagement heuristic alone will no longer cause an interstitial.
  if (IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));

    // Proceed through the interstitial, which will add the host to the
    // allowlist and navigate to the HTTP fallback URL.
    ProceedThroughInterstitial(contents);

    // Verify that the interstitial metrics were correctly recorded. The
    // interstitial was shown twice, once clicked through and once not.
    histograms()->ExpectTotalCount("interstitial.https_first_mode.decision", 4);
    histograms()->ExpectBucketCount(
        "interstitial.https_first_mode.decision",
        security_interstitials::MetricsHelper::Decision::SHOW, 2);
    histograms()->ExpectBucketCount(
        "interstitial.https_first_mode.decision",
        security_interstitials::MetricsHelper::Decision::PROCEED, 1);
    histograms()->ExpectBucketCount(
        "interstitial.https_first_mode.decision",
        security_interstitials::MetricsHelper::Decision::DONT_PROCEED, 1);
  } else {
    EXPECT_FALSE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    if (IsSiteEngagementHeuristicEnabled()) {
      // Verify that the interstitial metrics were correctly recorded. The
      // interstitial was once and navigated away from.
      histograms()->ExpectTotalCount("interstitial.https_first_mode.decision",
                                     2);
      histograms()->ExpectBucketCount(
          "interstitial.https_first_mode.decision",
          security_interstitials::MetricsHelper::Decision::SHOW, 1);
      histograms()->ExpectBucketCount(
          "interstitial.https_first_mode.decision",
          security_interstitials::MetricsHelper::Decision::DONT_PROCEED, 1);
    } else {
      histograms()->ExpectTotalCount("interstitial.https_first_mode.decision",
                                     0);
    }
  }

  // Check engagement heuristic metrics. These are only recorded when the
  // site engagement heuristic is enabled and the interstitial is due to this
  // heuristic and not because of prefs.
  if (IsSiteEngagementHeuristicEnabled() &&
      !IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    // Event histogram shouldn't change because Site Engagement heuristic didn't
    // kick in.
    histograms()->ExpectTotalCount(kEventHistogramWithEngagementHeuristic, 3);

    // Check host count.
    histograms()->ExpectTotalCount(kSiteEngagementHeuristicHostCountHistogram,
                                   2);
    histograms()->ExpectBucketCount(kSiteEngagementHeuristicHostCountHistogram,
                                    0,
                                    /*expected_count=*/1);
    histograms()->ExpectBucketCount(kSiteEngagementHeuristicHostCountHistogram,
                                    1,
                                    /*expected_count=*/1);
    // Check accumulated host count.
    histograms()->ExpectTotalCount(
        kSiteEngagementHeuristicAccumulatedHostCountHistogram, 2);
    histograms()->ExpectBucketCount(
        kSiteEngagementHeuristicAccumulatedHostCountHistogram, 0,
        /*expected_count=*/0);
    histograms()->ExpectBucketCount(
        kSiteEngagementHeuristicAccumulatedHostCountHistogram, 1,
        /*expected_count=*/2);
    // Check enforcement duration. The host is now removed from HFM
    // enforcement list, so its HFM enforcement duration should be recorded now.
    histograms()->ExpectTotalCount(
        kSiteEngagementHeuristicEnforcementDurationHistogram, 1);
    histograms()->ExpectTimeBucketCount(
        kSiteEngagementHeuristicEnforcementDurationHistogram, base::Hours(1),
        1);
  } else {
    // Event histogram shouldn't change because Site Engagement heuristic didn't
    // kick in.
    histograms()->ExpectTotalCount(kEventHistogramWithEngagementHeuristic, 0);

    // If HFM pref was enabled, no SE metrics should be recorded because HFM
    // won't be auto-enabled.
    histograms()->ExpectTotalCount(kSiteEngagementHeuristicHostCountHistogram,
                                   0);
    histograms()->ExpectTotalCount(
        kSiteEngagementHeuristicAccumulatedHostCountHistogram, 0);
    histograms()->ExpectTotalCount(
        kSiteEngagementHeuristicEnforcementDurationHistogram, 0);
  }
}

// Test that Site Engagement Heuristic doesn't enforce HTTPS on URLs with
// non-default ports.
IN_PROC_BROWSER_TEST_P(
    HttpsUpgradesBrowserTest,
    UrlWithHttpScheme_BrokenSSL_SiteEngagementHeuristic_ShouldIgnoreUrlsWithNonDefaultPorts) {
  // HFM+SE is not enabled in Incognito.
  if (IsIncognito()) {
    return;
  }
  // Disable the testing port configuration, as this test doesn't use the
  // EmbeddedTestServer.
  HttpsUpgradesInterceptor::SetHttpsPortForTesting(0);
  HttpsUpgradesInterceptor::SetHttpPortForTesting(0);
  auto url_loader_interceptor = MakeInterceptorForSiteEngagementHeuristic();

  content::WebContents* contents =
      GetBrowser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = GetBrowser()->profile();
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();

  // Set test clock.
  auto clock = std::make_unique<base::SimpleTestClock>();
  auto* clock_ptr = clock.get();
  StatefulSSLHostStateDelegate* chrome_state =
      static_cast<StatefulSSLHostStateDelegate*>(state);
  chrome_state->SetClockForTesting(std::move(clock));

  // Start the clock at standard system time.
  clock_ptr->SetNow(base::Time::NowFromSystemTime());

  GURL http_url("http://bad-https.com");
  GURL https_url("https://bad-https.com");
  GURL navigated_url("http://bad-https.com:8080");

  SetSiteEngagementScore(http_url, kLowSiteEngagementScore);
  SetSiteEngagementScore(https_url, kHighSiteEnagementScore);
  HttpsFirstModeService* hfm_service =
      HttpsFirstModeServiceFactory::GetForProfile(profile);
  MaybeEnableHttpsFirstModeForEngagedSitesAndWait(hfm_service);

  // This URL should be upgraded by HTTPS-Upgrades, but not have HFM
  // auto-enabled on it because it has a non-default port.
  NavigateAndWaitForFallback(contents, navigated_url);
  EXPECT_EQ(navigated_url, contents->GetLastCommittedURL());

  // Balanced Mode also exempts non-default ports.
  if (IsHttpsFirstModePrefEnabled() || IsIncognito()) {
    EXPECT_EQ(HFMInterstitialType::kStandard,
              chrome_browser_interstitials::GetHFMInterstitialType(contents));
  } else {
    EXPECT_EQ(HFMInterstitialType::kNone,
              chrome_browser_interstitials::GetHFMInterstitialType(contents));
  }

  // Verify that navigation event metrics were correctly recorded.
  histograms()->ExpectTotalCount(kEventHistogram, 3);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeFailed, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeCertError, 1);

  // Engagement heuristic shouldn't handle any navigation events because we
  // didn't navigate to example.com.
  histograms()->ExpectTotalCount(kEventHistogramWithEngagementHeuristic, 0);

  // Check engagement heuristic metrics. These are only recorded when the
  // site engagement interstitial is enabled.
  if (IsSiteEngagementHeuristicEnabled() &&
      !IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    // Check the heuristic state. The heuristic should enable HFM for
    // example.com
    histograms()->ExpectTotalCount(kSiteEngagementHeuristicStateHistogram, 1);
    histograms()->ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                                    SiteEngagementHeuristicState::kDisabled, 0);
    histograms()->ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                                    SiteEngagementHeuristicState::kEnabled, 1);
    // Check host count.
    histograms()->ExpectTotalCount(kSiteEngagementHeuristicHostCountHistogram,
                                   1);
    histograms()->ExpectBucketCount(kSiteEngagementHeuristicHostCountHistogram,
                                    0,
                                    /*expected_count=*/0);
    histograms()->ExpectBucketCount(kSiteEngagementHeuristicHostCountHistogram,
                                    1,
                                    /*expected_count=*/1);
    // Check accumulated host count.
    histograms()->ExpectTotalCount(
        kSiteEngagementHeuristicAccumulatedHostCountHistogram, 1);
    histograms()->ExpectBucketCount(
        kSiteEngagementHeuristicAccumulatedHostCountHistogram, 0,
        /*expected_count=*/0);
    histograms()->ExpectBucketCount(
        kSiteEngagementHeuristicAccumulatedHostCountHistogram, 1,
        /*expected_count=*/1);
    // Check enforcement duration. Since the host isn't removed from HFM
    // enforcement list, no duration should be recorded yet.
    histograms()->ExpectTotalCount(
        kSiteEngagementHeuristicEnforcementDurationHistogram, 0);
  } else {
    histograms()->ExpectTotalCount(kEventHistogramWithEngagementHeuristic, 0);
  }
}

IN_PROC_BROWSER_TEST_P(
    HttpsUpgradesBrowserTest,
    PRE_UrlWithHttpScheme_BrokenSSL_ShouldInterstitial_TypicallySecureUser) {
  // HFM-for-Typically-Secure-Users is not enabled in Incognito.
  if (IsIncognito()) {
    return;
  }

  content::WebContents* contents =
      GetBrowser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());

  if (!IsHttpsFirstModePrefEnabled() && !InBalancedMode()) {
    // When HFM is not enabled via pref, these should never be set in this test.
    EXPECT_FALSE(
        profile->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
    EXPECT_FALSE(
        profile->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));
  }

  // Typically Secure User heuristic requires a minimum total site engagement
  // score.
  SetSiteEngagementScore(GURL("https://google.com"), 90);

  base::SimpleTestClock clock;
  base::Time now;
  EXPECT_TRUE(base::Time::FromUTCString("2023-10-15T06:00:00Z", &now));
  // Start the clock at standard system time.
  clock.SetNow(now);

  profile->SetCreationTimeForTesting(clock.Now() - base::Days(30));

  HttpsFirstModeService* hfm_service =
      HttpsFirstModeServiceFactory::GetForProfile(profile);
  hfm_service->SetClockForTesting(&clock);

  GURL http_url = http_server()->GetURL("bad-https.com", "/simple.html");
  GURL https_url = https_server()->GetURL("bad-https.com", "/simple.html");

  // Visit the HTTP URL. Profile age is old enough but we haven't been observing
  // navigations for long enough, so Typically Secure Users feature won't show
  // an interstitial here.
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  ExpectedInterstitialReasons expected_reasons;

  if (IsHttpsFirstModePrefEnabled()) {
    ExpectInterstitial(contents);
    expected_reasons.pref++;
  } else if (InBalancedMode()) {
    ExpectInterstitial(contents);
    expected_reasons.balanced++;
  } else {
    EXPECT_FALSE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  }
  CheckInterstitialReasonHistogram(expected_reasons);

  // Move the clock forward and revisit HTTP. Profile is old enough now, but
  // Typically Secure Users feature will only auto-enable HFM after a restart
  // and show an interstitial.
  clock.Advance(base::Days(15));
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  if (IsHttpsFirstModePrefEnabled()) {
    ExpectInterstitial(contents);
    expected_reasons.pref++;
  } else if (InBalancedMode()) {
    ExpectInterstitial(contents);
    expected_reasons.balanced++;
  } else {
    EXPECT_FALSE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  }
  CheckInterstitialReasonHistogram(expected_reasons);

  if (!IsHttpsFirstModePrefEnabled() && !InBalancedMode()) {
    EXPECT_FALSE(
        profile->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeEnabled));
    EXPECT_FALSE(
        profile->GetPrefs()->HasPrefPath(prefs::kHttpsOnlyModeAutoEnabled));
  }
}

// TODO(crbug.com/40925331): Fails on the linux-wayland-rel bot.
#if defined(OZONE_PLATFORM_WAYLAND)
#define MAYBE_UrlWithHttpScheme_BrokenSSL_ShouldInterstitial_TypicallySecureUser \
  DISABLED_UrlWithHttpScheme_BrokenSSL_ShouldInterstitial_TypicallySecureUser
#else
#define MAYBE_UrlWithHttpScheme_BrokenSSL_ShouldInterstitial_TypicallySecureUser \
  UrlWithHttpScheme_BrokenSSL_ShouldInterstitial_TypicallySecureUser
#endif
IN_PROC_BROWSER_TEST_P(
    HttpsUpgradesBrowserTest,
    MAYBE_UrlWithHttpScheme_BrokenSSL_ShouldInterstitial_TypicallySecureUser) {
  // HFM-for-Typically-Secure-Users is not enabled in Incognito.
  if (IsIncognito()) {
    return;
  }

  // Advance the clock to one day after the last fallback event, which happened
  // on the 15th day.
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::NowFromSystemTime() + base::Days(16));

  content::WebContents* contents =
      GetBrowser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());

  HttpsFirstModeService* hfm_service =
      HttpsFirstModeServiceFactory::GetForProfile(profile);
  // Do lots of navigations so that Typically Secure User can kick in.
  for (size_t i = 0; i < 1500; i++) {
    hfm_service->IncrementRecentNavigationCount();
  }

  hfm_service->SetClockForTesting(&clock);
  // HFM service runs this on startup, but we can't set the test clock before it
  // runs, and we need to move the clock forward for this to work. So call it
  // explicitly again here.
  hfm_service->CheckUserIsTypicallySecureAndMaybeEnableHttpsFirstBalancedMode();
  size_t initial_navigation_count = hfm_service->GetRecentNavigationCount();

  // Use a different hostname than the PRE_ test so that we don't hit the
  // allowlist.
  GURL http_url = http_server()->GetURL("bad-https2.com", "/simple.html");
  GURL https_url = https_server()->GetURL("bad-https2.com", "/simple.html");
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_EQ(initial_navigation_count + 1u,
            hfm_service->GetRecentNavigationCount());

  bool expect_interstitial = IsHttpsFirstModeInterstitialEnabledAcrossSites() ||
                             IsTypicallySecureUserFeatureEnabled();
  // Expect typically secure text only when HFM is auto-enabled, so exclude
  // HttpsUpgradesTestType::kAll where HFM is enabled via pref).
  bool expect_typically_secure_user_interstitial_text =
      https_upgrades_test_type() ==
          HttpsUpgradesTestType::kHttpsFirstModeForTypicallySecureUsers ||
      https_upgrades_test_type() == HttpsUpgradesTestType::kAllAutoHFM;

  ExpectedInterstitialReasons expected_reasons;
  if (expect_interstitial) {
    EXPECT_EQ(expect_typically_secure_user_interstitial_text
                  ? HFMInterstitialType::kTypicallySecure
                  : HFMInterstitialType::kStandard,
              chrome_browser_interstitials::GetHFMInterstitialType(contents));

    if (expect_typically_secure_user_interstitial_text) {
      expected_reasons.typically_secure_user++;
    } else if (IsHttpsFirstModePrefEnabled()) {
      expected_reasons.pref++;
    } else if (InBalancedMode()) {
      expected_reasons.balanced++;
    } else {
      NOTREACHED();
    }
  } else {
    EXPECT_FALSE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  }
  CheckInterstitialReasonHistogram(expected_reasons);

  // Move the clock forward a day and revisit HTTP. Should still show HFM
  // interstitial.
  clock.Advance(base::Days(1));
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_EQ(initial_navigation_count + 2u,
            hfm_service->GetRecentNavigationCount());

  if (expect_interstitial) {
    EXPECT_EQ(expect_typically_secure_user_interstitial_text
                  ? HFMInterstitialType::kTypicallySecure
                  : HFMInterstitialType::kStandard,
              chrome_browser_interstitials::GetHFMInterstitialType(contents));

    if (expect_typically_secure_user_interstitial_text) {
      expected_reasons.typically_secure_user++;
    } else if (IsHttpsFirstModePrefEnabled()) {
      expected_reasons.pref++;
    } else if (InBalancedMode()) {
      expected_reasons.balanced++;
    } else {
      NOTREACHED();
    }

  } else {
    EXPECT_FALSE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  }
  CheckInterstitialReasonHistogram(expected_reasons);

  // Disable HFM and HF-balanced-mode. Should no longer auto-enable it.
  SetPref(false);
  auto* state = static_cast<StatefulSSLHostStateDelegate*>(
      profile->GetSSLHostStateDelegate());
  state->SetHttpsFirstBalancedModeSuppressedForTesting(true);
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(initial_navigation_count + 3u,
            hfm_service->GetRecentNavigationCount());
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));

  // Re-enable HFM. Should now show HFM interstitial without the auto-enabled
  // text.
  SetPref(true);
  state->SetHttpsFirstBalancedModeSuppressedForTesting(false);
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_EQ(initial_navigation_count + 4u,
            hfm_service->GetRecentNavigationCount());

  EXPECT_EQ(HFMInterstitialType::kStandard,
            chrome_browser_interstitials::GetHFMInterstitialType(contents));
  expected_reasons.pref++;

  CheckInterstitialReasonHistogram(expected_reasons);
}

// Checks that navigation to a non-unique hostname counts as a fallback event
// for the Typically Secure User heuristic and does not auto-enable HFM even if
// all other conditions are satisfied.
IN_PROC_BROWSER_TEST_P(
    HttpsUpgradesBrowserTest,
    UrlWithHttpScheme_NonUniqueHostname_ShouldNotInterstitial_TypicallySecureUser) {
  // HFM-for-Typically-Secure-Users is not enabled in Incognito.
  if (IsIncognito()) {
    return;
  }
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::NowFromSystemTime());

  // Disable the testing port configuration, as this test doesn't use the
  // EmbeddedTestServer.
  HttpsUpgradesInterceptor::SetHttpsPortForTesting(0);
  HttpsUpgradesInterceptor::SetHttpPortForTesting(0);
  auto url_loader_interceptor = MakeInterceptorForSiteEngagementHeuristic();

  // Prepare the profile so that HFM can be automatically enabled:
  content::WebContents* contents =
      GetBrowser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  SetSiteEngagementScore(GURL("https://google.com:12345"), 90);
  profile->SetCreationTimeForTesting(clock.Now() - base::Days(30));
  HttpsFirstModeService* hfm_service =
      HttpsFirstModeServiceFactory::GetForProfile(profile);
  hfm_service->SetClockForTesting(&clock);
  // Also do lots of navigations.
  for (size_t i = 0; i < 1500; i++) {
    hfm_service->IncrementRecentNavigationCount();
  }
  clock.Advance(base::Days(15));

  // Navigate to an HTTP URL. This will start Typically Secure observation.
  GURL http_url("http://bad-https.com/simple.html");
  content::NavigateToURLBlockUntilNavigationsComplete(
      contents, http_url, /*number_of_navigations=*/1);
  ExpectInterstitialOnlyIfPrefIsSetOrInBalancedMode(contents);

  // Advance the clock and navigate to an HTTP URL again. This will drop the
  // first fallback event.
  clock.Advance(base::Days(35));
  content::NavigateToURLBlockUntilNavigationsComplete(
      contents, http_url, /*number_of_navigations=*/1);
  ExpectInterstitialOnlyIfPrefIsSetOrInBalancedMode(contents);

  // Then, navigate to a non-unique hostname. This will also show an
  // interstitial iff HFM pref is enabled. It'll also record a fallback entry
  // which will disable typically secure since we now have two fallbacks in
  // a short time.
  GURL url("http://test/simple.html");
  content::NavigateToURLBlockUntilNavigationsComplete(
      contents, url, /*number_of_navigations=*/1);
  if (IsHttpsFirstModePrefEnabled()) {
    ExpectInterstitial(contents);
  } else {
    EXPECT_FALSE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  }

  // Advance the clock and try auto-enabling HFM.
  clock.Advance(base::Days(1));
  hfm_service->CheckUserIsTypicallySecureAndMaybeEnableHttpsFirstBalancedMode();

  // The interstitial should only be displayed if HFM is enabled by the pref
  // and not by the Typically Secure User heuristic.
  content::NavigateToURLBlockUntilNavigationsComplete(
      contents, http_url, /*number_of_navigations=*/1);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  ExpectInterstitialOnlyIfPrefIsSetOrInBalancedMode(contents);
}

// Regression test for crbug.com/1441276. Sequence of events:
// 1. Loads http://example.com. This gets upgraded to https://example.com.
// 2. https://example.com has an iframe for https://nonexistentsite.com. It
//    navigates away immediately to http://example.com.
// 3. This causes a crash in
//    HttpsUpgradesInterceptor::MaybeCreateLoaderForResponse() for
//    nonexistentsite.com.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       LoadIFrameAndNavigateAway_ShouldNotCrash) {
  // Disable the testing port configuration, as this test doesn't use the
  // EmbeddedTestServer.
  HttpsUpgradesInterceptor::SetHttpsPortForTesting(0);
  HttpsUpgradesInterceptor::SetHttpPortForTesting(0);

  bool navigated_once = false;
  auto url_loader_interceptor = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [&navigated_once](
              content::URLLoaderInterceptor::RequestParams* params) {
            if (params->url_request.url == GURL("https://example.com")) {
              if (!navigated_once) {
                // Load an iframe that will result in an error and immediately
                // navigate away.
                content::URLLoaderInterceptor::WriteResponse(
                    "HTTP/1.1 200 OK\nContent-type: text/html\n\n",
                    "<html>"
                    "<iframe src='https://nonexistentsite.com'></iframe>"
                    "<script>window.location.href = "
                    "'http://example.com';</script></html>",
                    params->client.get());
                navigated_once = true;
                return true;
              }
              // Return a normal response the second time this is called,
              // otherwise the test will timeout due to navigating back and
              // forth between http and https URLs.
              content::URLLoaderInterceptor::WriteResponse(
                  "HTTP/1.1 200 OK\nContent-type: text/html\n\n",
                  "<html>Done</html>", params->client.get());
              return true;
            }

            if (params->url_request.url == GURL("http://example.com")) {
              content::URLLoaderInterceptor::WriteResponse(
                  "HTTP/1.1 200 OK\nContent-type: text/html\n\n",
                  "<html>Test</html>", params->client.get());
              return true;
            }

            if (params->url_request.url.host() == "nonexistentsite.com") {
              // This request must fail for the bug to trigger.
              params->client->OnComplete(network::URLLoaderCompletionStatus(
                  net::ERR_CONNECTION_RESET));
              return true;
            }
            return false;
          }));

  GURL http_url("http://example.com");
  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, http_url);
}

// If the user triggers an HTTPS-Only Mode interstitial for a host and then
// clicks through the interstitial, they should end up on the HTTP URL.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       InterstitialBypassed_HttpFallbackLoaded) {
  GURL http_url = http_server()->GetURL("bad-https.com", "/simple.html");

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, http_url);

  if (IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));

    // Proceed through the interstitial, which will add the host to the
    // allowlist and navigate to the HTTP fallback URL.
    ProceedThroughInterstitial(contents);

    // Verify that the interstitial metrics were correctly recorded.
    histograms()->ExpectTotalCount("interstitial.https_first_mode.decision", 2);
    histograms()->ExpectBucketCount(
        "interstitial.https_first_mode.decision",
        security_interstitials::MetricsHelper::Decision::SHOW, 1);
    histograms()->ExpectBucketCount(
        "interstitial.https_first_mode.decision",
        security_interstitials::MetricsHelper::Decision::PROCEED, 1);
  }

  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  // Verify that navigation event metrics were correctly recorded.
  histograms()->ExpectTotalCount(kEventHistogram, 3);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeFailed, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeCertError, 1);
}

// If the upgraded HTTPS URL is not available due to a net error, it should
// trigger the HTTPS-Only Mode interstitial and offer fallback.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       NetErrorOnUpgrade_ShouldInterstitial) {
  GURL http_url = http_server()->GetURL("foo.com", "/close-socket");
  GURL https_url = https_server()->GetURL("foo.com", "/close-socket");

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  if (IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  }

  // Verify that navigation event metrics were correctly recorded.
  histograms()->ExpectTotalCount(kEventHistogram, 3);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeFailed, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeNetError, 1);
}

// If the upgraded HTTPS URL is not available due to a potentially-transient
// exempted net error (here a DNS resolution error), show the regular net error
// page instead of the HTTPS-First Mode interstitial. If the network conditions
// change such that the network error no longer triggers, reloading the tab
// should continue the upgraded navigation, which will fail and trigger fallback
// to HTTP. (Regression test for crbug.com/1277211.)
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       ExemptNetErrorOnUpgrade_ShouldNotFallback) {
  // This test is only interesting when HTTPS-First Mode is enabled.
  if (!IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    return;
  }

  GURL http_url = http_server()->GetURL("bad-https.com", "/simple.html");
  GURL https_url = https_server()->GetURL("bad-https.com", "/simple.html");
  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();

  {
    // Set up an interceptor that will return ERR_NAME_NOT_RESOLVED. Navigating
    // to the HTTP URL should get upgraded to HTTPS, but fail with a net error
    // page on the HTTPS URL.
    auto dns_failure_interceptor =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](content::URLLoaderInterceptor::RequestParams* params) {
              params->client->OnComplete(network::URLLoaderCompletionStatus(
                  net::ERR_NAME_NOT_RESOLVED));
              return true;
            }));
    EXPECT_FALSE(content::NavigateToURL(contents, http_url));
    EXPECT_EQ(https_url, contents->GetLastCommittedURL());
    EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));

    // Reload the tab. The net error should still be showing as the navigation
    // still results in ERR_NAME_NOT_RESOLVED.
    content::TestNavigationObserver nav_observer(contents, 1);
    contents->GetController().Reload(content::ReloadType::NORMAL,
                                     /*check_for_repost=*/false);
    nav_observer.Wait();
    EXPECT_EQ(https_url, contents->GetLastCommittedURL());
    EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));
  }

  // Interceptor is now out of scope and no longer applies. Reload the tab and
  // the upgraded navigation should continue, fail due to the bad HTTPS on the
  // server, and fall back to HTTP.
  content::TestNavigationObserver nav_observer(contents, 1);
  contents->GetController().Reload(content::ReloadType::NORMAL,
                                   /*check_for_repost=*/false);
  nav_observer.Wait();

  if (IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    ASSERT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    ProceedThroughInterstitial(contents);
  }

  // Should now be on the HTTP URL and it should be allowlisted.
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  EXPECT_TRUE(state->IsHttpAllowedForHost(
      http_url.host(), contents->GetPrimaryMainFrame()->GetStoragePartition()));
}

// Test that if one site redirects to a non-existent site, that we show the
// regular net error page instead of the HTTPS-First Mode interstitial.
// (Regression test for crbug.com/1277211.)
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       RedirectToNonexistentSite_ShouldNotInterstitial) {
  // This test is only interesting when HTTPS-First Mode is enabled.
  if (!IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    return;
  }

  std::string nonexistent_domain = "nonexistentsite.com";
  GURL nonexistent_http_url =
      http_server()->GetURL(nonexistent_domain, "/simple.html");
  GURL nonexistent_https_url =
      https_server()->GetURL(nonexistent_domain, "/simple.html");
  std::string www_redirect_path =
      base::StrCat({"/server-redirect?", nonexistent_http_url.spec()});
  GURL redirecting_https_url =
      https_server()->GetURL("foo.com", www_redirect_path);
  GURL redirecting_http_url =
      http_server()->GetURL("foo.com", www_redirect_path);

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();

  // Set up an interceptor that will return ERR_NAME_NOT_RESOLVED for
  // nonexistentsite.com.
  auto dns_failure_interceptor =
      std::make_unique<content::URLLoaderInterceptor>(
          base::BindLambdaForTesting(
              [nonexistent_domain](
                  content::URLLoaderInterceptor::RequestParams* params) {
                if (params->url_request.url.host() == nonexistent_domain) {
                  params->client->OnComplete(network::URLLoaderCompletionStatus(
                      net::ERR_NAME_NOT_RESOLVED));
                  return true;
                }
                return false;
              }));

  // Navigating to the HTTP URL should get upgraded to HTTPS, but fail with a
  // net error page on the HTTPS URL.
  EXPECT_FALSE(content::NavigateToURL(contents, redirecting_http_url));
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));
  EXPECT_EQ(url::kHttpsScheme, contents->GetLastCommittedURL().scheme());
  EXPECT_EQ(nonexistent_domain, contents->GetLastCommittedURL().host());
}

// If the upgraded HTTPS URL is not available due to a potentially-transient
// exempted net error but the hostname is non-unique, don't show the net error
// page and instead just fallback to HTTP and the HTTPS-First Mode interstitial.
// Otherwise, the user can be stuck on the net error page when the HTTP version
// of the host would have resolved, such as for corp single-label hostnames.
// (Regression test for crbug.com/1451040.)
IN_PROC_BROWSER_TEST_P(
    HttpsUpgradesBrowserTest,
    ExemptNetErrorOnUpgrade_NonUniqueHostname_ShouldFallback) {
  // This test is only interesting when HTTPS-First Mode is fully enabled.
  if (!IsHttpsFirstModePrefEnabled()) {
    return;
  }

  GURL http_url = http_server()->GetURL("blorp", "/simple.html");
  GURL https_url = https_server()->GetURL("blorp", "/simple.html");
  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();

  // Set up an interceptor that will return ERR_NAME_NOT_RESOLVED. Navigating
  // to the HTTP URL should get upgraded to HTTPS, and then fallback to HTTP
  // and the HFM interstitial.
  auto dns_failure_interceptor =
      std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
          [](content::URLLoaderInterceptor::RequestParams* params) {
            params->client->OnComplete(
                network::URLLoaderCompletionStatus(net::ERR_NAME_NOT_RESOLVED));
            return true;
          }));
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(contents));
  ProceedThroughInterstitial(contents);

  // Should now be on the HTTP URL and it should be allowlisted.
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  EXPECT_TRUE(state->IsHttpAllowedForHost(
      http_url.host(), contents->GetPrimaryMainFrame()->GetStoragePartition()));
}

// If the upgraded HTTPS URL is not available due to an exempted net error but
// is to a single-label unique hostname (i.e. a TLD) don't show the net error
// page. This is the same as
// ExemptNetErrorOnUpgrade_NonUniqueHostname_ShouldFallback except with a unique
// one-label hostname.
// (Partial regression test for impact of crrev.com/c/5507613 on b/348330182.)
IN_PROC_BROWSER_TEST_P(
    HttpsUpgradesBrowserTest,
    ExemptNetErrorOnUpgrade_UniqueSingleLabelHostname_ShouldFallback) {
  // This test is only interesting when HTTPS-First Mode is enabled.
  if (!IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    return;
  }

  GURL http_url = http_server()->GetURL("cl", "/simple.html");
  GURL https_url = https_server()->GetURL("cl", "/simple.html");
  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();

  // Set up an interceptor that will return ERR_NAME_NOT_RESOLVED. Navigating
  // to the HTTP URL should get upgraded to HTTPS, and then fallback to HTTP
  // and the HFM interstitial.
  auto dns_failure_interceptor =
      std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
          [](content::URLLoaderInterceptor::RequestParams* params) {
            params->client->OnComplete(
                network::URLLoaderCompletionStatus(net::ERR_NAME_NOT_RESOLVED));
            return true;
          }));
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));

  // Balanced mode doesn't show the interstitial on any single-label hosts.
  if (OnlyInBalancedMode()) {
    EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));
  } else {
    EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(contents));
    ProceedThroughInterstitial(contents);
  }

  // Should now be on the HTTP URL and it should be allowlisted.
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  EXPECT_TRUE(state->IsHttpAllowedForHost(
      http_url.host(), contents->GetPrimaryMainFrame()->GetStoragePartition()));
}

// Navigations in subframes should not get upgraded by HTTPS-Only Mode. They
// should be blocked as mixed content.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       HttpsParentHttpSubframeNavigation_Blocked) {
  const GURL parent_url(
      https_server()->GetURL("foo.com", "/iframe_blank.html"));
  const GURL iframe_url(http_server()->GetURL("foo.com", "/simple.html"));

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::NavigateToURL(contents, parent_url));

  content::TestNavigationObserver nav_observer(contents, 1);
  EXPECT_TRUE(content::NavigateIframeToURL(contents, "test", iframe_url));
  nav_observer.Wait();
  EXPECT_NE(iframe_url, nav_observer.last_navigation_url());

  // Verify that no navigation event metrics were recorded.
  histograms()->ExpectTotalCount(kEventHistogram, 0);
}

// Navigating to an HTTP URL in a subframe of an HTTP page should not upgrade
// the subframe navigation to HTTPS (even if the subframe navigation is to a
// different host than the parent frame).
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       HttpParentHttpSubframeNavigation_NotUpgraded) {
  // The parent frame will fail to upgrade to HTTPS.
  const GURL parent_url(
      http_server()->GetURL("bad-https.com", "/iframe_blank.html"));
  const GURL iframe_url(http_server()->GetURL("bar.com", "/simple.html"));

  // Navigate to `parent_url` and bypass the HTTPS-Only Mode warning.
  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, parent_url);

  if (IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    // Proceeding through the interstitial will add the hostname to the
    // allowlist.
    ProceedThroughInterstitial(contents);
  }

  // Verify that navigation event metrics were recorded for the main frame.
  histograms()->ExpectTotalCount(kEventHistogram, 3);

  // Navigate the iframe to `iframe_url`. It should successfully navigate and
  // not get upgraded to HTTPS as the hostname is now in the allowlist.
  content::TestNavigationObserver nav_observer(contents, 1);
  EXPECT_TRUE(content::NavigateIframeToURL(contents, "test", iframe_url));
  nav_observer.Wait();
  EXPECT_EQ(iframe_url, nav_observer.last_navigation_url());

  // Verify that no new navigation event metrics were recorded for the subframe.
  histograms()->ExpectTotalCount(kEventHistogram, 3);
}

// Tests that a navigation to the HTTP version of a site with an HTTPS version
// that is slow to respond gets upgraded to HTTPS but times out and shows the
// HTTPS-Only Mode interstitial.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, SlowHttps_ShouldInterstitial) {
  // Set timeout to zero so that HTTPS upgrades immediately timeout.
  HttpsUpgradesNavigationThrottle::set_timeout_for_testing(base::TimeDelta());

  // Set up a custom HTTPS server that times out without sending a response.
  net::EmbeddedTestServer timeout_server{net::EmbeddedTestServer::TYPE_HTTPS};
  timeout_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        // Server will hang until destroyed.
        return std::make_unique<net::test_server::HungResponse>();
      }));
  ASSERT_TRUE(timeout_server.Start());
  HttpsUpgradesInterceptor::SetHttpsPortForTesting(timeout_server.port());

  const GURL http_url = http_server()->GetURL("foo.com", "/simple.html");
  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, http_url);

  if (IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  }
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
}

// Tests that an HTTP POST form navigation to "bar.com" from an HTTP page on
// "foo.com" is not upgraded to HTTPS. (HTTP form navigations from HTTPS are
// blocked by the Mixed Forms warning.)
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, HttpPageHttpPost_NotUpgraded) {
  // Point the HTTP form target to "bar.com".
  base::StringPairs replacement_text;
  replacement_text.emplace_back(make_pair(
      "REPLACE_WITH_HOST_AND_PORT",
      net::HostPortPair::FromURL(http_server()->GetURL("foo.com", "/"))
          .ToString()));
  auto replacement_path = net::test_server::GetFilePathWithReplacements(
      "/ssl/page_with_form_targeting_http_url.html", replacement_text);

  // Navigate to the page hosting the form on "foo.com".
  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  content::NavigateToURLBlockUntilNavigationsComplete(
      contents, http_server()->GetURL("bad-https.com", replacement_path), 1);

  if (IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    // The HTTPS-Only Mode interstitial should trigger.
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    // Proceed through the interstitial to add the hostname to the allowlist.
    ProceedThroughInterstitial(contents);
  }

  // Verify that navigation event metrics were recorded for the initial page.
  histograms()->ExpectTotalCount(kEventHistogram, 3);

  // Submit the form and wait for the navigation to complete.
  content::TestNavigationObserver nav_observer(contents, 1);
  ASSERT_TRUE(
      content::ExecJs(contents, "document.getElementById('submit').click();"));
  nav_observer.Wait();

  // Check that the navigation has ended up on the HTTP target.
  EXPECT_EQ("foo.com", contents->GetLastCommittedURL().host());
  EXPECT_TRUE(contents->GetLastCommittedURL().SchemeIs(url::kHttpScheme));

  // Verify that no new navigation event metrics were recorded for the POST
  // navigation.
  histograms()->ExpectTotalCount(kEventHistogram, 3);
}

// Tests that if an HTTPS navigation redirects to HTTP on a different host, it
// should upgrade to HTTPS on that new host. (A downgrade redirect on the same
// host would imply a redirect loop.)
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       HttpsToHttpRedirect_ShouldUpgrade) {
  GURL target_url = http_server()->GetURL("bar.com", "/title1.html");
  GURL url = https_server()->GetURL("foo.com",
                                    "/server-redirect?" + target_url.spec());

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();

  // NavigateToURL() returns `false` because the final redirected URL does not
  // match `url`. Separately ensure the navigation succeeded using a navigation
  // observer.
  content::TestNavigationObserver nav_observer(contents, 1);
  EXPECT_FALSE(content::NavigateToURL(contents, url));
  nav_observer.Wait();
  EXPECT_TRUE(nav_observer.last_navigation_succeeded());

  // Verify that navigation event metrics were correctly recorded.
  EXPECT_TRUE(contents->GetLastCommittedURL().SchemeIs(url::kHttpsScheme));
  histograms()->ExpectTotalCount(kEventHistogram, 2);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeSucceeded, 1);

  EXPECT_EQ("bar.com", contents->GetLastCommittedURL().host());
}

// Regression test for crbug.com/41488861.
// Tests that a slow fallback load is not cancelled with timeout.
// bad-ssl.com is configured to return a slow load over http. Navigating to
// http://bad-ssl.com should upgrade and immediately fall back, then display the
// http response without cancelling it for timeout.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       CancelTimeoutForFallbackNavigations) {
  net::EmbeddedTestServer http_server;
  net::EmbeddedTestServer https_server{net::EmbeddedTestServer::TYPE_HTTPS};

  // Make the HTTP server return a slow response.
  http_server.RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        // The HTTP load needs to be slower than the 1 second timeout configured
        // by the the test.
        auto slow_http_response =
            std::make_unique<net::test_server::DelayedHttpResponse>(
                base::Seconds(2));
        slow_http_response->set_content_type("text/html");
        slow_http_response->set_content("hello from http");
        return std::move(slow_http_response);
      }));
  ASSERT_TRUE(http_server.Start());
  ASSERT_TRUE(https_server.Start());

  // Set the timeout short enough, but not zero. We can't set it to zero
  // because it'll cancel the HTTPS load with timeout instead of an error.
  HttpsUpgradesNavigationThrottle::set_timeout_for_testing(base::Seconds(1));

  HttpsUpgradesInterceptor::SetHttpPortForTesting(http_server.port());
  HttpsUpgradesInterceptor::SetHttpsPortForTesting(https_server.port());

  GURL http_url(http_server.GetURL("bad-https.com", "/"));
  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  if (IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    histograms()->ExpectTotalCount("Net.ErrorCodesForMainFrame4", 1);
    histograms()->ExpectBucketCount("Net.ErrorCodesForMainFrame4",
                                    -net::ERR_ABORTED, 1);
  } else {
    // Shouldn't record any net errors.
    EXPECT_FALSE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    histograms()->ExpectTotalCount("Net.ErrorCodesForMainFrame4", 0);
  }

  histograms()->ExpectTotalCount(kEventHistogram, 3);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeFailed, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeCertError, 1);
}

// Creates a redirect response.
std::unique_ptr<net::test_server::HttpResponse> RedirectResponseHandler(
    const GURL& dest_url,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Location", dest_url.spec());
  return std::move(http_response);
}

// Creates a response that causes a redirect loop over https and returns a slow
// response over http.
std::unique_ptr<net::test_server::HttpResponse> RedirectLoopResponse(
    int& http_port,
    int& https_port,
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path() == "/redirect") {
    // Over https, this URL redirects to itself.
    if (request.GetURL().SchemeIs("https")) {
      GURL url(base::StringPrintf("http://a.com:%d/redirect", http_port));
      return RedirectResponseHandler(url, request);
    }
    // Over http, it prints a slow hello. This should delay longer than the
    // HTTPS upgrade timeout which is set to 1 second.
    auto slow_http_response =
        std::make_unique<net::test_server::DelayedHttpResponse>(
            base::Seconds(2));
    slow_http_response->set_content_type("text/html");
    slow_http_response->set_content("hello from http");
    return std::move(slow_http_response);
  }
  return nullptr;
}

// Another regression test for crbug.com/41488861.
// Tests that a slow load that's detected as a redirect loop will not display
// a flash of a net error page.
//
// Assume that a.com/redirect:
// - Shows a slow loading response when loaded over http
// - Redirects to http://a.com/redirect when loaded over https.
//
// The flow of the test is as follows:
// 1. Load http://a.com/redirect.
// 2. http://a.com/redirect is upgraded to http.
// 3. https://a.com/redirect redirects to http://a.com/redirect
// 4. This triggers a redirect loop (the URL was seen at step 1).
// 5. a.com is allowlisted and http://a.com/redirect is loaded as fallback.
// 6. http://a.com/redirect prints a message after a slow load.
//
// This flow should never display a net error page with ERR_TIMED_OUT to the
// user. If the interstitial is enabled, it should be displayed at the final
// step.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       RedirectLoopWithSlowRedirect_ShouldInterstitial) {
  net::EmbeddedTestServer redirect_server_http;
  net::EmbeddedTestServer redirect_server_https{
      net::EmbeddedTestServer::TYPE_HTTPS};

  // Set the timeout short enough, but not zero. We can't set it to zero
  // because it'll cancel the HTTPS load with timeout instead of an error.
  // The HTTP load in this test needs to be slower than this timeout for the
  // test to be meaningful.
  HttpsUpgradesNavigationThrottle::set_timeout_for_testing(base::Seconds(1));

  // We don't know the ports without starting the servers and we can't start
  // the servers without registering request handlers. Pass them as refs so
  // that we can change them later.
  int http_port = 0;
  int https_port = 0;
  redirect_server_http.RegisterRequestHandler(base::BindRepeating(
      &RedirectLoopResponse, std::ref(http_port), std::ref(https_port)));
  redirect_server_https.RegisterRequestHandler(base::BindRepeating(
      &RedirectLoopResponse, std::ref(http_port), std::ref(https_port)));

  ASSERT_TRUE(redirect_server_http.Start());
  ASSERT_TRUE(redirect_server_https.Start());

  http_port = redirect_server_http.port();
  https_port = redirect_server_https.port();

  HttpsUpgradesInterceptor::SetHttpPortForTesting(redirect_server_http.port());
  HttpsUpgradesInterceptor::SetHttpsPortForTesting(
      redirect_server_https.port());

  GURL http_url(redirect_server_http.GetURL("a.com", "/redirect"));
  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  if (IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    histograms()->ExpectTotalCount("Net.ErrorCodesForMainFrame4", 1);
    histograms()->ExpectBucketCount("Net.ErrorCodesForMainFrame4",
                                    -net::ERR_ABORTED, 1);
  } else {
    // Shouldn't record any net errors.
    EXPECT_FALSE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    histograms()->ExpectTotalCount("Net.ErrorCodesForMainFrame4", 0);
  }

  histograms()->ExpectTotalCount(kEventHistogram, 3);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeFailed, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeRedirectLoop,
                                  1);
}

// Tests that navigating to an HTTPS page that downgrades to HTTP on the same
// host will fail and trigger the HTTPS-Only Mode interstitial (due to
// interceptor detecting a redirect loop and triggering fallback).
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       RedirectLoop_ShouldInterstitial) {
  // Set up a new test server instance so it can have a custom handler.
  net::EmbeddedTestServer downgrading_server{
      net::EmbeddedTestServer::TYPE_HTTPS};
  // Downgrade by swapping the scheme for HTTP. HTTPS-Only Mode will upgrade it
  // back to HTTPS.
  downgrading_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        GURL::Replacements http_downgrade;
        http_downgrade.SetSchemeStr(url::kHttpScheme);
        // The HttpRequest will by default refer to the test server by the
        // loopback address rather than any hostname in the navigation (i.e.,
        // the EmbeddedTestServer has no notion of virtual hosts). This
        // explicitly sets the hostname back to the test host so that this
        // doesn't fail due to the exception for localhost.
        http_downgrade.SetHostStr("foo.com");
        auto redirect_url = request.GetURL().ReplaceComponents(http_downgrade);
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(net::HTTP_TEMPORARY_REDIRECT);
        response->AddCustomHeader("Location", redirect_url.spec());
        return response;
      }));
  ASSERT_TRUE(downgrading_server.Start());
  HttpsUpgradesInterceptor::SetHttpPortForTesting(downgrading_server.port());
  HttpsUpgradesInterceptor::SetHttpsPortForTesting(downgrading_server.port());

  GURL url = downgrading_server.GetURL("foo.com", "/");
  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, url);

  if (IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  }

  // Verify that navigation event metrics were correctly recorded.
  histograms()->ExpectTotalCount(kEventHistogram, 3);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeFailed, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeRedirectLoop,
                                  1);
}

// Tests that the security level is WARNING when the HTTPS-Only Mode
// interstitial is shown for a net error on HTTPS. (Without HTTPS-Only Mode, a
// net error would be a security level of NONE.)
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       NetErrorOnUpgrade_SecurityLevelWarning) {
  GURL http_url = http_server()->GetURL("foo.com", "/close-socket");
  GURL https_url = https_server()->GetURL("foo.com", "/close-socket");

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  auto* helper = SecurityStateTabHelper::FromWebContents(contents);
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  if (IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));

    EXPECT_EQ(security_state::WARNING, helper->GetSecurityLevel());

    // Proceed through the interstitial to navigate to the HTTP site.
    ProceedThroughInterstitial(contents);
  }

  // The HTTP site results in a net error, which should have security level NONE
  // (as no connection was made).
  // TODO(crbug.com/40248833): Uncomment once upgrades are tracked
  // per-navigation.
  // EXPECT_EQ(security_state::NONE, helper->GetSecurityLevel());
}

// Tests that the security level is WARNING when the HTTPS-Only Mode
// interstitial is shown for a cert error on HTTPS. (Without HTTPS-Only Mode, a
// a cert error would be a security level of DANGEROUS.) After clicking through
// the interstitial, the security level should still be WARNING.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       BrokenSSLOnUpgrade_SecurityLevelWarning) {
  GURL http_url = http_server()->GetURL("bad-https.com", "/simple.html");
  GURL https_url = https_server()->GetURL("bad-https.com", "/simple.html");

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  auto* helper = SecurityStateTabHelper::FromWebContents(contents);
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  if (IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));

    EXPECT_EQ(security_state::WARNING, helper->GetSecurityLevel());

    // Proceed through the interstitial to navigate to the HTTP page.
    ProceedThroughInterstitial(contents);
  }

  // The security level should still be WARNING.
  EXPECT_EQ(security_state::WARNING, helper->GetSecurityLevel());
}

// Regression test for crbug.com/1233207.
// Tests the case where the HTTP version of a site redirects to HTTPS, but the
// HTTPS version of the site has a cert error. If the user initially navigates
// to the HTTP URL, then HTTPS-First Mode should upgrade the navigation to HTTPS
// and trigger the HTTPS-First Mode interstitial when that fails, but if the
// user clicks through the HTTPS-First Mode interstitial and falls back into the
// HTTP->HTTPS redirect back to the cert error, then the SSL interstitial should
// be shown and the user should be able to click through the SSL interstitial to
// visit the HTTPS version of the site (but in a DANGEROUS security level
// state).
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       HttpsUpgradeWithBrokenSSL_ShouldTriggerSSLInterstitial) {
  // Set up a new test server instance so it can have a custom handler that
  // redirects to the HTTPS server.
  net::EmbeddedTestServer upgrading_server{net::EmbeddedTestServer::TYPE_HTTP};
  upgrading_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(net::HTTP_TEMPORARY_REDIRECT);
        response->AddCustomHeader(
            "Location",
            "https://bad-https.com:" +
                base::NumberToString(
                    HttpsUpgradesInterceptor::GetHttpsPortForTesting()) +
                "/simple.html");
        return response;
      }));
  ASSERT_TRUE(upgrading_server.Start());
  HttpsUpgradesInterceptor::SetHttpPortForTesting(upgrading_server.port());

  GURL http_url = upgrading_server.GetURL("bad-https.com", "/simple.html");
  // HTTPS server will have a cert error.
  GURL https_url = https_server()->GetURL("bad-https.com", "/simple.html");

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, http_url);

  if (IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    // The HTTPS-First Mode interstitial should trigger first.
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));

    // Proceeding through the HTTPS-First Mode interstitial will hit the
    // upgrading server's HTTP->HTTPS redirect. This should result in an SSL
    // interstitial (not an HTTPS-First Mode interstitial).
    ProceedThroughInterstitial(contents);
  }

  EXPECT_EQ(https_url, contents->GetLastCommittedURL());
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(contents));

  // Proceeding through the SSL interstitial should navigate to the HTTPS
  // version of the site but with the DANGEROUS security level.
  ProceedThroughInterstitial(contents);
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());
  auto* helper = SecurityStateTabHelper::FromWebContents(contents);
  EXPECT_EQ(security_state::DANGEROUS, helper->GetSecurityLevel());

  // Verify that navigation event metrics were correctly recorded. They should
  // only have been recorded for the initial navigation that resulted in the
  // HTTPS-First Mode interstitial.
  histograms()->ExpectTotalCount(kEventHistogram, 3);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeFailed, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeCertError, 1);

  if (IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    // Verify that the interstitial metrics were correctly recorded.
    histograms()->ExpectBucketCount(
        "interstitial.https_first_mode.decision",
        security_interstitials::MetricsHelper::Decision::SHOW, 1);
    histograms()->ExpectBucketCount(
        "interstitial.https_first_mode.decision",
        security_interstitials::MetricsHelper::Decision::PROCEED, 1);
  }
}

// Tests that clicking the "Learn More" link in the HTTPS-First Mode
// interstitial opens a new tab for the help center article.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, InterstitialLearnMoreLink) {
  // This test is only relevant to HTTPS-First Mode.
  if (!IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    return;
  }

  GURL http_url = http_server()->GetURL("foo.com", "/close-socket");
  GURL https_url = https_server()->GetURL("foo.com", "/close-socket");

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));

  // Simulate clicking the learn more link (CMD_OPEN_HELP_CENTER).
  ASSERT_TRUE(content::ExecJs(
      contents, "window.certificateErrorPageController.openHelpCenter();"));

  // New tab should include the p-link "first_mode".
  EXPECT_EQ(GetBrowser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetVisibleURL()
                .query(),
            "p=first_mode");

  // Verify that the interstitial metrics were correctly recorded.
  histograms()->ExpectBucketCount(
      "interstitial.https_first_mode.decision",
      security_interstitials::MetricsHelper::Decision::SHOW, 1);
  histograms()->ExpectBucketCount(
      "interstitial.https_first_mode.interaction",
      security_interstitials::MetricsHelper::Interaction::TOTAL_VISITS, 1);
  histograms()->ExpectBucketCount(
      "interstitial.https_first_mode.interaction",
      security_interstitials::MetricsHelper::Interaction::SHOW_LEARN_MORE, 1);
}

// Tests that if the user bypasses the HTTPS-First Mode interstitial, and then
// later the server fixes their HTTPS support and the user successfully connects
// over HTTPS, the allowlist entry is cleared (so HFM will kick in again for
// that site).
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, BadHttpsFollowedByGoodHttps) {
  // TODO(crbug.com/40248833): This test is flakey when only HTTPS Upgrades are
  // enabled.
  if (!IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    return;
  }

  GURL http_url = http_server()->GetURL("foo.com", "/close-socket");
  GURL bad_https_url = https_server()->GetURL("foo.com", "/close-socket");
  GURL good_https_url = https_server()->GetURL("foo.com", "/ssl/google.html");

  ASSERT_EQ(http_url.host(), bad_https_url.host());
  ASSERT_EQ(bad_https_url.host(), good_https_url.host());

  auto* tab = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  auto* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  auto* state = static_cast<StatefulSSLHostStateDelegate*>(
      profile->GetSSLHostStateDelegate());

  // First check that main frame requests revoke the decision.

  // Navigate to `http_url`, which will get upgraded to `bad_https_url`.
  NavigateAndWaitForFallback(tab, http_url);

  if (IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    ASSERT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(tab));
    ProceedThroughInterstitial(tab);
  }

  EXPECT_TRUE(state->HasAllowException(
      http_url.host(), tab->GetPrimaryMainFrame()->GetStoragePartition()));

  EXPECT_TRUE(content::NavigateToURL(tab, good_https_url));
  EXPECT_FALSE(state->HasAllowException(
      http_url.host(), tab->GetPrimaryMainFrame()->GetStoragePartition()));

  // Rarely, an open connection with the bad cert might be reused for the next
  // navigation, which is supposed to show an interstitial. Close open
  // connections to ensure a fresh connection (and certificate validation) for
  // the next navigation. See https://crbug.com/1150592. A deeper fix for this
  // issue would be to unify certificate bypass logic which is currently split
  // between the net stack and content layer; see https://crbug.com/488043.
  // See also: SSLUITest.BadCertFollowedByGoodCert.
  state->RevokeUserAllowExceptionsHard(http_url.host());

  // Now check that subresource requests revoke the decision.

  // Navigate to `http_url`, which will get upgraded to `bad_https_url`.
  NavigateAndWaitForFallback(tab, http_url);

  if (IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    ASSERT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(tab));
    ProceedThroughInterstitial(tab);
  }

  EXPECT_TRUE(state->HasAllowException(
      http_url.host(), tab->GetPrimaryMainFrame()->GetStoragePartition()));

  // Load "logo.gif" as an image on the page.
  GURL image = https_server()->GetURL("foo.com", "/ssl/google_files/logo.gif");

  EXPECT_EQ(
      true,
      EvalJs(tab,
             std::string("var img = document.createElement('img');img.src ='") +
                 image.spec() +
                 "';"
                 "new Promise(resolve => {"
                 "  img.onload=function() { "
                 "    resolve(true); };"
                 "  document.body.appendChild(img);"
                 "});"));

  EXPECT_FALSE(state->HasAllowException(
      http_url.host(), tab->GetPrimaryMainFrame()->GetStoragePartition()));
}

// Tests that clicking the "Go back" button in the HTTPS-First Mode interstitial
// navigates back to the previous page (about:blank in this case).
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, InterstitialGoBack) {
  // This test is only relevant to HTTPS-First Mode.
  if (!IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    return;
  }

  GURL http_url = http_server()->GetURL("foo.com", "/close-socket");
  GURL https_url = https_server()->GetURL("foo.com", "/close-socket");

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));

  // Simulate clicking the "Go back" button.
  DontProceedThroughInterstitial(contents);

  EXPECT_EQ(GURL("about:blank"), contents->GetLastCommittedURL());

  // Verify that the interstitial metrics were correctly recorded.
  histograms()->ExpectBucketCount(
      "interstitial.https_first_mode.decision",
      security_interstitials::MetricsHelper::Decision::SHOW, 1);
  histograms()->ExpectBucketCount(
      "interstitial.https_first_mode.decision",
      security_interstitials::MetricsHelper::Decision::DONT_PROCEED, 1);
}

// Tests that closing the tab of the HTTPS-First Mode interstitial counts as
// not proceeding through the interstitial for metrics.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, CloseInterstitialTab) {
  // This test is only relevant to HTTPS-First Mode.
  if (!IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    return;
  }

  GURL http_url = http_server()->GetURL("foo.com", "/close-socket");
  GURL https_url = https_server()->GetURL("foo.com", "/close-socket");

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));

  // Leave the interstitial by closing the tab.
  chrome::CloseWebContents(GetBrowser(), contents, false);

  // Verify that the interstitial metrics were correctly recorded.
  histograms()->ExpectBucketCount(
      "interstitial.https_first_mode.decision",
      security_interstitials::MetricsHelper::Decision::SHOW, 1);
  histograms()->ExpectBucketCount(
      "interstitial.https_first_mode.decision",
      security_interstitials::MetricsHelper::Decision::DONT_PROCEED, 1);
}

// Tests that if a user allowlists a host and then does not visit it again for
// seven days (the expiration period), then the interstitial will be shown again
// the next time they visit the host.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, AllowlistEntryExpires) {
  content::WebContents* contents =
      GetBrowser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();

  // Set a testing clock on the StatefulSSLHostStateDelegate, keeping a pointer
  // to the clock object around so the test can manipulate time. `chrome_state`
  // takes ownership of `clock`.
  auto clock = std::make_unique<base::SimpleTestClock>();
  auto* clock_ptr = clock.get();
  StatefulSSLHostStateDelegate* chrome_state =
      static_cast<StatefulSSLHostStateDelegate*>(state);
  chrome_state->SetClockForTesting(std::move(clock));

  // Start the clock at standard system time.
  clock_ptr->SetNow(base::Time::NowFromSystemTime());

  // Visit a host that doesn't support HTTPS for the first time, and click
  // through the HTTPS-First Mode interstitial to allowlist the host.
  GURL http_url = http_server()->GetURL("bad-https.com", "/simple.html");
  NavigateAndWaitForFallback(contents, http_url);

  if (IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    ProceedThroughInterstitial(contents);
  }

  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_TRUE(state->IsHttpAllowedForHost(
      http_url.host(), contents->GetPrimaryMainFrame()->GetStoragePartition()));

  // Simulate the clock advancing by 16 days, which is past the expiration
  // point.
  clock_ptr->Advance(base::Days(16));

  // The host should no longer be allowlisted, and the interstitial should
  // trigger again.
  EXPECT_FALSE(state->IsHttpAllowedForHost(
      http_url.host(), contents->GetPrimaryMainFrame()->GetStoragePartition()));
  NavigateAndWaitForFallback(contents, http_url);

  if (IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  }
}

// Tests that re-visiting an allowlisted host bumps the expiration time to a new
// seven days in the future from now.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, RevisitingBumpsExpiration) {
  content::WebContents* contents =
      GetBrowser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();

  // Set a testing clock on the StatefulSSLHostStateDelegate, keeping a pointer
  // to the clock object around so the test can manipulate time. `chrome_state`
  // takes ownership of `clock`.
  auto clock = std::make_unique<base::SimpleTestClock>();
  auto* clock_ptr = clock.get();
  StatefulSSLHostStateDelegate* chrome_state =
      static_cast<StatefulSSLHostStateDelegate*>(state);
  chrome_state->SetClockForTesting(std::move(clock));

  // Start the clock at standard system time.
  clock_ptr->SetNow(base::Time::NowFromSystemTime());

  // Visit a host that doesn't support HTTPS for the first time, and click
  // through the HTTPS-First Mode interstitial to allowlist the host.
  GURL http_url = http_server()->GetURL("bad-https.com", "/simple.html");
  NavigateAndWaitForFallback(contents, http_url);

  if (IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    ProceedThroughInterstitial(contents);
  }

  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  EXPECT_TRUE(state->IsHttpAllowedForHost(
      http_url.host(), contents->GetPrimaryMainFrame()->GetStoragePartition()));

  // Simulate the clock advancing by ten days.
  clock_ptr->Advance(base::Days(10));

  // Navigate to the host again; this will reset the allowlist expiration to
  // now + 7 days.
  EXPECT_TRUE(content::NavigateToURL(contents, http_url));

  // Simulate the clock advancing another ten days. This will be _after_ the
  // initial expiration date of the allowlist entry, but _before_ the bumped
  // expiration date from the second navigation.
  clock_ptr->Advance(base::Days(10));
  EXPECT_TRUE(state->IsHttpAllowedForHost(
      http_url.host(), contents->GetPrimaryMainFrame()->GetStoragePartition()));
  EXPECT_TRUE(content::NavigateToURL(contents, http_url));
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));
}

// Tests that if a hostname has an HSTS entry registered, then HTTPS-First Mode
// should not try to upgrade it (instead allowing HSTS to handle the upgrade as
// it is more strict).
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, PreferHstsOverHttpsFirstMode) {
  content::WebContents* contents =
      GetBrowser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());

  // URL for HTTPS server that will result in a certificate error.
  GURL https_url = https_server()->GetURL("bad-https.com", "/simple.html");

  // HTTP version of that URL that will get upgraded to HTTPS (but with the
  // correct port for the HTTPS server -- the test code can configure
  // HTTPS-First Mode to be aware of the different ports, but can't do that for
  // HSTS).
  GURL::Replacements downgrade_scheme_to_http;
  downgrade_scheme_to_http.SetSchemeStr(url::kHttpScheme);
  GURL http_url = https_url.ReplaceComponents(downgrade_scheme_to_http);

  // Add hostname to the TransportSecurityState.
  base::Time expiry = base::Time::Now() + base::Days(100);
  bool include_subdomains = false;
  auto* network_context =
      profile->GetDefaultStoragePartition()->GetNetworkContext();
  base::RunLoop run_loop;
  network_context->AddHSTS(http_url.host(), expiry, include_subdomains,
                           run_loop.QuitClosure());
  run_loop.Run();

  // Navigate to the HTTP URL. It should get upgraded to HTTPS and trigger a
  // fatal certificate error (because of HTTPS) instead of falling back to the
  // HTTPS-First Mode interstitial.
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(contents));

  // Verify that no HFM event histograms were emitted (to check that HFM did not
  // trigger for this navigation at all).
  histograms()->ExpectTotalCount(kEventHistogram, 0);

  // Verify that general navigation request metrics were recorded.
  histograms()->ExpectTotalCount(kNavigationRequestSecurityLevelHistogram, 2);
  histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                  NavigationRequestSecurityLevel::kHstsUpgraded,
                                  1);
  histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                  NavigationRequestSecurityLevel::kSecure, 1);
}

// Regression test for crbug.com/1272781. Previously, performing back/forward
// navigations around the HTTPS-First Mode interstitial could cause history
// entries to dropped.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       InterstitialFallbackMaintainsHistory) {
  // This test only applies to HTTPS-First Mode.
  if (!IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    return;
  }

  GURL good_https_url = https_server()->GetURL("site1.com", "/defaultresponse");

  // Set up a new test server instance so it can have a custom handler.
  net::EmbeddedTestServer downgrading_server{
      net::EmbeddedTestServer::TYPE_HTTPS};
  // Downgrade by swapping the scheme for HTTP. HTTPS-First Mode will upgrade it
  // back to HTTPS.
  downgrading_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        GURL::Replacements http_downgrade;
        http_downgrade.SetSchemeStr(url::kHttpScheme);
        // The HttpRequest will by default refer to the test server by the
        // loopback address rather than any hostname in the navigation (i.e.,
        // the EmbeddedTestServer has no notion of virtual hosts). This
        // explicitly sets the hostname back to the test host so that this
        // doesn't fail due to the exception for localhost.
        http_downgrade.SetHostStr("site2.com");
        auto redirect_url = request.GetURL().ReplaceComponents(http_downgrade);
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(net::HTTP_TEMPORARY_REDIRECT);
        response->AddCustomHeader("Location", redirect_url.spec());
        return response;
      }));
  ASSERT_TRUE(downgrading_server.Start());
  HttpsUpgradesInterceptor::SetHttpPortForTesting(downgrading_server.port());
  HttpsUpgradesInterceptor::SetHttpsPortForTesting(downgrading_server.port());

  GURL downgrading_https_url = downgrading_server.GetURL("site2.com", "/");
  GURL::Replacements swap_http_scheme;
  swap_http_scheme.SetSchemeStr(url::kHttpScheme);
  GURL downgrading_http_url =
      downgrading_https_url.ReplaceComponents(swap_http_scheme);

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to a "good" HTTPS site.
  EXPECT_TRUE(content::NavigateToURL(contents, good_https_url));

  // Navigate to the HTTP version of `downgrading_https_url`, which will get
  // upgraded to HTTPS and fail, triggering the HTTPS-First Mode
  // interstitial.
  content::NavigateToURLBlockUntilNavigationsComplete(contents,
                                                      downgrading_http_url, 1);
  EXPECT_EQ(downgrading_http_url, contents->GetLastCommittedURL());
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));

  // Simulate clicking the browser "back" button.
  EXPECT_TRUE(content::HistoryGoBack(contents));
  EXPECT_EQ(good_https_url, contents->GetLastCommittedURL());
  auto* helper = SecurityStateTabHelper::FromWebContents(contents);
  EXPECT_EQ(security_state::SECURE, helper->GetSecurityLevel());

  // Simulate clicking the browser "forward" button. The HistoryGoForward()
  // call returns `false` because it is an error page.
  EXPECT_FALSE(content::HistoryGoForward(contents));
  EXPECT_EQ(downgrading_http_url, contents->GetLastCommittedURL());
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));

  // No forward entry should be present.
  EXPECT_FALSE(contents->GetController().CanGoForward());

  // Simulate clicking the browser "back" button again. Previously this would
  // result in `about:blank` being shown.
  EXPECT_TRUE(content::HistoryGoBack(contents));
  EXPECT_EQ(good_https_url, contents->GetLastCommittedURL());

  // Repeat forward one last time. (Previously the user would no longer be able
  // to go back any more as the history entries were lost.)
  EXPECT_FALSE(content::HistoryGoForward(contents));  // error page -> false
  EXPECT_EQ(downgrading_http_url, contents->GetLastCommittedURL());
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));
  EXPECT_TRUE(contents->GetController().CanGoBack());
}

// Tests that if the HttpAllowlist enterprise policy is set, then HTTPS upgrades
// are skipped for hosts in the allowlist. Includes simple hostname, wildcard
// hostname pattern, and bare IP address cases.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       EnterpriseAllowlistDisablesUpgrades) {
  content::WebContents* contents =
      GetBrowser()->tab_strip_model()->GetActiveWebContents();

  // Without any policy allowlist, navigate to HTTP URL on foo.com. It *should*
  // get upgraded to HTTPS.
  auto http_url = http_server()->GetURL("foo.com", "/simple.html");
  auto https_url = https_server()->GetURL("foo.com", "/simple.html");
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());

  // Artificially add the pref that gets mapped from the enterprise policy.
  auto* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  auto* prefs = profile->GetPrefs();
  base::Value::List allowlist;
  allowlist.Append("foo.com");
  allowlist.Append("[*.]bar.com");
  allowlist.Append(http_server()->GetIPLiteralString());
  // These cases should not work, but the policy->pref mapping won't immediately
  // reject them.
  allowlist.Append("[*]");
  allowlist.Append("*");
  prefs->SetList(prefs::kHttpAllowlist, std::move(allowlist));

  // Navigate to HTTP URL on foo.com. It should not get upgraded to HTTPS and
  // no interstitial should be shown.
  http_url = http_server()->GetURL("foo.com", "/simple.html");
  https_url = https_server()->GetURL("foo.com", "/simple.html");
  EXPECT_TRUE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));

  // Navigate to HTTP URL on bar.com. Same result.
  http_url = http_server()->GetURL("bar.com", "/simple.html");
  https_url = https_server()->GetURL("bar.com", "/simple.html");
  EXPECT_TRUE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));

  // Navigate to HTTP URL on bar.bar.com. Same result as subdomain wildcard
  // was specified.
  http_url = http_server()->GetURL("bar.bar.com", "/simple.html");
  https_url = https_server()->GetURL("bar.bar.com", "/simple.html");
  EXPECT_TRUE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));

  // Navigate to HTTP URL on foo.foo.com. Subdomains of foo.com should not be
  // considered as being in the allowlist as no wildcard was specified. This
  // should get upgraded to HTTPS.
  http_url = http_server()->GetURL("foo.foo.com", "/simple.html");
  https_url = https_server()->GetURL("foo.foo.com", "/simple.html");
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());

  // Navigate to HTTP URL on baz.com, which is not on the allowlist. Should get
  // upgraded to HTTPS.
  http_url = http_server()->GetURL("baz.com", "/simple.html");
  https_url = https_server()->GetURL("baz.com", "/simple.html");
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());

  // Navigate to HTTP URL on the HTTP test server's IP address. It should not
  // get upgraded to HTTPS and no interstitial should be shown.
  http_url = http_server()->GetURL("/simple.html");
  https_url = https_server()->GetURL("/simple.html");
  EXPECT_TRUE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));
}

// Tests that if the HttpAllowlist enterprise policy is set, then HTTPS upgrades
// and HTTPS-First Mode For Site Engagement checks are skipped for hosts in the
// allowlist.
IN_PROC_BROWSER_TEST_P(
    HttpsUpgradesBrowserTest,
    EnterpriseAllowlistDisablesHttpsFirstModeForSiteEngagament) {
  // Skip this test when HTTPS-First Mode for Site Engagement isn't enabled.
  if (!IsSiteEngagementHeuristicEnabled()) {
    return;
  }
  // Disable the testing port configuration, as this test doesn't use the
  // EmbeddedTestServer.
  HttpsUpgradesInterceptor::SetHttpsPortForTesting(0);
  HttpsUpgradesInterceptor::SetHttpPortForTesting(0);
  auto url_loader_interceptor = MakeInterceptorForSiteEngagementHeuristic();

  content::WebContents* contents =
      GetBrowser()->tab_strip_model()->GetActiveWebContents();
  auto* profile = Profile::FromBrowserContext(contents->GetBrowserContext());

  // Without any policy allowlist, navigate to an HTTP URL. It should show the
  // HFM+SE interstitial.
  GURL http_url("http://bad-https.com");
  GURL https_url("https://bad-https.com");
  SetSiteEngagementScore(http_url, kLowSiteEngagementScore);
  SetSiteEngagementScore(https_url, kHighSiteEnagementScore);

  HttpsFirstModeService* hfm_service =
      HttpsFirstModeServiceFactory::GetForProfile(profile);
  MaybeEnableHttpsFirstModeForEngagedSitesAndWait(hfm_service);

  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));

  // Artificially add the pref that gets mapped from the enterprise policy.
  auto* prefs = profile->GetPrefs();
  base::Value::List allowlist;
  allowlist.Append("bad-https.com");
  prefs->SetList(prefs::kHttpAllowlist, std::move(allowlist));

  // Navigate to the same URL. It should not get upgraded to HTTPS and
  // no interstitial should be shown.
  EXPECT_TRUE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));
}

IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       EnterprisePolicyDisablesUpgrades) {
  // Disable HTTPS-Upgrades via enterprise policy.
  auto* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kHttpsUpgradesEnabled, false);

  content::WebContents* contents =
      GetBrowser()->tab_strip_model()->GetActiveWebContents();
  GURL http_url = http_server()->GetURL("foo.com", "/simple.html");
  GURL https_url = https_server()->GetURL("foo.com", "/simple.html");

  if (IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    // HTTPS-First Mode should supercede HTTPS-Upgrades and upgrade the
    // navigation despite the HttpsUpgradeMode policy setting.
    EXPECT_FALSE(content::NavigateToURL(contents, http_url));
    EXPECT_EQ(https_url, contents->GetLastCommittedURL());
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kUpgraded,
                                    1);
  } else {
    // If HTTPS-First Mode is not enabled but upgrading is, then the policy
    // should prevent the upgrade.
    EXPECT_TRUE(content::NavigateToURL(contents, http_url));
    EXPECT_EQ(http_url, contents->GetLastCommittedURL());
    histograms()->ExpectBucketCount(
        kNavigationRequestSecurityLevelHistogram,
        NavigationRequestSecurityLevel::kAllowlisted, 1);
  }
}

// Test that HTTPS Upgrades are skipped if the "Insecure content" site setting
// is set to "allow".
// MIXED_SCRIPT isn't enabled as a content setting on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_InsecureContentSettingDisablesUpgrades \
  DISABLED_InsecureContentSettingDisablesUpgrades
#else
#define MAYBE_InsecureContentSettingDisablesUpgrades \
  InsecureContentSettingDisablesUpgrades
#endif
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       MAYBE_InsecureContentSettingDisablesUpgrades) {
  content::WebContents* contents =
      GetBrowser()->tab_strip_model()->GetActiveWebContents();
  GURL http_url = http_server()->GetURL("foo.com", "/simple.html");
  GURL https_url = https_server()->GetURL("foo.com", "/simple.html");
  auto* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  // Set insecure content setting to allowed for `http_url`.
  host_content_settings_map->SetContentSettingDefaultScope(
      http_url, GURL(), ContentSettingsType::MIXEDSCRIPT,
      CONTENT_SETTING_ALLOW);

  if (IsHttpsFirstModePrefEnabled()) {
    // If HTTPS-First Mode is enabled, upgrades should still be applied.
    EXPECT_FALSE(content::NavigateToURL(contents, http_url));
    EXPECT_EQ(https_url, contents->GetLastCommittedURL());
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kUpgraded,
                                    1);
  } else {
    // Otherwise, the upgrades should be skipped.
    EXPECT_TRUE(content::NavigateToURL(contents, http_url));
    EXPECT_EQ(http_url, contents->GetLastCommittedURL());
    histograms()->ExpectBucketCount(
        kNavigationRequestSecurityLevelHistogram,
        NavigationRequestSecurityLevel::kAllowlisted, 1);
  }

  // Unset the content settings.
  host_content_settings_map->ClearSettingsForOneType(
      ContentSettingsType::MIXEDSCRIPT);

  // Set insecure content setting to allowed for `https_url`.
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetContentSettingDefaultScope(https_url, GURL(),
                                      ContentSettingsType::MIXEDSCRIPT,
                                      CONTENT_SETTING_ALLOW);
  if (IsHttpsFirstModePrefEnabled()) {
    // If HTTPS-First Mode is enabled, upgrades should still be applied.
    EXPECT_FALSE(content::NavigateToURL(contents, http_url));
    EXPECT_EQ(https_url, contents->GetLastCommittedURL());
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kUpgraded,
                                    2);
  } else {
    // Otherwise, the upgrades should be skipped.
    EXPECT_TRUE(content::NavigateToURL(contents, http_url));
    EXPECT_EQ(http_url, contents->GetLastCommittedURL());
    histograms()->ExpectBucketCount(
        kNavigationRequestSecurityLevelHistogram,
        NavigationRequestSecurityLevel::kAllowlisted, 2);
  }
}

// Test that HTTPS Upgrades are skipped if the "Insecure content" site setting
// is set to "allow".
// MIXED_SCRIPT isn't enabled as a content setting on Android.
// This test is identical to InsecureContentSettingDisablesUpgrades except it
// sets a high site engagement score for the https URL and checks an additional
// histogram.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_InsecureContentSettingDisablesHFMForEngagedSites \
  DISABLED_InsecureContentSettingDisablesHFMForEngagedSites
#else
#define MAYBE_InsecureContentSettingDisablesHFMForEngagedSites \
  InsecureContentSettingDisablesHFMForEngagedSites
#endif
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       MAYBE_InsecureContentSettingDisablesHFMForEngagedSites) {
  content::WebContents* contents =
      GetBrowser()->tab_strip_model()->GetActiveWebContents();
  GURL http_url = http_server()->GetURL("foo.com", "/simple.html");
  GURL https_url = https_server()->GetURL("foo.com", "/simple.html");
  auto* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  // Setting a high engagement score on the HTTPS URL enables HFM on the site
  // if the HFM+SE feature is enabled, but an Insecure Content entry disables
  // HFM+SE on the site.
  SetSiteEngagementScore(http_url, kLowSiteEngagementScore);
  SetSiteEngagementScore(https_url, kHighSiteEnagementScore);

  // Set insecure content setting to allowed for `http_url`.
  host_content_settings_map->SetContentSettingDefaultScope(
      http_url, GURL(), ContentSettingsType::MIXEDSCRIPT,
      CONTENT_SETTING_ALLOW);

  if (IsHttpsFirstModePrefEnabled()) {
    // If HTTPS-First Mode is fully enabled, upgrades should still be applied.
    EXPECT_FALSE(content::NavigateToURL(contents, http_url));
    EXPECT_EQ(https_url, contents->GetLastCommittedURL());
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kUpgraded,
                                    1);
  } else {
    // Otherwise, the upgrades should be skipped.
    EXPECT_TRUE(content::NavigateToURL(contents, http_url));
    EXPECT_EQ(http_url, contents->GetLastCommittedURL());
    histograms()->ExpectBucketCount(
        kNavigationRequestSecurityLevelHistogram,
        NavigationRequestSecurityLevel::kAllowlisted, 1);
  }
  // In both cases, HFM+SE events shouldn't be recorded because of the Insecure
  // content setting.
  histograms()->ExpectTotalCount(kEventHistogramWithEngagementHeuristic, 0);

  // Unset the content settings.
  host_content_settings_map->ClearSettingsForOneType(
      ContentSettingsType::MIXEDSCRIPT);

  // Set insecure content setting to allowed for `https_url`.
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetContentSettingDefaultScope(https_url, GURL(),
                                      ContentSettingsType::MIXEDSCRIPT,
                                      CONTENT_SETTING_ALLOW);
  if (IsHttpsFirstModePrefEnabled()) {
    // If HTTPS-First Mode is enabled, upgrades should still be applied.
    EXPECT_FALSE(content::NavigateToURL(contents, http_url));
    EXPECT_EQ(https_url, contents->GetLastCommittedURL());
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kUpgraded,
                                    2);
  } else {
    // Otherwise, the upgrades should be skipped.
    EXPECT_TRUE(content::NavigateToURL(contents, http_url));
    EXPECT_EQ(http_url, contents->GetLastCommittedURL());
    histograms()->ExpectBucketCount(
        kNavigationRequestSecurityLevelHistogram,
        NavigationRequestSecurityLevel::kAllowlisted, 2);
  }
  // In both cases, HFM+SE events shouldn't be recorded because of the Insecure
  // content setting.
  histograms()->ExpectTotalCount(kEventHistogramWithEngagementHeuristic, 0);
}

// Regression test for crbug.com/1431026. Triggers a navigation where HTTPS
// upgrades applied multiple times across redirects to different sites.
// Should not crash when DCHECKS are enabled.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, crbug1431026) {
  GURL www_bad_https_url =
      https_server()->GetURL("www.bad-https.com", "/simple.html");
  GURL www_http_url =
      http_server()->GetURL("www.bad-https.com", "/simple.html");

  // Configure HTTP and bad-HTTPS URLs which redirect to www. subdomain.
  std::string www_redirect_path =
      base::StrCat({"/server-redirect?", www_http_url.spec()});
  GURL redirecting_bad_https_url =
      https_server()->GetURL("bad-https.com", www_redirect_path);
  GURL redirecting_http_url =
      http_server()->GetURL("bad-https.com", www_redirect_path);

  // A good HTTPS URL which redirects to an HTTP URL, which also redirects.
  GURL initial_redirecting_good_https_url = https_server()->GetURL(
      "good-https.com",
      base::StrCat({"/server-redirect-301?", redirecting_http_url.spec()}));

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(
      content::NavigateToURL(contents, initial_redirecting_good_https_url));

  if (IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    // Should be showing interstitial on http://bad-https.com/.
    EXPECT_EQ(redirecting_http_url, contents->GetLastCommittedURL());
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  } else {
    // Either due to no upgrades, or due to fast fallback to HTTP, this should
    // end up on http://www.bad-https.com.
    EXPECT_EQ(www_http_url, contents->GetLastCommittedURL());
    EXPECT_FALSE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  }
}

// Tests that when the HTTPS-First Mode setting is toggled on or off, the
// HTTP allowlist is cleared.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       TogglingSettingClearsAllowlist) {
  // The allowlist in an Incognito window is in-memory only, and is not cleared
  // when the main profile's pref changes.
  // TODO(crbug.com/40937027): Add a test to cover the Incognito allowlisting
  // behavior explicitly.
  if (IsIncognito()) {
    return;
  }

  auto http_url = http_server()->GetURL("bad-https.com", "/simple.html");
  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();

  // Start by enabling HTTPS-First Mode.
  SetPref(true);

  // Navigate to a URL that will fail upgrades, and click through the
  // interstitial to add it to the allowlist.
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));
  ProceedThroughInterstitial(contents);

  // Disable the HTTPS-First Mode pref. This should clear the allowlist.
  SetPref(false);

  if (InBalancedMode()) {
    EXPECT_FALSE(content::NavigateToURL(contents, http_url));
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));

    // Proceed through the interstitial and add the host to the allowlist.
    ProceedThroughInterstitial(contents);
  } else {
    // With HTTPS-Upgrades enabled, navigating again should cause the site to
    // get added back to the allowlist.
    EXPECT_TRUE(content::NavigateToURL(contents, http_url));
    EXPECT_FALSE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  }

  // Re-enable the HTTPS-First Mode pref. The allowlist should be cleared again.
  SetPref(true);

  // Navigate to a URL that will fail upgrades, and the interstitial should be
  // shown again as the allowlist was cleared.
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));
}

// Main window HTTP allowlist should not apply to Incognito window.
// Regression test for crbug.com/40949400.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       IncognitoHasSeparateAllowlist) {
  // This test only covers the case of HFM-in-Incognito.
  if (!IsIncognito()) {
    return;
  }
  // In a regular window, add a host to the HTTP allowlist.
  // Note: This is explicitly done with HTTPS-First Mode disabled as that is the
  // specific regression case for crbug.com/40949400, but HTTPS-First Mode and
  // HTTPS-Upgrades may eventually have separate allowlists.
  SetPref(false);
  auto http_url = http_server()->GetURL("bad-https.com", "/simple.html");
  auto* normal_tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::NavigateToURL(normal_tab, http_url));
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          normal_tab));

  // In an Incognito window, navigating to that same host should still trigger
  // the HTTP interstitial, as the allowlist is not inherited.
  auto* incognito_tab = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(incognito_tab, http_url));
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      incognito_tab));
}

// Tests that URLs typed with an explicit http:// scheme are opted out from
// upgrades.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       URLsTypedWithHttpSchemeNoUpgrades) {
  GURL http_url = http_server()->GetURL("foo.com", "/simple.html");
  GURL https_url = https_server()->GetURL("foo.com", "/simple.html");
  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  OmniboxClient* omnibox_client = GetBrowser()
                                      ->window()
                                      ->GetLocationBar()
                                      ->GetOmniboxView()
                                      ->controller()
                                      ->client();

  // Simulate the full URL was typed with an http scheme.
  content::TestNavigationObserver nav_observer(contents, 1);
  omnibox_client->OnAutocompleteAccept(
      http_url, nullptr, WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::URL_WHAT_YOU_TYPED,
      base::TimeTicks(), false, true, std::u16string(), AutocompleteMatch(),
      AutocompleteMatch(), IDNA2008DeviationCharacter::kNone);
  nav_observer.Wait();

  if (IsHttpsFirstModePrefEnabled() || IsIncognito()) {
    // Typed http URLs don't opt out of upgrades in HFM.
    EXPECT_EQ(https_url, contents->GetLastCommittedURL());
  } else {
    histograms()->ExpectTotalCount(kNavigationRequestSecurityLevelHistogram, 1);
    histograms()->ExpectBucketCount(
        kNavigationRequestSecurityLevelHistogram,
        NavigationRequestSecurityLevel::kExplicitHttpScheme, 1);
    EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  }
}

// Tests that URLs with an explicit http:// scheme are upgraded if they were
// autocompleted.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       URLsAutocompletedWithHttpSchemeAreUpgraded) {
  GURL http_url = http_server()->GetURL("foo.com", "/simple.html");
  GURL https_url = https_server()->GetURL("foo.com", "/simple.html");
  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  OmniboxClient* omnibox_client = GetBrowser()
                                      ->window()
                                      ->GetLocationBar()
                                      ->GetOmniboxView()
                                      ->controller()
                                      ->client();

  // Simulate the full URL was autocompleted with an http scheme.
  content::TestNavigationObserver nav_observer(contents, 1);
  omnibox_client->OnAutocompleteAccept(
      http_url, nullptr, WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::NAVSUGGEST,
      base::TimeTicks(), false, false, std::u16string(), AutocompleteMatch(),
      AutocompleteMatch(), IDNA2008DeviationCharacter::kNone);
  nav_observer.Wait();

  EXPECT_EQ(https_url, contents->GetLastCommittedURL());
}

// Tests that URLs typed with an explicit http:// scheme that result in an
// opt-out cause the url to be added to the allowlist.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       URLsTypedWithHttpSchemeNoUpgradesAllowlist) {
  if (IsHttpsFirstModeInterstitialEnabledAcrossSites()) {
    return;
  }
  GURL http_url = http_server()->GetURL("foo.com", "/simple.html");
  GURL https_url = https_server()->GetURL("foo.com", "/simple.html");
  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  OmniboxClient* omnibox_client = GetBrowser()
                                      ->window()
                                      ->GetLocationBar()
                                      ->GetOmniboxView()
                                      ->controller()
                                      ->client();

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();

  // Site should not yet be in the allowlist.
  EXPECT_FALSE(state->IsHttpAllowedForHost(
      http_url.host(), contents->GetPrimaryMainFrame()->GetStoragePartition()));

  // Simulate the full URL was typed with an http scheme.
  content::TestNavigationObserver nav_observer(contents, 1);
  omnibox_client->OnAutocompleteAccept(
      http_url, nullptr, WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::URL_WHAT_YOU_TYPED,
      base::TimeTicks(), false, true, std::u16string(), AutocompleteMatch(),
      AutocompleteMatch(), IDNA2008DeviationCharacter::kNone);
  nav_observer.Wait();

  // URL should not have been upgraded, and site should now be in the allowlist.
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_TRUE(state->IsHttpAllowedForHost(
      http_url.host(), contents->GetPrimaryMainFrame()->GetStoragePartition()));
}

// Url used to detect the presence of a captive portal.
constexpr char kCaptivePortalPingUrl[] = "http://captive-portal-ping-url.com/";
// HTTPS version of the same URL.
constexpr char kCaptivePortalPingUrlHttps[] =
    "https://captive-portal-ping-url.com/";

// Returns a URL loader interceptor that responds to HTTPS URLs with a cert
// error and to HTTP URLs with a good response.
std::unique_ptr<content::URLLoaderInterceptor> MakeCaptivePortalInterceptor(
    bool login_page_has_valid_https) {
  return std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [login_page_has_valid_https](
              content::URLLoaderInterceptor::RequestParams* params) {
            if (params->url_request.url == GURL(kCaptivePortalPingUrl) ||
                (login_page_has_valid_https &&
                 params->url_request.url == GURL(kCaptivePortalPingUrlHttps))) {
              // Return a non-204 response for the captive portal ping URL
              // so that the portal is detected.
              content::URLLoaderInterceptor::WriteResponse(
                  "HTTP/1.1 200 OK\nContent-type: text/html\n\n",
                  "<html>Non-204 response to trigger captive portal "
                  "detection</html>",
                  params->client.get());
              return true;
            }

            if (params->url_request.url.SchemeIs("https")) {
              // Fail with an SSL error so that a fallback is triggered.
              network::URLLoaderCompletionStatus status;
              status.error_code = net::ERR_CERT_COMMON_NAME_INVALID;
              status.ssl_info = net::SSLInfo();
              status.ssl_info->cert_status =
                  net::CERT_STATUS_COMMON_NAME_INVALID;
              // The cert doesn't matter.
              status.ssl_info->cert = net::ImportCertFromFile(
                  net::GetTestCertsDirectory(), "ok_cert.pem");
              status.ssl_info->unverified_cert = status.ssl_info->cert;
              params->client->OnComplete(status);
              return true;
            }
            content::URLLoaderInterceptor::WriteResponse(
                "HTTP/1.1 200 OK\nContent-type: text/html\n\n",
                "<html>Done</html>", params->client.get());
            return true;
          }));
}

void HttpsUpgradesBrowserTest::EnableCaptivePortalDetection(Browser* browser) {
  captive_portal::CaptivePortalService* captive_portal_service =
      CaptivePortalServiceFactory::GetForProfile(browser->profile());
  captive_portal_service->set_test_url(GURL(kCaptivePortalPingUrl));

  captive_portal::CaptivePortalService::set_state_for_testing(
      captive_portal::CaptivePortalService::NOT_TESTING);
  browser->profile()->GetPrefs()->SetBoolean(
      embedder_support::kAlternateErrorPagesEnabled, true);
}

// Checks that an automatically opened captive portal login page is not upgraded
// to HTTPS unless the interstitial is enabled. The captive portal's login
// page supports https.
IN_PROC_BROWSER_TEST_P(
    HttpsUpgradesBrowserTest,
    CaptivePortal_LoginPageWithValidSSL_ShouldNotUpgradeUnlessInterstitialEnabled) {
  if (https_upgrades_test_type() ==
      HttpsUpgradesTestType::kHttpsFirstModeIncognito) {
    return;
  }
  auto interceptor =
      MakeCaptivePortalInterceptor(/*login_page_has_valid_https=*/true);

  // Disable the testing port configuration, as this test doesn't use the
  // EmbeddedTestServer.
  HttpsUpgradesInterceptor::SetHttpsPortForTesting(0);
  HttpsUpgradesInterceptor::SetHttpPortForTesting(0);
  EnableCaptivePortalDetection(browser());

  auto* tab_strip = GetBrowser()->tab_strip_model();
  auto* contents = tab_strip->GetActiveWebContents();
  size_t tab_count = tab_strip->count();

  // Go to an HTTPS URL. The navigation will fail and trigger a captive portal
  // detection.
  ui_test_utils::TabAddedWaiter waiter(browser());
  NavigateAndWaitForFallback(contents,
                             GURL("https://ssl-error-for-captive-portal.com/"));
  waiter.Wait();

  // Captive portal login page should not be upgraded.
  content::WebContents* login_page = tab_strip->GetWebContentsAt(tab_count);
  content::WaitForLoadStop(login_page);
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          login_page));

  if (IsHttpsFirstModePrefEnabled()) {
    // If the interstitial is enabled, captive portal login page should also be
    // upgraded to HTTPS.
    EXPECT_EQ(GURL(kCaptivePortalPingUrlHttps),
              login_page->GetLastCommittedURL());

    // Should only attempt an upgrade for the original page.
    histograms()->ExpectTotalCount(kNavigationRequestSecurityLevelHistogram, 3);
    // The original page serves bad HTTPS, but any HTTPS URL is counted as
    // secure in this histogram. Captive portal login page is valid HTTPS, so
    // it's also counted here.
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kSecure, 2);
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kUpgraded,
                                    1);
  } else {
    // Captive portal login page should not be upgraded to HTTPS.
    EXPECT_EQ(GURL(kCaptivePortalPingUrl), login_page->GetLastCommittedURL());

    // Should only attempt an upgrade for the original page.
    histograms()->ExpectTotalCount(kNavigationRequestSecurityLevelHistogram, 2);
    // The original page serves bad HTTPS, but any HTTPS URL is counted as
    // secure in this histogram:
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kSecure, 1);
    histograms()->ExpectBucketCount(
        kNavigationRequestSecurityLevelHistogram,
        NavigationRequestSecurityLevel::kCaptivePortalLogin, 1);
  }
}

// Same as
// CaptivePortal_LoginPageWithValidSSL_ShouldNotUpgradeUnlessInterstitialEnabled
// but the captive portal's login page serves bad SSL.
IN_PROC_BROWSER_TEST_P(
    HttpsUpgradesBrowserTest,
    CaptivePortal_LoginPageWithoutValidSSL_ShouldNotUpgradeUnlessInterstitialEnabled) {
  if (https_upgrades_test_type() ==
      HttpsUpgradesTestType::kHttpsFirstModeIncognito) {
    return;
  }
  auto interceptor =
      MakeCaptivePortalInterceptor(/*login_page_has_valid_https=*/true);

  // Disable the testing port configuration, as this test doesn't use the
  // EmbeddedTestServer.
  HttpsUpgradesInterceptor::SetHttpsPortForTesting(0);
  HttpsUpgradesInterceptor::SetHttpPortForTesting(0);
  EnableCaptivePortalDetection(browser());

  auto* tab_strip = GetBrowser()->tab_strip_model();
  auto* contents = tab_strip->GetActiveWebContents();
  size_t tab_count = tab_strip->count();

  // Go to an HTTPS URL. The navigation will fail and trigger a captive portal
  // detection.
  ui_test_utils::TabAddedWaiter waiter(browser());
  NavigateAndWaitForFallback(contents,
                             GURL("https://ssl-error-for-captive-portal.com/"));
  waiter.Wait();

  content::WebContents* login_page = tab_strip->GetWebContentsAt(tab_count);
  content::WaitForLoadStop(login_page);

  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          login_page));

  if (IsHttpsFirstModePrefEnabled()) {
    // If the interstitial is enabled, captive portal login page should also be
    // upgraded to HTTPS.
    EXPECT_EQ(GURL(kCaptivePortalPingUrlHttps),
              login_page->GetLastCommittedURL());

    // Should only attempt an upgrade for the original page.
    histograms()->ExpectTotalCount(kNavigationRequestSecurityLevelHistogram, 3);
    // The original page serves bad HTTPS, but any HTTPS URL is counted as
    // secure in this histogram. Captive portal login page is valid HTTPS, so
    // it's also counted here.
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kSecure, 2);
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kUpgraded,
                                    1);
  } else {
    // Captive portal login page should not be upgraded to HTTPS.
    EXPECT_EQ(GURL(kCaptivePortalPingUrl), login_page->GetLastCommittedURL());

    // The original page serves bad HTTPS, but any HTTPS URL is counted as
    // secure in this histogram:
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kSecure, 1);
    histograms()->ExpectBucketCount(
        kNavigationRequestSecurityLevelHistogram,
        NavigationRequestSecurityLevel::kCaptivePortalLogin, 1);
  }
}

// A simple test fixture that constructs a HistogramTester (so that it gets
// initialized before browser startup). Used for testing pref tracking logic.
class HttpsUpgradesPrefsBrowserTest : public InProcessBrowserTest {
 public:
  HttpsUpgradesPrefsBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kHttpsFirstModeIncognito,
                               features::kHttpsFirstBalancedMode});
  }
  ~HttpsUpgradesPrefsBrowserTest() override = default;

 protected:
  void SetUISetting(HttpsFirstModeSetting setting) {
    extensions::settings_private::GeneratedPrefs prefs(browser()->profile());
    prefs.SetPref(
        kGeneratedHttpsFirstModePref,
        std::make_unique<base::Value>(static_cast<int>(setting)).get());
  }

  bool GetPref() const {
    auto* prefs = browser()->profile()->GetPrefs();
    return prefs->GetBoolean(prefs::kHttpsOnlyModeEnabled);
  }

  base::HistogramTester* histograms() { return &histograms_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histograms_;
};

// Tests that the HTTPS-First Mode state is recorded at startup and when
// changed. This test requires restarting the browser to test the "at startup"
// metric in order for the preference state to be set up before the
// HttpsFirstModeService is created.
IN_PROC_BROWSER_TEST_F(HttpsUpgradesPrefsBrowserTest, PRE_PrefStatesRecorded) {
  // The default pref state is `false`, which should get recorded when the
  // initial browser instance is started here.
  histograms()->ExpectUniqueSample(
      "Security.HttpsFirstMode.SettingEnabledAtStartup2",
      HttpsFirstModeSetting::kDisabled, 1);

  EXPECT_TRUE(variations::IsInSyntheticTrialGroup("HttpsFirstModeClientSetting",
                                                  "Disabled"));

  // Emulate changing the UI setting to Enabled. This should get recorded
  // in the histogram.
  SetUISetting(HttpsFirstModeSetting::kEnabledFull);
  histograms()->ExpectUniqueSample("Security.HttpsFirstMode.SettingChanged",
                                   true, 1);
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup("HttpsFirstModeClientSetting",
                                                  "Enabled"));
}

IN_PROC_BROWSER_TEST_F(HttpsUpgradesPrefsBrowserTest, PrefStatesRecorded) {
  // Restarting the browser from the PRE_ test should record the startup setting
  // histogram. Checking the unique count also ensures that other profile
  // types (e.g. the ChromeOS sign-in profile) don't cause double-counting.
  EXPECT_TRUE(GetPref());
  histograms()->ExpectUniqueSample(
      "Security.HttpsFirstMode.SettingEnabledAtStartup2",
      HttpsFirstModeSetting::kEnabledFull, 1);
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup("HttpsFirstModeClientSetting",
                                                  "Enabled"));

  // Open an Incognito window. Startup metrics should not get recorded.
  CreateIncognitoBrowser();
  histograms()->ExpectTotalCount(
      "Security.HttpsFirstMode.SettingEnabledAtStartup2", 1);
}

enum class BalancedModeParam {
  kNotAutoEnabled,
  kAutoEnabled,
};

// A simple test fixture that constructs a HistogramTester (so that it gets
// initialized before browser startup). Used for testing pref tracking logic.
// Variant of HttpsUpgradesPrefsBrowserTest but with the
// HttpsFirstBalancedMode feature enabled.
class HttpsUpgradesBalancedModePrefsBrowserTest
    : public testing::WithParamInterface<BalancedModeParam>,
      public InProcessBrowserTest {
 protected:
  void SetUp() override {
    // Feature flag must be enabled before SetUp() continues.
    switch (GetParam()) {
      case BalancedModeParam::kNotAutoEnabled:
        feature_list()->InitWithFeatures(
            /*enabled_features=*/{features::kHttpsFirstBalancedMode},
            /*disabled_features=*/{
                features::kHttpsFirstBalancedModeAutoEnable});
        break;

      case BalancedModeParam::kAutoEnabled:
        feature_list()->InitWithFeatures(
            /*enabled_features=*/{features::kHttpsFirstBalancedMode,
                                  features::kHttpsFirstBalancedModeAutoEnable},
            /*disabled_features=*/{});
        break;
    }
    InProcessBrowserTest::SetUp();
  }

  void SetUISetting(HttpsFirstModeSetting setting) {
    extensions::settings_private::GeneratedPrefs prefs(browser()->profile());
    prefs.SetPref(
        kGeneratedHttpsFirstModePref,
        std::make_unique<base::Value>(static_cast<int>(setting)).get());
  }

  bool GetPref() const {
    auto* prefs = browser()->profile()->GetPrefs();
    return prefs->GetBoolean(prefs::kHttpsFirstBalancedMode);
  }

  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }
  base::HistogramTester* histograms() { return &histograms_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histograms_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    HttpsUpgradesBalancedModePrefsBrowserTest,
    ::testing::Values(BalancedModeParam::kNotAutoEnabled,
                      BalancedModeParam::kAutoEnabled),
    // Map param to a human-readable string for better test output.
    [](testing::TestParamInfo<BalancedModeParam> input_type) -> std::string {
      switch (input_type.param) {
        case BalancedModeParam::kNotAutoEnabled:
          return "BalancedModeNotAutoEnabled";
        case BalancedModeParam::kAutoEnabled:
          return "BalancedModeAutoEnabled";
      }
    });

// Tests that the HTTPS-First Mode setting is recorded at startup and when
// changed, when the HFM-Balanced-Mode feature flag is enabled. This test
// requires restarting the browser to test the "at startup" metric in order
// for the preference state to be set up before the HttpsFirstModeService is
// created.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBalancedModePrefsBrowserTest,
                       PRE_PrefStatesRecorded) {
  if (GetParam() == BalancedModeParam::kNotAutoEnabled) {
    // The default Balanced Mode pref state is false, which should get recorded
    // when the initial browser instance is started here.
    histograms()->ExpectUniqueSample(
        "Security.HttpsFirstMode.SettingEnabledAtStartup2",
        HttpsFirstModeSetting::kDisabled, 1);

    EXPECT_TRUE(variations::IsInSyntheticTrialGroup(
        "HttpsFirstModeClientSetting", "Disabled"));
  } else if (GetParam() == BalancedModeParam::kAutoEnabled) {
    // The default Balanced Mode pref state is false, but Balanced Mode is auto
    // enabled.
    histograms()->ExpectUniqueSample(
        "Security.HttpsFirstMode.SettingEnabledAtStartup2",
        HttpsFirstModeSetting::kEnabledBalanced, 1);

    EXPECT_TRUE(variations::IsInSyntheticTrialGroup(
        "HttpsFirstModeClientSetting", "Balanced"));
  }

  // Emulate changing the UI setting to Balanced Mode. This should get recorded
  // in the histogram.
  SetUISetting(HttpsFirstModeSetting::kEnabledBalanced);
  EXPECT_TRUE(GetPref());
  histograms()->ExpectUniqueSample("Security.HttpsFirstMode.SettingChanged2",
                                   HttpsFirstModeSetting::kEnabledBalanced, 1);
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup("HttpsFirstModeClientSetting",
                                                  "Balanced"));
}

IN_PROC_BROWSER_TEST_P(HttpsUpgradesBalancedModePrefsBrowserTest,
                       PrefStatesRecorded) {
  // Restarting the browser from the PRE_ test should record the startup setting
  // histogram. Checking the unique count also ensures that other profile
  // types (e.g. the ChromeOS sign-in profile) don't cause double-counting.
  EXPECT_TRUE(GetPref());
  histograms()->ExpectUniqueSample(
      "Security.HttpsFirstMode.SettingEnabledAtStartup2",
      HttpsFirstModeSetting::kEnabledBalanced, 1);
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup("HttpsFirstModeClientSetting",
                                                  "Balanced"));

  // Open an Incognito window. Startup metrics should not get recorded.
  CreateIncognitoBrowser();
  histograms()->ExpectTotalCount(
      "Security.HttpsFirstMode.SettingEnabledAtStartup2", 1);
}

using TypicallySecureUserBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(TypicallySecureUserBrowserTest,
                       PRE_RestoreCountsOnStartup_OneNavigation) {
  HttpsFirstModeService* hfm_service =
      HttpsFirstModeServiceFactory::GetForProfile(browser()->profile());
  hfm_service->IncrementRecentNavigationCount();
}

IN_PROC_BROWSER_TEST_F(TypicallySecureUserBrowserTest,
                       RestoreCountsOnStartup_OneNavigation) {
  HttpsFirstModeService* hfm_service =
      HttpsFirstModeServiceFactory::GetForProfile(browser()->profile());
  // A single navigation will not be persisted to the pref and won't be
  // restored on startup.
  EXPECT_EQ(0u, hfm_service->GetRecentNavigationCount());
}

IN_PROC_BROWSER_TEST_F(TypicallySecureUserBrowserTest,
                       PRE_RestoreCountsOnStartup_TenNavigations) {
  HttpsFirstModeService* hfm_service =
      HttpsFirstModeServiceFactory::GetForProfile(browser()->profile());
  // Increment repeatedly to force the counts to be persisted to the pref.
  for (size_t i = 0; i < 10; i++) {
    hfm_service->IncrementRecentNavigationCount();
  }
}

IN_PROC_BROWSER_TEST_F(TypicallySecureUserBrowserTest,
                       RestoreCountsOnStartup_TenNavigations) {
  HttpsFirstModeService* hfm_service =
      HttpsFirstModeServiceFactory::GetForProfile(browser()->profile());
  EXPECT_EQ(10u, hfm_service->GetRecentNavigationCount());
}
