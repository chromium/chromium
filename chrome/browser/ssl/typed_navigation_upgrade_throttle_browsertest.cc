// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_test_utils.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/security_interstitials/content/ssl_error_handler.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "typed_navigation_upgrade_throttle.h"

namespace {

// Test URLs that load fine.
const char* const kSiteWithHttp = "http://test-site.com";
const char* const kSiteWithGoodHttps = "https://site-with-good-https.com";
const char* const kSiteWithGoodHttpsOverHttp =
    "http://site-with-good-https.com";

// Site that returns an SSL error over HTTPS (which would normally show an SSL
// interstitial) but loads fine over HTTP.
const char* const kSiteWithBadHttps = "https://site-with-bad-https.com";
const char* const kSiteWithBadHttpsOverHttp = "http://site-with-bad-https.com";
// Site that loads slowly over HTTPS, but loads fine over HTTP.
const char* const kSiteWithSlowHttps = "https://site-with-slow-https.com";
const char* const kSiteWithSlowHttpsOverHttp =
    "http://site-with-slow-https.com";

// Site that returns a connection error over HTTPS but loads fine over HTTP.
const char* const kSiteWithNetError = "https://site-with-net-error.com";
const char* const kSiteWithNetErrorOverHttp = "http://site-with-net-error.com";

const char kNetErrorHistogram[] = "Net.ErrorPageCounts";

enum class NavigationExpectation {
  // Test should expect a successful navigation to HTTPS.
  kExpectHttps,
  // Test should expect a fallback navigation to HTTP.
  kExpectHttp
};

std::string GetURLWithoutScheme(const GURL& url) {
  return url.spec().substr(url.scheme().size() + strlen("://"));
}

}  // namespace

class TypedNavigationUpgradeThrottleBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool /* IsFeatureEnabled */> {
 protected:
  TypedNavigationUpgradeThrottleBrowserTest() = default;
  ~TypedNavigationUpgradeThrottleBrowserTest() override = default;

  void SetUp() override {
    // Set the delay long enough so that the HTTPS navigation is guaranteed
    // to succeed or fail during this window.
    SetUpFeature(base::TimeDelta::FromHours(12));
    InProcessBrowserTest::SetUp();
  }

  void SetUpFeature(base::TimeDelta fallback_delay) {
    std::vector<base::test::ScopedFeatureList::FeatureAndParams>
        enabled_features;
    std::vector<base::Feature> disabled_features;
    if (IsFeatureEnabled()) {
      base::FieldTrialParams params;
      params["timeout"] =
          base::NumberToString(fallback_delay.InMilliseconds()) + "ms";
      enabled_features.emplace_back(omnibox::kDefaultTypedNavigationsToHttps,
                                    params);
    } else {
      disabled_features.push_back(omnibox::kDefaultTypedNavigationsToHttps);
    }
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }
  void SetUpOnMainThread() override {
    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            &TypedNavigationUpgradeThrottleBrowserTest::OnIntercept,
            base::Unretained(this)));

    WaitForHistoryToLoad();
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

  bool OnIntercept(content::URLLoaderInterceptor::RequestParams* params) {
    // Instead of EmbeddedTestServer, we use URLLoaderInterceptor so that we can
    // load URLs using the default ports. The autocomplete code that upgrades
    // URLs from HTTP to HTTPS assumes default ports: it just changes the scheme
    // of URLs that don't have a port (since it can't guess what port the HTTPS
    // URL is being served from). EmbeddedTestServer doesn't support serving
    // HTTP or HTTPS on default ports.
    network::URLLoaderCompletionStatus status;
    status.error_code = net::OK;

    if (params->url_request.url == GURL(kSiteWithBadHttps)) {
      // Fail with an SSL error.
      status.error_code = net::ERR_CERT_COMMON_NAME_INVALID;
      status.ssl_info = net::SSLInfo();
      status.ssl_info->cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;
      // The cert doesn't matter.
      status.ssl_info->cert =
          net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
      status.ssl_info->unverified_cert = status.ssl_info->cert;
      params->client->OnComplete(status);
      return true;
    }

    if (params->url_request.url == GURL(kSiteWithNetError)) {
      params->client->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_CONNECTION_RESET));
      return true;
    }

    if (params->url_request.url == GURL(kSiteWithSlowHttps)) {
      // Do nothing. This will hang the load.
      return true;
    }

    if (params->url_request.url == GURL(kSiteWithHttp) ||
        params->url_request.url == GURL(kSiteWithGoodHttps) ||
        params->url_request.url == GURL(kSiteWithBadHttpsOverHttp) ||
        params->url_request.url == GURL(kSiteWithSlowHttpsOverHttp) ||
        params->url_request.url == GURL(kSiteWithNetErrorOverHttp)) {
      std::string headers =
          "HTTP/1.1 200 OK\nContent-Type: text/html; charset=utf-8\n";
      std::string body = "<html><title>Success</title>Hello world</html>";
      content::URLLoaderInterceptor::WriteResponse(headers, body,
                                                   params->client.get());
      return true;
    }
    return false;
  }

 protected:
  bool IsFeatureEnabled() const { return GetParam(); }

  OmniboxView* omnibox() {
    return browser()->window()->GetLocationBar()->GetOmniboxView();
  }

  void FocusOmnibox() {
    // If the omnibox already has focus, just notify OmniboxTabHelper.
    if (omnibox()->model()->has_focus()) {
      content::WebContents* active_tab =
          browser()->tab_strip_model()->GetActiveWebContents();
      OmniboxTabHelper::FromWebContents(active_tab)
          ->OnFocusChanged(OMNIBOX_FOCUS_VISIBLE,
                           OMNIBOX_FOCUS_CHANGE_EXPLICIT);
    } else {
      browser()->window()->GetLocationBar()->FocusLocation(false);
    }
  }

  void SetOmniboxText(const std::string& text) {
    FocusOmnibox();
    // Enter user input mode to prevent spurious unelision.
    omnibox()->model()->SetInputInProgress(true);
    omnibox()->OnBeforePossibleChange();
    omnibox()->SetUserText(base::UTF8ToUTF16(text), true);
    omnibox()->OnAfterPossibleChange(true);
  }

  void TypeUrlAndCheckNavigation(const std::string& hostname,
                                 const base::HistogramTester& histograms,
                                 NavigationExpectation expectation) {
    const GURL http_url(std::string("http://") + hostname);
    const GURL https_url(std::string("https://") + hostname);
    SetOmniboxText(hostname);

    // Wait for navigations.
    // - If the expectation is to successfully load the HTTPS URL, there should
    // be a single navigation.
    // - Otherwise, there should be two navigations: One for the initial HTTPS
    // navigation (which will be cancelled because of the timeout, or SSL or net
    // errors) and one for the fallback HTTP navigation (which will succeed).
    PressEnterAndWaitForNavigations(
        expectation == NavigationExpectation::kExpectHttps ? 1 : 2);

    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    const GURL expected_url = expectation == NavigationExpectation::kExpectHttps
                                  ? https_url
                                  : http_url;
    EXPECT_EQ(expected_url, contents->GetLastCommittedURL());

    // Should never hit an error page.
    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                0);
    histograms.ExpectTotalCount(kNetErrorHistogram, 0);

    // Should have either the HTTP or the HTTPS URL in history, but not both.
    ui_test_utils::HistoryEnumerator enumerator(browser()->profile());
    if (expectation == NavigationExpectation::kExpectHttp) {
      EXPECT_TRUE(base::Contains(enumerator.urls(), http_url));
      EXPECT_FALSE(base::Contains(enumerator.urls(), https_url));
    } else {
      EXPECT_TRUE(base::Contains(enumerator.urls(), https_url));
      EXPECT_FALSE(base::Contains(enumerator.urls(), http_url));
    }
  }

  void PressEnterAndWaitForNavigations(size_t num_navigations) {
    content::TestNavigationObserver navigation_observer(
        browser()->tab_strip_model()->GetActiveWebContents(), num_navigations);
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN,
                                                false, false, false, false));
    navigation_observer.Wait();
  }

  void WaitForHistoryToLoad() {
    history::HistoryService* const history_service =
        HistoryServiceFactory::GetForProfile(
            browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS);
    ui_test_utils::WaitForHistoryToLoad(history_service);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         TypedNavigationUpgradeThrottleBrowserTest,
                         testing::Bool() /* IsFeatureEnabled */);

// If the user types a full HTTP URL, the navigation should end up on that
// exact URL.
IN_PROC_BROWSER_TEST_P(TypedNavigationUpgradeThrottleBrowserTest,
                       UrlTypedWithHttpScheme) {
  base::HistogramTester histograms;
  const GURL url(kSiteWithHttp);

  // Type "http://test-site.com".
  SetOmniboxText(url.spec());
  PressEnterAndWaitForNavigations(1);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(url, contents->GetLastCommittedURL());
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));

  histograms.ExpectTotalCount(TypedNavigationUpgradeThrottle::kHistogramName,
                              0);

  ui_test_utils::HistoryEnumerator enumerator(browser()->profile());
  EXPECT_TRUE(base::Contains(enumerator.urls(), url));
}

// If the user types a full HTTPS URL, the navigation should end up on that
// exact URL.
IN_PROC_BROWSER_TEST_P(TypedNavigationUpgradeThrottleBrowserTest,
                       UrlTypedWithHttpsScheme) {
  base::HistogramTester histograms;
  const GURL url(kSiteWithGoodHttps);

  // Type "https://site-with-good-https.com".
  SetOmniboxText(url.spec());
  PressEnterAndWaitForNavigations(1);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(url, contents->GetLastCommittedURL());
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));

  histograms.ExpectTotalCount(TypedNavigationUpgradeThrottle::kHistogramName,
                              0);

  ui_test_utils::HistoryEnumerator enumerator(browser()->profile());
  EXPECT_TRUE(base::Contains(enumerator.urls(), url));
}

// If the user types a full HTTPS URL, the navigation should end up on that
// exact URL, even if the site has an SSL error.
IN_PROC_BROWSER_TEST_P(TypedNavigationUpgradeThrottleBrowserTest,
                       UrlTypedWithHttpsScheme_BrokenSSL) {
  base::HistogramTester histograms;
  const GURL url(kSiteWithBadHttps);

  // Type "https://site-with-bad-https.com".
  SetOmniboxText(url.spec());
  PressEnterAndWaitForNavigations(1);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(url, contents->GetLastCommittedURL());
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(contents));

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);

  histograms.ExpectTotalCount(TypedNavigationUpgradeThrottle::kHistogramName,
                              0);

  // Broken SSL results in an interstitial and interstitial pages aren't added
  // to history.
  ui_test_utils::HistoryEnumerator enumerator(browser()->profile());
  EXPECT_FALSE(base::Contains(enumerator.urls(), url));
}

// If the feature is disabled, typing a URL in the omnibox without a scheme
// should load the HTTP version.
IN_PROC_BROWSER_TEST_P(TypedNavigationUpgradeThrottleBrowserTest,
                       UrlTypedWithoutScheme_FeatureDisabled) {
  if (IsFeatureEnabled()) {
    return;
  }
  base::HistogramTester histograms;
  const GURL url(kSiteWithHttp);

  // Type "test-site.com".
  SetOmniboxText(GetURLWithoutScheme(url));
  PressEnterAndWaitForNavigations(1);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(url, contents->GetLastCommittedURL());
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));

  histograms.ExpectTotalCount(TypedNavigationUpgradeThrottle::kHistogramName,
                              0);
}

// If the feature is enabled, typing a URL in the omnibox without a scheme
// should load the HTTPS version.
IN_PROC_BROWSER_TEST_P(TypedNavigationUpgradeThrottleBrowserTest,
                       UrlTypedWithoutScheme_GoodHttps) {
  if (!IsFeatureEnabled()) {
    return;
  }
  base::HistogramTester histograms;
  const GURL https_url(kSiteWithGoodHttps);
  const GURL http_url(kSiteWithGoodHttpsOverHttp);

  // Type "site-with-good-https.com".
  const GURL url(kSiteWithGoodHttps);
  TypeUrlAndCheckNavigation(url.host(), histograms,
                            NavigationExpectation::kExpectHttps);

  histograms.ExpectTotalCount(TypedNavigationUpgradeThrottle::kHistogramName,
                              2);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadStarted, 1);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadSucceeded, 1);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadTimedOut, 0);

  // Now that the HTTPS URL is in history, try again. We should load it directly
  // without going through the upgrade.
  // Type "site-with-good-https.com".
  SetOmniboxText(url.host());
  TypeUrlAndCheckNavigation(url.host(), histograms,
                            NavigationExpectation::kExpectHttps);

  // Since the throttle wasn't involved in the second navigation, histogram
  // values shouldn't change.
  histograms.ExpectTotalCount(TypedNavigationUpgradeThrottle::kHistogramName,
                              2);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadStarted, 1);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadSucceeded, 1);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadTimedOut, 0);
}

// If the upgraded HTTPS URL is not available because of an SSL error), we
// should load the HTTP URL.
IN_PROC_BROWSER_TEST_P(TypedNavigationUpgradeThrottleBrowserTest,
                       UrlTypedWithoutScheme_BadHttps_ShouldFallback) {
  if (!IsFeatureEnabled()) {
    return;
  }

  base::HistogramTester histograms;
  const GURL https_url(kSiteWithBadHttps);
  const GURL http_url(kSiteWithBadHttpsOverHttp);

  // Type "site-with-bad-https.com".
  const GURL url(kSiteWithBadHttps);
  TypeUrlAndCheckNavigation(url.host(), histograms,
                            NavigationExpectation::kExpectHttp);

  histograms.ExpectTotalCount(TypedNavigationUpgradeThrottle::kHistogramName,
                              2);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadStarted, 1);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadFailedWithCertError, 1);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadTimedOut, 0);

  // TODO(meacer): Try again and check that the histogram counts doubled. Doing
  // that currently fails on lacros because this time the navigation never gets
  // upgraded (probably because of an issue in the autocomplete logic).
}

// If the upgraded HTTPS URL is not available because of a net error, we should
// load the HTTP URL.
IN_PROC_BROWSER_TEST_P(TypedNavigationUpgradeThrottleBrowserTest,
                       UrlTypedWithoutScheme_NetError_ShouldFallback) {
  if (!IsFeatureEnabled()) {
    return;
  }
  base::HistogramTester histograms;
  // Type "site-with-net-error.com".
  const GURL https_url(kSiteWithNetError);
  const GURL http_url(kSiteWithNetErrorOverHttp);
  TypeUrlAndCheckNavigation(http_url.host(), histograms,
                            NavigationExpectation::kExpectHttp);

  histograms.ExpectTotalCount(TypedNavigationUpgradeThrottle::kHistogramName,
                              2);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadStarted, 1);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadFailedWithNetError, 1);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadTimedOut, 0);

  ui_test_utils::HistoryEnumerator enumerator(browser()->profile());
  EXPECT_TRUE(base::Contains(enumerator.urls(), http_url));
  EXPECT_FALSE(base::Contains(enumerator.urls(), https_url));

  // TODO(meacer): Try again and check that the histogram counts doubled. Doing
  // that currently fails on lacros because this time the navigation never gets
  // upgraded (probably because of an issue in the autocomplete logic).
}

class TypedNavigationUpgradeThrottleFastTimeoutBrowserTest
    : public TypedNavigationUpgradeThrottleBrowserTest {
 protected:
  void SetUp() override {
    // Set timeout to zero so that HTTPS upgrades immediately timeout.
    SetUpFeature(base::TimeDelta::FromSeconds(0));
    InProcessBrowserTest::SetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         TypedNavigationUpgradeThrottleFastTimeoutBrowserTest,
                         testing::Bool() /* IsFeatureEnabled */);

// If the upgraded HTTPS URL does not load within the timeout window, we should
// load the HTTP URL.
IN_PROC_BROWSER_TEST_P(TypedNavigationUpgradeThrottleFastTimeoutBrowserTest,
                       UrlTypedWithoutScheme_SlowHttps_ShouldFallback) {
  if (!IsFeatureEnabled()) {
    return;
  }

  base::HistogramTester histograms;

  // Type "site-with-slow-https.com".
  const GURL url(kSiteWithSlowHttps);
  TypeUrlAndCheckNavigation(url.host(), histograms,
                            NavigationExpectation::kExpectHttp);

  histograms.ExpectTotalCount(TypedNavigationUpgradeThrottle::kHistogramName,
                              2);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadStarted, 1);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadTimedOut, 1);
}

// TODO(crbug.com/1141691): Test the following cases:
// - Various types of omnibox entries (URLs typed with a port, URLs in history,
// non-unique URLs such as machine.local, IP addresses etc.
// - Redirects (either in the upgraded HTTPS navigation or in the fallback)
// - Various types of navigation states such as downloads, external protocols
// etc.
// - Non-cert errors such as HTTP 4XX or 5XX.
// - Test cases for crbug.com/1161620.
