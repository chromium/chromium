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
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/security_interstitials/content/ssl_error_handler.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "typed_navigation_upgrade_throttle.h"

namespace {

// Test URLs that load fine.
const char* const kSiteWithHttp = "site-with-http.com";
const char* const kSiteWithGoodHttps = "site-with-good-https.com";

// Site that loads fine over HTTPS and redirects to kSiteWithGoodHttps.
const char* const kSiteWithGoodHttpsRedirect =
    "site-with-good-https-redirect.com";

// Site that returns an SSL error over HTTPS (which would normally show an SSL
// interstitial) but loads fine over HTTP.
const char* const kSiteWithBadHttps = "site-with-bad-https.com";

// Site that loads slowly over HTTPS, but loads fine over HTTP.
const char* const kSiteWithSlowHttps = "site-with-slow-https.com";

// Site that returns a connection error over HTTPS but loads fine over HTTP.
const char* const kSiteWithNetError = "site-with-net-error.com";

// Site (likely on an intranet) that contains a non-registerable or
// non-assignable domain name (eg: a gTLD that has not been assigned by IANA)
// that therefore is unlikely to support HTTPS.
const char* const kNonUniqueHostname1 = "testpage";
const char* const kNonUniqueHostname2 = "site.test";

const char kNetErrorHistogram[] = "Net.ErrorPageCounts";

enum class NavigationExpectation {
  // Test should expect a successful navigation to HTTPS.
  kExpectHttps,
  // Test should expect a fallback navigation to HTTP.
  kExpectHttp,
  // Test should expect a search query navigation. This happens when the user
  // enters a non-URL query such as "testpage".
  kExpectSearch
};

enum class UpgradeExpectation {
  // Test should expect a search query navigation. This happens when the user
  // enters a non-URL query such as "testpage".
  kExpectSearch,
  // Test should expect the initial URL to not be upgraded.
  kExpectNoUpgrade,
  // Test should expect the initial URL to successfully be upgraded. The final
  // URL may or may not be the https version of the initial URL:
  // - If the initial URL does not redirect, the final URL will be its https
  //    version.
  // - If the initial URL does redirect, the final URL can be completely
  //   different.
  kExpectSuccessfulUpgrade,
  // Test should expect the initial URL to fall back to HTTP.
  kExpectFallback
};

std::string GetURLWithoutScheme(const GURL& url) {
  return url.spec().substr(url.scheme().size() + strlen("://"));
}

GURL MakeHttpsURL(const std::string& url_without_scheme) {
  return GURL("https://" + url_without_scheme);
}

GURL MakeHttpURL(const std::string& url_without_scheme) {
  return GURL("http://" + url_without_scheme);
}

GURL MakeURLWithPort(const std::string& url_without_scheme,
                     const std::string& scheme,
                     int port) {
  GURL url(scheme + "://" + url_without_scheme);
  DCHECK(!url.port().empty());

  GURL::Replacements replacements;
  const std::string port_str = base::NumberToString(port);
  replacements.SetPortStr(port_str);
  return url.ReplaceComponents(replacements);
}

GURL MakeHttpsURLWithPort(const std::string& url_without_scheme, int port) {
  return MakeURLWithPort(url_without_scheme, "https", port);
}

GURL MakeHttpURLWithPort(const std::string& url_without_scheme, int port) {
  return MakeURLWithPort(url_without_scheme, "http", port);
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
      params[omnibox::kDefaultTypedNavigationsToHttpsTimeoutParam] =
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

    if (params->url_request.url == MakeHttpsURL(kSiteWithBadHttps)) {
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

    if (params->url_request.url == MakeHttpsURL(kSiteWithNetError)) {
      params->client->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_CONNECTION_RESET));
      return true;
    }

    if (params->url_request.url == MakeHttpsURL(kSiteWithSlowHttps)) {
      // Do nothing. This will hang the load.
      return true;
    }

    if (params->url_request.url == MakeHttpsURL(kSiteWithGoodHttps) ||
        params->url_request.url == MakeHttpURL(kSiteWithHttp) ||
        params->url_request.url == MakeHttpURL(kSiteWithBadHttps) ||
        params->url_request.url == MakeHttpURL(kSiteWithSlowHttps) ||
        params->url_request.url == MakeHttpURL(kSiteWithNetError) ||
        params->url_request.url == MakeHttpURL(kNonUniqueHostname1) ||
        params->url_request.url == MakeHttpURL(kNonUniqueHostname2) ||
        params->url_request.url == GURL("http://127.0.0.1")) {
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

  void TypeUrlAndExpectSuccessfulUpgrade(
      const std::string& url_without_scheme) {
    ASSERT_TRUE(IsFeatureEnabled());
    base::HistogramTester histograms;
    TypeUrlAndExpectHttps(url_without_scheme, histograms, 1);

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

  // Type |url_without_scheme| in the URL bar and hit enter. The navigation
  // shouldn't be upgraded to HTTPS. Expect a search query to be issued if
  // |expect_search_query| is true. Otherwise, the final URL will be an HTTP
  // URL.
  void TypeUrlAndExpectNoUpgrade(const std::string& url_without_scheme,
                                 bool expect_search_query) {
    base::HistogramTester histograms;
    TypeUrlAndCheckNavigation(url_without_scheme, histograms,
                              expect_search_query
                                  ? NavigationExpectation::kExpectSearch
                                  : NavigationExpectation::kExpectHttp,
                              1);
    histograms.ExpectTotalCount(TypedNavigationUpgradeThrottle::kHistogramName,
                                0);
  }

  // Type |url_without_scheme| in the URL bar and hit enter. The navigation
  // should initially be upgraded to HTTPS but then fall back to HTTP because
  // the HTTPS URL wasn't available (e.g. had an SSL or net error).
  void TypeUrlAndExpectHttpFallback(const std::string& url_without_scheme,
                                    const base::HistogramTester& histograms) {
    // There should be two navigations: One for the initial HTTPS
    // navigation (which will be cancelled because of the timeout, or SSL or net
    // errors) and one for the fallback HTTP navigation (which will succeed).
    TypeUrlAndCheckNavigation(url_without_scheme, histograms,
                              NavigationExpectation::kExpectHttp, 2);
  }

  // Type |url_without_scheme| in the URL bar and hit enter. The navigation
  // should be upgraded to HTTPS and the HTTPS URL should successfully load.
  void TypeUrlAndExpectHttps(const std::string& url_without_scheme,
                             const base::HistogramTester& histograms,
                             size_t num_expected_navigations = 1) {
    TypeUrlAndCheckNavigation(url_without_scheme, histograms,
                              NavigationExpectation::kExpectHttps,
                              num_expected_navigations);
  }

  void PressEnterAndWaitForNavigations(size_t num_navigations) {
    content::TestNavigationObserver navigation_observer(
        browser()->tab_strip_model()->GetActiveWebContents(), num_navigations);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](const Browser* browser) {
              EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
                  browser, ui::VKEY_RETURN, false, false, false, false));
            },
            browser()));
    navigation_observer.Wait();
  }

  void WaitForHistoryToLoad() {
    history::HistoryService* const history_service =
        HistoryServiceFactory::GetForProfile(
            browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS);
    ui_test_utils::WaitForHistoryToLoad(history_service);
  }

 private:
  void WaitForAutocompleteControllerDone() {
    AutocompleteController* controller =
        omnibox()->model()->autocomplete_controller();
    ASSERT_TRUE(controller);

    if (controller->done())
      return;

    ui_test_utils::WaitForAutocompleteDone(browser());
    ASSERT_TRUE(controller->done());
  }

  void TypeUrlAndCheckNavigation(const std::string& url_without_scheme,
                                 const base::HistogramTester& histograms,
                                 NavigationExpectation expectation,
                                 size_t num_expected_navigations) {
    SetOmniboxText(url_without_scheme);
    // Regression check for crbug.com/1184872: The first autocomplete result
    // should be the same as the typed text, without a scheme.
    OmniboxPopupModel* popup_model = omnibox()->model()->popup_model();
    ASSERT_TRUE(popup_model);
    WaitForAutocompleteControllerDone();
    ASSERT_TRUE(popup_model->IsOpen());
    EXPECT_EQ(base::UTF8ToUTF16(url_without_scheme),
              popup_model->result().match_at(0).fill_into_edit);

    PressEnterAndWaitForNavigations(num_expected_navigations);

    ui_test_utils::HistoryEnumerator enumerator(browser()->profile());
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    const GURL http_url = MakeHttpURL(url_without_scheme);
    const GURL https_url = MakeHttpsURL(url_without_scheme);

    if (expectation != NavigationExpectation::kExpectSearch) {
      const GURL expected_url =
          expectation == NavigationExpectation::kExpectHttps ? https_url
                                                             : http_url;
      EXPECT_EQ(expected_url, contents->GetLastCommittedURL());

      // Should have either the HTTP or the HTTPS URL in history, but not both.
      if (expectation == NavigationExpectation::kExpectHttp) {
        EXPECT_TRUE(base::Contains(enumerator.urls(), http_url));
        EXPECT_FALSE(base::Contains(enumerator.urls(), https_url));
      } else {
        EXPECT_TRUE(base::Contains(enumerator.urls(), https_url));
        EXPECT_FALSE(base::Contains(enumerator.urls(), http_url));
      }
    } else {
      // The user entered a search query.
      EXPECT_EQ("www.google.com", contents->GetLastCommittedURL().host());
      EXPECT_FALSE(base::Contains(enumerator.urls(), https_url));
    }

    // Should never hit an error page.
    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                0);
    histograms.ExpectTotalCount(kNetErrorHistogram, 0);
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         TypedNavigationUpgradeThrottleBrowserTest,
                         testing::Bool() /* IsFeatureEnabled */);

// If the user types a full HTTP URL, the navigation should end up on that
// exact URL.
IN_PROC_BROWSER_TEST_P(TypedNavigationUpgradeThrottleBrowserTest,
                       UrlTypedWithHttpScheme_ShouldNotUpgrade) {
  base::HistogramTester histograms;
  const GURL url = MakeHttpURL(kSiteWithHttp);

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
                       UrlTypedWithHttpsScheme_ShouldNotUpgrade) {
  base::HistogramTester histograms;
  const GURL url = MakeHttpsURL(kSiteWithGoodHttps);

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
                       UrlTypedWithHttpsScheme_BrokenSSL_ShouldNotUpgrade) {
  base::HistogramTester histograms;
  const GURL url = MakeHttpsURL(kSiteWithBadHttps);

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
                       UrlTypedWithoutScheme_FeatureDisabled_ShouldNotUpgrade) {
  if (IsFeatureEnabled()) {
    return;
  }
  base::HistogramTester histograms;
  const GURL http_url = MakeHttpURL(kSiteWithHttp);

  // Type "test-site.com".
  SetOmniboxText(GetURLWithoutScheme(http_url));
  PressEnterAndWaitForNavigations(1);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));

  histograms.ExpectTotalCount(TypedNavigationUpgradeThrottle::kHistogramName,
                              0);
}

// Test the case when the user types a search keyword. The keyword may or may
// not be a non-unique hostname. The navigation should always result in a
// search and we should never upgrade it to https.
IN_PROC_BROWSER_TEST_P(TypedNavigationUpgradeThrottleBrowserTest,
                       SearchQuery_ShouldNotUpgrade) {
  TypeUrlAndExpectNoUpgrade("testpage", /*expect_search_query=*/true);
}

// Same as SearchQuery_ShouldNotUpgrade but with two words. This is a definite
// search query, and can never be a hostname.
IN_PROC_BROWSER_TEST_P(TypedNavigationUpgradeThrottleBrowserTest,
                       SearchQuery_TwoWords_ShouldNotUpgrade) {
  TypeUrlAndExpectNoUpgrade("test page", /*expect_search_query=*/true);
}

// Test the case when the user types a non-unique hostname. We shouldn't upgrade
// it to https.
IN_PROC_BROWSER_TEST_P(TypedNavigationUpgradeThrottleBrowserTest,
                       NonUniqueHostnameTypedWithoutScheme_ShouldNotUpgrade) {
  TypeUrlAndExpectNoUpgrade(kNonUniqueHostname2, /*expect_search_query=*/false);
}

// Test the case when the user types an IP address. We shouldn't upgrade it to
// https.
IN_PROC_BROWSER_TEST_P(TypedNavigationUpgradeThrottleBrowserTest,
                       IPAddressTypedWithoutScheme_ShouldNotUpgrade) {
  TypeUrlAndExpectNoUpgrade("127.0.0.1", /*expect_search_query=*/false);
}

// If the feature is enabled, typing a URL in the omnibox without a scheme
// should load the HTTPS version.
IN_PROC_BROWSER_TEST_P(TypedNavigationUpgradeThrottleBrowserTest,
                       UrlTypedWithoutScheme_GoodHttps) {
  if (!IsFeatureEnabled()) {
    return;
  }
  TypeUrlAndExpectSuccessfulUpgrade(kSiteWithGoodHttps);

  // Try again. Should directly load the https version this time and not record
  // any histograms in the throttle.
  base::HistogramTester histograms;
  TypeUrlAndExpectHttps(kSiteWithGoodHttps, histograms, 1);
  histograms.ExpectTotalCount(TypedNavigationUpgradeThrottle::kHistogramName,
                              0);
}

// If the upgraded HTTPS URL is not available because of an SSL error), we
// should load the HTTP URL.
IN_PROC_BROWSER_TEST_P(TypedNavigationUpgradeThrottleBrowserTest,
                       UrlTypedWithoutScheme_BadHttps_ShouldFallback) {
  if (!IsFeatureEnabled()) {
    return;
  }

  base::HistogramTester histograms;
  // Type "site-with-bad-https.com".
  const GURL http_url = MakeHttpURL(kSiteWithBadHttps);
  TypeUrlAndExpectHttpFallback(http_url.host(), histograms);

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

  // Try again. This time the omnibox will find a history match for the http
  // URL and navigate directly to it. Histograms shouldn't change.
  // TODO(crbug.com/1169564): We should try the https URL after a certain
  // time has passed.
  TypeUrlAndExpectNoUpgrade(http_url.host(), false);

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
  TypeUrlAndExpectHttpFallback(kSiteWithNetError, histograms);

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

  // Try again. This time the omnibox will find a history match for the http
  // URL and navigate directly to it. Histograms shouldn't change.
  // TODO(crbug.com/1169564): We should try the https URL after a certain
  // time has passed.
  TypeUrlAndExpectNoUpgrade(kSiteWithNetError, false);

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
  const GURL url = MakeHttpsURL(kSiteWithSlowHttps);
  TypeUrlAndExpectHttpFallback(url.host(), histograms);

  histograms.ExpectTotalCount(TypedNavigationUpgradeThrottle::kHistogramName,
                              2);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadStarted, 1);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadTimedOut, 1);
}

// Tests redirects. This is a separate class because as there currently doesn't
// seem to be a good way to properly simulate redirects using
// content::URLLoaderInterceptor. Instead, this class uses an EmbeddedTestServer
// with https.
// Previously, AutocompleteInput didn't upgrade URLs with a non-default port to
// HTTPS. This class passes the port of the https server to AutocompleteInput
// which in turn replaces any non-default http port with the https port.
//
// For example, assume that the http EmbeddedTestServer runs on port 5678 and
// the https EmbeddedTestServer runs on port 8765. Then, AutocompleteInput will
// see example.com:5678 and upgrade it to https://example.com:8765.
//
// TODO(crbug.com/1168371): Fold into TypedNavigationUpgradeThrottleBrowserTest
// when URLLoaderInterceptor supports redirects.
class TypedNavigationUpgradeThrottleRedirectBrowserTest
    : public TypedNavigationUpgradeThrottleBrowserTest {
 protected:
  TypedNavigationUpgradeThrottleRedirectBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    WaitForHistoryToLoad();
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    // Don't use "*" here, one of the tests simulates a DNS error.
    host_resolver()->AddRule(kSiteWithGoodHttps, "127.0.0.1");
    host_resolver()->AddRule(kSiteWithGoodHttpsRedirect, "127.0.0.1");
    host_resolver()->AddRule(kSiteWithBadHttps, "127.0.0.1");

    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
    ASSERT_TRUE(embedded_test_server()->Start());

    TypedNavigationUpgradeThrottle::SetHttpsPortForTesting(
        https_server_.port());
    TypedNavigationUpgradeThrottle::SetHttpPortForTesting(
        embedded_test_server()->port());
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    TypedNavigationUpgradeThrottleBrowserTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  // Type |url_without_scheme| in the omnibox, press enter and expect a redirect
  // that ends up on a good SSL page.
  void TypeUrlAndCheckRedirectToGoodHttps(
      const std::string& url_without_scheme,
      const base::HistogramTester& histograms,
      const GURL& expected_final_url) {
    SetOmniboxText(url_without_scheme);
    PressEnterAndWaitForNavigations(1);

    ui_test_utils::HistoryEnumerator enumerator(browser()->profile());
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    const GURL http_url =
        MakeHttpURLWithPort(url_without_scheme, embedded_test_server()->port());
    const GURL https_url =
        MakeHttpsURLWithPort(url_without_scheme, https_server_.port());

    EXPECT_EQ(expected_final_url, contents->GetLastCommittedURL());

    // Should never hit an error page.
    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                0);
    histograms.ExpectTotalCount(kNetErrorHistogram, 0);
    // The http or https version of the URL shouldn't be in history because
    // of the redirect.
    EXPECT_FALSE(base::Contains(enumerator.urls(), https_url));
    EXPECT_FALSE(base::Contains(enumerator.urls(), http_url));
  }

  // Type |url_without_scheme| in the omnibox, press enter and expect a redirect
  // that ends up on an SSL or net error page. This should fall back to the http
  // version which also redirects to the same error page.
  void TypeUrlAndCheckRedirectToBadHttps(
      const std::string& url_without_scheme,
      const base::HistogramTester& histograms,
      const GURL& expected_final_url) {
    SetOmniboxText(url_without_scheme);
    PressEnterAndWaitForNavigations(2);

    ui_test_utils::HistoryEnumerator enumerator(browser()->profile());
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    const GURL http_url =
        MakeHttpURLWithPort(url_without_scheme, embedded_test_server()->port());
    const GURL https_url =
        MakeHttpsURLWithPort(url_without_scheme, https_server_.port());

    EXPECT_EQ(expected_final_url, contents->GetLastCommittedURL());

    // Neither the https nor the http version of the URL should be in history.
    // - https URL eventually failed to load and we fell back to the http URL.
    // - http URL redirected to an SSL error or a net error.
    EXPECT_FALSE(base::Contains(enumerator.urls(), https_url));
    EXPECT_FALSE(base::Contains(enumerator.urls(), http_url));
  }

  void SetUpMockCertVerifierWithErrorForHttpsServer(
      const std::string& hostname) {
    scoped_refptr<net::X509Certificate> cert(https_server_.GetCertificate());
    net::CertVerifyResult verify_result;
    verify_result.is_issued_by_known_root = false;
    verify_result.verified_cert = cert;
    verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

    mock_cert_verifier_.mock_cert_verifier()->AddResultForCertAndHost(
        cert, hostname, verify_result, net::ERR_CERT_COMMON_NAME_INVALID);
  }

 private:
  net::EmbeddedTestServer https_server_;
  content::ContentMockCertVerifier mock_cert_verifier_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         TypedNavigationUpgradeThrottleRedirectBrowserTest,
                         testing::Bool() /* IsFeatureEnabled */);

// If the feature is enabled, typing a URL in the omnibox without a scheme
// should load the HTTPS version. In this test, the HTTPS site redirects to
// another working HTTPS site. This should succeed and an additional histogram
// entry should be recorded.
IN_PROC_BROWSER_TEST_P(
    TypedNavigationUpgradeThrottleRedirectBrowserTest,
    UrlTypedWithoutScheme_GoodHttps_Redirected_ShouldUpgrade) {
  if (!IsFeatureEnabled()) {
    return;
  }
  const GURL target_url =
      https_server()->GetURL(kSiteWithGoodHttps, "/title1.html");
  const GURL url = embedded_test_server()->GetURL(
      kSiteWithGoodHttpsRedirect, "/server-redirect?" + target_url.spec());

  base::HistogramTester histograms;
  TypeUrlAndCheckRedirectToGoodHttps(GetURLWithoutScheme(url), histograms,
                                     target_url);

  histograms.ExpectTotalCount(TypedNavigationUpgradeThrottle::kHistogramName,
                              3);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadStarted, 1);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadSucceeded, 1);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kRedirected, 1);

  // Try again. The navigation will be upgraded again and metrics will be
  // recorded.
  TypeUrlAndCheckRedirectToGoodHttps(GetURLWithoutScheme(url), histograms,
                                     target_url);

  histograms.ExpectTotalCount(TypedNavigationUpgradeThrottle::kHistogramName,
                              6);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadStarted, 2);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadSucceeded, 2);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kRedirected, 2);
}

// Same as UrlTypedWithoutScheme_GoodHttps_Redirected, but this time the
// redirect target is a broken HTTPS page. Should fall back to the HTTP URL
// which will redirect to the broken HTTPS page and show an interstitial
IN_PROC_BROWSER_TEST_P(
    TypedNavigationUpgradeThrottleRedirectBrowserTest,
    UrlTypedWithoutScheme_BadHttps_Redirected_ShouldFallback) {
  if (!IsFeatureEnabled()) {
    return;
  }
  SetUpMockCertVerifierWithErrorForHttpsServer(kSiteWithBadHttps);

  const GURL target_url =
      https_server()->GetURL(kSiteWithBadHttps, "/title1.html");
  const GURL url = embedded_test_server()->GetURL(
      kSiteWithGoodHttpsRedirect, "/server-redirect?" + target_url.spec());
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  base::HistogramTester histograms;
  TypeUrlAndCheckRedirectToBadHttps(GetURLWithoutScheme(url), histograms,
                                    target_url);
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(contents));

  histograms.ExpectTotalCount(TypedNavigationUpgradeThrottle::kHistogramName,
                              3);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadStarted, 1);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadFailedWithCertError, 1);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kRedirected, 1);

  // The SSL error is recorded twice even though the interstitial is only shown
  // once. The error is encountered first at the end of the upgraded HTTPS
  // navigation, and then at the end of the fallback.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectTotalCount(kNetErrorHistogram, 0);

  // Try again, histogram numbers should double.
  TypeUrlAndCheckRedirectToBadHttps(GetURLWithoutScheme(url), histograms,
                                    target_url);
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(contents));

  histograms.ExpectTotalCount(TypedNavigationUpgradeThrottle::kHistogramName,
                              6);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadStarted, 2);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadFailedWithCertError, 2);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kRedirected, 2);

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 4);
  histograms.ExpectTotalCount(kNetErrorHistogram, 0);

  // Regression test for crbug.com/1182760: Try again, this time using the
  // target URL. This should show an interstitial instead of a net error.
  const std::string url_without_scheme = GetURLWithoutScheme(target_url);
  SetOmniboxText(url_without_scheme);
  PressEnterAndWaitForNavigations(/*num_expected_navigations=*/1);
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(contents));
  ui_test_utils::HistoryEnumerator enumerator(browser()->profile());
  const GURL http_url = MakeHttpURL(url_without_scheme);
  const GURL https_url = MakeHttpsURL(url_without_scheme);
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());
  // Since the navigation results in an interstitial, https_url isn't added to
  // history.
  EXPECT_FALSE(base::Contains(enumerator.urls(), https_url));
  EXPECT_FALSE(base::Contains(enumerator.urls(), http_url));
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 6);
  histograms.ExpectTotalCount(kNetErrorHistogram, 0);
  // Throttle histogram numbers shouldn't change:
  histograms.ExpectTotalCount(TypedNavigationUpgradeThrottle::kHistogramName,
                              6);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadStarted, 2);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadFailedWithCertError, 2);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kRedirected, 2);
}

// Same as UrlTypedWithoutScheme_BadHttps_Redirected_ShouldFallback, but the
// redirect ends up on a net error instead of an SSL error.
IN_PROC_BROWSER_TEST_P(
    TypedNavigationUpgradeThrottleRedirectBrowserTest,
    UrlTypedWithoutScheme_NetError_Redirected_ShouldFallback) {
  if (!IsFeatureEnabled()) {
    return;
  }

  const GURL target_url =
      https_server()->GetURL(kSiteWithNetError, "/title1.html");
  const GURL url = embedded_test_server()->GetURL(
      kSiteWithGoodHttpsRedirect, "/server-redirect?" + target_url.spec());
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  base::HistogramTester histograms;
  TypeUrlAndCheckRedirectToBadHttps(GetURLWithoutScheme(url), histograms,
                                    target_url);
  ASSERT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));

  histograms.ExpectTotalCount(TypedNavigationUpgradeThrottle::kHistogramName,
                              3);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadStarted, 1);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadFailedWithNetError, 1);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kRedirected, 1);

  // // The first navigation never shows the interstitial or the net error page.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 0);
  histograms.ExpectTotalCount(kNetErrorHistogram, 0);

  // Try again, histogram numbers should double.
  TypeUrlAndCheckRedirectToBadHttps(GetURLWithoutScheme(url), histograms,
                                    target_url);
  ASSERT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));

  histograms.ExpectTotalCount(TypedNavigationUpgradeThrottle::kHistogramName,
                              6);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadStarted, 2);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kHttpsLoadFailedWithNetError, 2);
  histograms.ExpectBucketCount(
      TypedNavigationUpgradeThrottle::kHistogramName,
      TypedNavigationUpgradeThrottle::Event::kRedirected, 2);

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 0);
  // TODO(meacer): This should record 1 instead of zero. However, simulating the
  // net error page with a URLLoaderInterceptor doesn't work, probably because
  // of the reasons outlined in https://crbug.com/1168371.
  histograms.ExpectTotalCount(kNetErrorHistogram, 0);
}

// TODO(crbug.com/1141691): Test the following cases:
// - Various types of omnibox entries (URLs typed with a port, URLs in history,
// non-unique URLs such as machine.local, IP addresses etc.
// - Redirects (either in the upgraded HTTPS navigation or in the fallback)
// - Various types of navigation states such as downloads, external protocols
// etc.
// - Non-cert errors such as HTTP 4XX or 5XX.
// - Test cases for crbug.com/1161620.
