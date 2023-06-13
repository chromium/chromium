// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/ssl/https_only_mode_navigation_throttle.h"
#include "chrome/browser/ssl/https_only_mode_upgrade_interceptor.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/security_interstitials/core/https_only_mode_metrics.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/hashing.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/url_constants.h"

using security_interstitials::https_only_mode::Event;
using security_interstitials::https_only_mode::kEventHistogram;

// Tests for the v1 implementation of HTTPS-First Mode. See
// https_upgrade_browsertest.cc for the tests for v2.
class HttpsOnlyModeBrowserTest : public InProcessBrowserTest {
 public:
  HttpsOnlyModeBrowserTest() = default;
  ~HttpsOnlyModeBrowserTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kHttpsOnlyMode},
        /*disabled_features=*/{features::kHttpsFirstModeV2,
                               features::kHttpsUpgrades});
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // By default allow all hosts on HTTPS.
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    host_resolver()->AddRule("*", "127.0.0.1");

    // Set up "bad-https.test" as a hostname with an SSL error. HTTPS upgrades
    // to this host will fail.
    scoped_refptr<net::X509Certificate> cert(https_server_.GetCertificate());
    net::CertVerifyResult verify_result;
    verify_result.is_issued_by_known_root = false;
    verify_result.verified_cert = cert;
    verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;
    mock_cert_verifier_.mock_cert_verifier()->AddResultForCertAndHost(
        cert, "bad-https.test", verify_result,
        net::ERR_CERT_COMMON_NAME_INVALID);

    http_server_.AddDefaultHandlers(GetChromeTestDataDir());
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(http_server_.Start());
    ASSERT_TRUE(https_server_.Start());

    HttpsOnlyModeUpgradeInterceptor::SetHttpsPortForTesting(
        https_server()->port());
    HttpsOnlyModeUpgradeInterceptor::SetHttpPortForTesting(
        http_server()->port());

    SetPref(true);
  }

  void TearDownOnMainThread() override { SetPref(false); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

 protected:
  void SetPref(bool enabled) {
    auto* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(prefs::kHttpsOnlyModeEnabled, enabled);
  }

  bool GetPref() const {
    auto* prefs = browser()->profile()->GetPrefs();
    return prefs->GetBoolean(prefs::kHttpsOnlyModeEnabled);
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

  net::EmbeddedTestServer* http_server() { return &http_server_; }
  net::EmbeddedTestServer* https_server() { return &https_server_; }
  base::HistogramTester* histograms() { return &histograms_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer http_server_{net::EmbeddedTestServer::TYPE_HTTP};
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  content::ContentMockCertVerifier mock_cert_verifier_;
  base::HistogramTester histograms_;
};

// If the user navigates to an HTTP URL for a site that supports HTTPS, the
// navigation should end up on the HTTPS version of the URL.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
                       UrlWithHttpScheme_ShouldUpgrade) {
  GURL http_url = http_server()->GetURL("foo.test", "/simple.html");
  GURL https_url = https_server()->GetURL("foo.test", "/simple.html");

  // The NavigateToURL() call returns `false` because the navigation is
  // redirected to HTTPS.
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(contents, 1);
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  nav_observer.Wait();

  EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));

  // Verify that navigation event metrics were correctly recorded.
  histograms()->ExpectTotalCount(kEventHistogram, 2);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeSucceeded, 1);
}

// If the user navigates to an HTTPS URL for a site that supports HTTPS, the
// navigation should end up on that exact URL.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
                       UrlWithHttpsScheme_ShouldLoad) {
  GURL https_url = https_server()->GetURL("foo.test", "/simple.html");
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::NavigateToURL(contents, https_url));

  // Verify that navigation event metrics were not recorded as the navigation
  // was not upgraded.
  histograms()->ExpectTotalCount(kEventHistogram, 0);
}

// If the user navigates to a localhost URL, the navigation should end up on
// that exact URL.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest, Localhost_ShouldNotUpgrade) {
  GURL localhost_url = http_server()->GetURL("localhost", "/simple.html");
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::NavigateToURL(contents, localhost_url));

  // Verify that navigation event metrics were not recorded as the navigation
  // was not upgraded.
  histograms()->ExpectTotalCount(kEventHistogram, 0);
}

// If the user navigates to an HTTPS URL, the navigation should end up on that
// exact URL, even if the site has an SSL error.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
                       UrlWithHttpsScheme_BrokenSSL_ShouldNotFallback) {
  GURL https_url = https_server()->GetURL("bad-https.test", "/simple.html");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(contents, https_url));
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(contents));

  // Verify that navigation event metrics were not recorded as the navigation
  // was not upgraded.
  histograms()->ExpectTotalCount(kEventHistogram, 0);
}

// If the user navigates to an HTTP URL for a site with broken HTTPS, the
// navigation should end up on the HTTPS URL and show the HTTPS-Only Mode
// interstitial.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
                       UrlWithHttpScheme_BrokenSSL_ShouldInterstitial) {
  GURL http_url = http_server()->GetURL("bad-https.test", "/simple.html");
  GURL https_url = https_server()->GetURL("bad-https.test", "/simple.html");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());

  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));

  // Verify that navigation event metrics were correctly recorded.
  histograms()->ExpectTotalCount(kEventHistogram, 3);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeFailed, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeCertError, 1);
}

// If the user triggers an HTTPS-Only Mode interstitial for a host and then
// clicks through the interstitial, they should end up on the HTTP URL.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
                       InterstitialBypassed_HttpFallbackLoaded) {
  GURL http_url = http_server()->GetURL("bad-https.test", "/simple.html");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));

  // Proceed through the interstitial, which will add the host to the allowlist
  // and navigate to the HTTP fallback URL.
  ProceedThroughInterstitial(contents);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  // Verify that navigation event metrics were correctly recorded.
  histograms()->ExpectTotalCount(kEventHistogram, 3);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeFailed, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeCertError, 1);

  // Verify that the interstitial metrics were correctly recorded.
  histograms()->ExpectTotalCount("interstitial.https_first_mode.decision", 2);
  histograms()->ExpectBucketCount(
      "interstitial.https_first_mode.decision",
      security_interstitials::MetricsHelper::Decision::SHOW, 1);
  histograms()->ExpectBucketCount(
      "interstitial.https_first_mode.decision",
      security_interstitials::MetricsHelper::Decision::PROCEED, 1);
}

// If the upgraded HTTPS URL is not available due to a net error, it should
// trigger the HTTPS-Only Mode interstitial and offer fallback.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
                       NetErrorOnUpgrade_ShouldInterstitial) {
  GURL http_url = http_server()->GetURL("foo.test", "/close-socket");
  GURL https_url = https_server()->GetURL("foo.test", "/close-socket");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());

  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));

  // Verify that navigation event metrics were correctly recorded.
  histograms()->ExpectTotalCount(kEventHistogram, 3);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeFailed, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeNetError, 1);
}

// Navigations in subframes should not get upgraded by HTTPS-Only Mode. They
// should be blocked as mixed content.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
                       HttpsParentHttpSubframeNavigation_Blocked) {
  const GURL parent_url(
      https_server()->GetURL("foo.test", "/iframe_blank.html"));
  const GURL iframe_url(http_server()->GetURL("foo.test", "/simple.html"));

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
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
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
                       HttpParentHttpSubframeNavigation_NotUpgraded) {
  // The parent frame will fail to upgrade to HTTPS.
  const GURL parent_url(
      http_server()->GetURL("bad-https.test", "/iframe_blank.html"));
  const GURL iframe_url(http_server()->GetURL("bar.test", "/simple.html"));

  // Navigate to `parent_url` and bypass the HTTPS-Only Mode warning.
  // TODO(crbug.com/1218526): Update this to act on the interstitial once it is
  // added.
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(contents, parent_url));

  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));
  // Proceeding through the interstitial will add the hostname to the allowlist.
  ProceedThroughInterstitial(contents);

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
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest, SlowHttps_ShouldInterstitial) {
  // Set timeout to zero so that HTTPS upgrades immediately timeout.
  HttpsOnlyModeNavigationThrottle::set_timeout_for_testing(0);

  const GURL url = http_server()->GetURL("foo.test", "/hung");
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(contents, url));

  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));
}

// Tests that an HTTP POST form navigation to "bar.test" from an HTTP page on
// "foo.test" is not upgraded to HTTPS. (HTTP form navigations from HTTPS are
// blocked by the Mixed Forms warning.)
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest, HttpPageHttpPost_NotUpgraded) {
  // Point the HTTP form target to "bar.test".
  base::StringPairs replacement_text;
  replacement_text.push_back(make_pair(
      "REPLACE_WITH_HOST_AND_PORT",
      net::HostPortPair::FromURL(http_server()->GetURL("foo.test", "/"))
          .ToString()));
  auto replacement_path = net::test_server::GetFilePathWithReplacements(
      "/ssl/page_with_form_targeting_http_url.html", replacement_text);

  // Navigate to the page hosting the form on "foo.test". The HTTPS-Only Mode
  // interstitial should trigger.
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(
      contents, http_server()->GetURL("bad-https.test", replacement_path)));
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));

  // Proceed through the interstitial to add the hostname to the allowlist.
  ProceedThroughInterstitial(contents);

  // Verify that navigation event metrics were recorded for the initial page.
  histograms()->ExpectTotalCount(kEventHistogram, 3);

  // Submit the form and wait for the navigation to complete.
  content::TestNavigationObserver nav_observer(contents, 1);
  ASSERT_TRUE(
      content::ExecJs(contents, "document.getElementById('submit').click();"));
  nav_observer.Wait();

  // Check that the navigation has ended up on the HTTP target.
  EXPECT_EQ("foo.test", contents->GetLastCommittedURL().host());
  EXPECT_TRUE(contents->GetLastCommittedURL().SchemeIs(url::kHttpScheme));

  // Verify that no new navigation event metrics were recorded for the POST
  // navigation.
  histograms()->ExpectTotalCount(kEventHistogram, 3);
}

// Tests that if an HTTPS navigation redirects to HTTP on a different host, it
// should upgrade to HTTPS on that new host. (A downgrade redirect on the same
// host would imply a redirect loop.)
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
                       HttpsToHttpRedirect_ShouldUpgrade) {
  GURL target_url = http_server()->GetURL("bar.test", "/title1.html");
  GURL url = https_server()->GetURL("foo.test",
                                    "/server-redirect?" + target_url.spec());

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();

  // NavigateToURL() returns `false` because the final redirected URL does not
  // match `url`. Separately ensure the navigation succeeded using a navigation
  // observer.
  content::TestNavigationObserver nav_observer(contents, 1);
  EXPECT_FALSE(content::NavigateToURL(contents, url));
  nav_observer.Wait();
  EXPECT_TRUE(nav_observer.last_navigation_succeeded());

  EXPECT_TRUE(contents->GetLastCommittedURL().SchemeIs(url::kHttpsScheme));
  EXPECT_EQ("bar.test", contents->GetLastCommittedURL().host());

  // Verify that navigation event metrics were correctly recorded.
  histograms()->ExpectTotalCount(kEventHistogram, 2);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeSucceeded, 1);
}

// Tests that navigating to an HTTPS page that downgrades to HTTP on the same
// host will fail and trigger the HTTPS-Only Mode interstitial (due to the
// redirect loop hitting the redirect limit).
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
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
        http_downgrade.SetHostStr("foo.test");
        auto redirect_url = request.GetURL().ReplaceComponents(http_downgrade);
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(net::HTTP_TEMPORARY_REDIRECT);
        response->AddCustomHeader("Location", redirect_url.spec());
        return response;
      }));
  ASSERT_TRUE(downgrading_server.Start());
  HttpsOnlyModeUpgradeInterceptor::SetHttpsPortForTesting(
      downgrading_server.port());

  GURL url = downgrading_server.GetURL("foo.test", "/");
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(contents, url));
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));

  // Verify that navigation event metrics were correctly recorded.
  histograms()->ExpectTotalCount(kEventHistogram, 3);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeFailed, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeNetError, 1);
}

// Tests that (if no testing port is specified), the upgraded HTTPS version of
// an HTTP navigation will use the default HTTPS port 443.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest, HttpsUpgrade_DefaultPort) {
  // Unset the custom testing port so that the redirect uses the default
  // behavior of clearing the port.
  HttpsOnlyModeUpgradeInterceptor::SetHttpsPortForTesting(0);

  // Explicitly create an HTTP URL with the default port 80 specified.
  GURL http_url = GURL("http://foo.test:80/");

  // Navigate to the HTTP URL, which will get redirected to HTTPS/443.
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(contents, 1);
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  nav_observer.Wait();

  // Upgraded navigation should fail to connect as no test server is listening
  // on port 443. The HTTPS-Only Mode interstitial should be showing.
  EXPECT_FALSE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(url::kHttpsScheme, contents->GetLastCommittedURL().scheme());
  EXPECT_EQ(443, contents->GetLastCommittedURL().EffectiveIntPort());

  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));
}

// Tests that (if no testing port is specified), the upgraded HTTPS version of
// an HTTP navigation with a non-default port will retain that non-default port.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest, HttpsUpgrade_NonDefaultPort) {
  // Unset the custom testing port so that the redirect uses the default
  // behavior of clearing the port.
  HttpsOnlyModeUpgradeInterceptor::SetHttpsPortForTesting(0);

  // Explicitly create an HTTP URL with a non-default port 8000 specified.
  GURL http_url = GURL("http://foo.test:8000/");

  // Navigate to the HTTP URL, which will get redirected to HTTPS/443.
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(contents, 1);
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  nav_observer.Wait();

  // Upgraded navigation should fail to connect as no test server is listening
  // on port 8000. The HTTPS-Only Mode interstitial should be showing.
  EXPECT_FALSE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(url::kHttpsScheme, contents->GetLastCommittedURL().scheme());
  EXPECT_EQ(8000, contents->GetLastCommittedURL().EffectiveIntPort());

  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));
}

// Tests that the security level is WARNING when the HTTPS-Only Mode
// interstitial is shown for a net error on HTTPS. (Without HTTPS-Only Mode, a
// net error would be a security level of NONE.)
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
                       NetErrorOnUpgrade_SecurityLevelWarning) {
  GURL http_url = http_server()->GetURL("foo.test", "/close-socket");
  GURL https_url = https_server()->GetURL("foo.test", "/close-socket");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());

  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));

  auto* helper = SecurityStateTabHelper::FromWebContents(contents);
  EXPECT_EQ(security_state::WARNING, helper->GetSecurityLevel());

  // Proceed through the interstitial to navigate to the HTTP site. The HTTP
  // site results in a net error, which should have security level NONE (as no
  // connection was made).
  ProceedThroughInterstitial(contents);
  EXPECT_EQ(security_state::NONE, helper->GetSecurityLevel());
}

// Tests that the security level is WARNING when the HTTPS-Only Mode
// interstitial is shown for a cert error on HTTPS. (Without HTTPS-Only Mode, a
// a cert error would be a security level of DANGEROUS.) After clicking through
// the interstitial, the security level should still be WARNING.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
                       BrokenSSLOnUpgrade_SecurityLevelWarning) {
  GURL http_url = http_server()->GetURL("bad-https.test", "/simple.html");
  GURL https_url = https_server()->GetURL("bad-https.test", "/simple.html");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());

  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));

  auto* helper = SecurityStateTabHelper::FromWebContents(contents);
  EXPECT_EQ(security_state::WARNING, helper->GetSecurityLevel());

  // Proceed through the interstitial to navigate to the HTTP page. The security
  // level should still be WARNING.
  ProceedThroughInterstitial(contents);
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
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
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
            "https://bad-https.test:" +
                base::NumberToString(
                    HttpsOnlyModeUpgradeInterceptor::GetHttpsPortForTesting()) +
                "/simple.html");
        return response;
      }));
  HttpsOnlyModeUpgradeInterceptor::SetHttpPortForTesting(
      upgrading_server.port());
  ASSERT_TRUE(upgrading_server.Start());

  GURL http_url = upgrading_server.GetURL("bad-https.test", "/simple.html");
  // HTTPS server will have a cert error.
  GURL https_url = https_server()->GetURL("bad-https.test", "/simple.html");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());

  // The HTTPS-First Mode interstitial should trigger first.
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));

  // Proceeding through the HTTPS-First Mode interstitial will hit the upgrading
  // server's HTTP->HTTPS redirect. This should result in an SSL interstitial
  // (not an HTTPS-First Mode interstitial).
  ProceedThroughInterstitial(contents);
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

  // Verify that the interstitial metrics were correctly recorded.
  histograms()->ExpectBucketCount(
      "interstitial.https_first_mode.decision",
      security_interstitials::MetricsHelper::Decision::SHOW, 1);
  histograms()->ExpectBucketCount(
      "interstitial.https_first_mode.decision",
      security_interstitials::MetricsHelper::Decision::PROCEED, 1);
}

// Tests that clicking the "Learn More" link in the HTTPS-First Mode
// interstitial opens a new tab for the help center article.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest, InterstitialLearnMoreLink) {
  GURL http_url = http_server()->GetURL("foo.test", "/close-socket");
  GURL https_url = https_server()->GetURL("foo.test", "/close-socket");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());

  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));

  // Simulate clicking the learn more link (CMD_OPEN_HELP_CENTER).
  ASSERT_TRUE(content::ExecJs(
      contents, "window.certificateErrorPageController.openHelpCenter();"));

  // New tab should include the p-link "first_mode".
  EXPECT_EQ(browser()
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
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest, BadHttpsFollowedByGoodHttps) {
  GURL http_url = http_server()->GetURL("foo.test", "/close-socket");
  GURL bad_https_url = https_server()->GetURL("foo.test", "/close-socket");
  GURL good_https_url = https_server()->GetURL("foo.test", "/ssl/google.html");

  ASSERT_EQ(http_url.host(), bad_https_url.host());
  ASSERT_EQ(bad_https_url.host(), good_https_url.host());

  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  auto* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  auto* state = static_cast<StatefulSSLHostStateDelegate*>(
      profile->GetSSLHostStateDelegate());

  // First check that main frame requests revoke the decision.

  // Navigate to `http_url`, which will get upgraded to `bad_https_url`.
  EXPECT_FALSE(content::NavigateToURL(tab, http_url));

  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(tab));
  ProceedThroughInterstitial(tab);
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
  EXPECT_FALSE(content::NavigateToURL(tab, http_url));

  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(tab));
  ProceedThroughInterstitial(tab);
  EXPECT_TRUE(state->HasAllowException(
      http_url.host(), tab->GetPrimaryMainFrame()->GetStoragePartition()));

  // Load "logo.gif" as an image on the page.
  GURL image = https_server()->GetURL("foo.test", "/ssl/google_files/logo.gif");
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
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest, InterstitialGoBack) {
  GURL http_url = http_server()->GetURL("foo.test", "/close-socket");
  GURL https_url = https_server()->GetURL("foo.test", "/close-socket");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());

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
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest, CloseInterstitialTab) {
  GURL http_url = http_server()->GetURL("foo.test", "/close-socket");
  GURL https_url = https_server()->GetURL("foo.test", "/close-socket");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());

  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));

  // Leave the interstitial by closing the tab.
  chrome::CloseWebContents(browser(), contents, false);

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
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest, AllowlistEntryExpires) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
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
  GURL http_url = http_server()->GetURL("bad-https.test", "/simple.html");
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));
  ProceedThroughInterstitial(contents);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_TRUE(state->IsHttpAllowedForHost(
      http_url.host(), contents->GetPrimaryMainFrame()->GetStoragePartition()));

  // Simulate the clock advancing by eight days, which is past the expiration
  // point.
  clock_ptr->Advance(base::Days(8));

  // The host should no longer be allowlisted, and the interstitial should
  // trigger again.
  EXPECT_FALSE(state->IsHttpAllowedForHost(
      http_url.host(), contents->GetPrimaryMainFrame()->GetStoragePartition()));
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));
}

// Tests that re-visiting an allowlisted host bumps the expiration time to a new
// seven days in the future from now.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest, RevisitingBumpsExpiration) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
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
  GURL http_url = http_server()->GetURL("bad-https.test", "/simple.html");
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));
  ProceedThroughInterstitial(contents);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_TRUE(state->IsHttpAllowedForHost(
      http_url.host(), contents->GetPrimaryMainFrame()->GetStoragePartition()));

  // Simulate the clock advancing by five days.
  clock_ptr->Advance(base::Days(5));

  // Navigate to the host again; this will reset the allowlist expiration to
  // now + 7 days.
  EXPECT_TRUE(content::NavigateToURL(contents, http_url));

  // Simulate the clock advancing another five days. This will be _after_ the
  // initial expiration date of the allowlist entry, but _before_ the bumped
  // expiration date from the second navigation.
  clock_ptr->Advance(base::Days(5));
  EXPECT_TRUE(content::NavigateToURL(contents, http_url));
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));
}

// Tests that if a hostname has an HSTS entry registered, then HTTPS-First Mode
// should not try to upgrade it (instead allowing HSTS to handle the upgrade as
// it is more strict).
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest, PreferHstsOverHttpsFirstMode) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());

  // URL for HTTPS server that will result in a certificate error.
  GURL https_url = https_server()->GetURL("bad-https.test", "/simple.html");

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
}

// Tests that if the HttpAllowlist enterprise policy is set, then HTTPS upgrades
// are skipped for hosts in the allowlist.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
                       EnterpriseAllowlistDisablesUpgrades) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Without any policy allowlist, navigate to HTTP URL on foo.test. It *should*
  // get upgraded to HTTPS.
  auto http_url = http_server()->GetURL("foo.test", "/simple.html");
  auto https_url = https_server()->GetURL("foo.test", "/simple.html");
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());

  // Artificially add the pref that gets mapped from the enterprise policy.
  auto* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  auto* prefs = profile->GetPrefs();
  base::Value::List allowlist;
  allowlist.Append("foo.test");
  allowlist.Append("[*.]bar.test");
  allowlist.Append(http_server()->GetIPLiteralString());
  // These cases should not work, but the policy->pref mapping won't immediately
  // reject them.
  allowlist.Append("[*]");
  allowlist.Append("*");
  prefs->SetList(prefs::kHttpAllowlist, std::move(allowlist));

  // Navigate to HTTP URL on foo.test. It should not get upgraded to HTTPS and
  // no interstitial should be shown.
  http_url = http_server()->GetURL("foo.test", "/simple.html");
  https_url = https_server()->GetURL("foo.test", "/simple.html");
  EXPECT_TRUE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));

  // Navigate to HTTP URL on bar.test. Same result.
  http_url = http_server()->GetURL("bar.test", "/simple.html");
  https_url = https_server()->GetURL("bar.test", "/simple.html");
  EXPECT_TRUE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));

  // Navigate to HTTP URL on bar.bar.test. Same result as subdomain wildcard
  // was specified.
  http_url = http_server()->GetURL("bar.bar.test", "/simple.html");
  https_url = https_server()->GetURL("bar.bar.test", "/simple.html");
  EXPECT_TRUE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));

  // Navigate to HTTP URL on foo.foo.test. Subdomains of foo.test should not be
  // considered as being in the allowlist as no wildcard was specified. This
  // should get upgraded to HTTPS.
  http_url = http_server()->GetURL("foo.foo.test", "/simple.html");
  https_url = https_server()->GetURL("foo.foo.test", "/simple.html");
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());

  // Navigate to HTTP URL on baz.test, which is not on the allowlist. Should get
  // upgraded to HTTPS.
  http_url = http_server()->GetURL("baz.test", "/simple.html");
  https_url = https_server()->GetURL("baz.test", "/simple.html");
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());

  // Navigate to HTTP URL on the HTTP test server's IP address. It should not
  // get upgraded ot HTTPS and no interstitial should be shown.
  http_url = http_server()->GetURL("/simple.html");
  https_url = https_server()->GetURL("/simple.html");
  EXPECT_TRUE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));
}

// Tests that if HTTPS-First Mode is disabled, metrics are recorded on
// upgrade-eligible navigations.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
                       MetricsRecordedWhenHFMDisabled) {
  GURL http_url = http_server()->GetURL("foo.test", "/simple.html");

  // Ensure HTTPS-First Mode is off.
  SetPref(false);

  // NavigateToURL() returns `true` because the navigation is not redirected.
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::NavigateToURL(contents, http_url));

  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));

  // Verify that navigation event metrics were correctly recorded.
  histograms()->ExpectTotalCount(kEventHistogram, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeNotAttempted,
                                  1);
}

// A simple test fixture that ensures the kHttpsOnlyMode feature is enabled and
// constructs a HistogramTester (so that it gets initialized before browser
// startup). Used for testing pref tracking logic.
class HttpsOnlyModePrefsBrowserTest : public InProcessBrowserTest {
 public:
  HttpsOnlyModePrefsBrowserTest() = default;
  ~HttpsOnlyModePrefsBrowserTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kHttpsOnlyMode);
    InProcessBrowserTest::SetUp();
  }

 protected:
  void SetPref(bool enabled) {
    auto* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(prefs::kHttpsOnlyModeEnabled, enabled);
  }

  bool GetPref() const {
    auto* prefs = browser()->profile()->GetPrefs();
    return prefs->GetBoolean(prefs::kHttpsOnlyModeEnabled);
  }

  base::HistogramTester* histograms() { return &histograms_; }

  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histograms_;
};

// Tests that the HTTPS-First Mode pref is recorded at startup and when changed.
// This test requires restarting the browser to test the "at startup" metric in
// order for the preference state to be set up before the HttpsFirstModeService
// is created.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModePrefsBrowserTest, PRE_PrefStatesRecorded) {
  // The default pref state is `false`, which should get recorded when the
  // initial browser instance is started here.
  histograms()->ExpectUniqueSample(
      "Security.HttpsFirstMode.SettingEnabledAtStartup", false, 1);

  EXPECT_TRUE(variations::IsInSyntheticTrialGroup("HttpsFirstModeClientSetting",
                                                  "Disabled"));

  // Change the pref to true. This should get recorded in the histogram.
  SetPref(true);
  histograms()->ExpectUniqueSample("Security.HttpsFirstMode.SettingChanged",
                                   true, 1);
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup("HttpsFirstModeClientSetting",
                                                  "Enabled"));
}

IN_PROC_BROWSER_TEST_F(HttpsOnlyModePrefsBrowserTest, PrefStatesRecorded) {
  // Restarting the browser from the PRE_ test should record the startup pref
  // histogram. Checking the unique count also ensures that other profile types
  // (e.g. the ChromeOS sign-in profile) don't cause double-counting.
  EXPECT_TRUE(GetPref());
  histograms()->ExpectUniqueSample(
      "Security.HttpsFirstMode.SettingEnabledAtStartup", true, 1);
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup("HttpsFirstModeClientSetting",
                                                  "Enabled"));

  // Open an Incognito window. Startup metrics should not get recorded.
  CreateIncognitoBrowser();
  histograms()->ExpectTotalCount(
      "Security.HttpsFirstMode.SettingEnabledAtStartup", 1);
}

// Tests for enabling HTTPS-Only Mode for Advanced Protection users.
class HttpsOnlyModeForAdvancedProtectionBrowserTest
    : public HttpsOnlyModePrefsBrowserTest,
      public testing::WithParamInterface<
          bool /* is_enabled_for_advanced_protection */> {
  void SetUp() override {
    if (is_enabled_for_advanced_protection()) {
      feature_list()->InitWithFeatures(
          {features::kHttpsOnlyMode,
           features::kHttpsFirstModeForAdvancedProtectionUsers},
          {});
    } else {
      feature_list()->InitWithFeatures(
          {features::kHttpsOnlyMode},
          {features::kHttpsFirstModeForAdvancedProtectionUsers});
    }
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    http_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(http_server_.Start());
  }

 protected:
  bool is_enabled_for_advanced_protection() const { return GetParam(); }

  net::EmbeddedTestServer* http_server() { return &http_server_; }

 private:
  net::EmbeddedTestServer http_server_{net::EmbeddedTestServer::TYPE_HTTP};
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HttpsOnlyModeForAdvancedProtectionBrowserTest,
    testing::Bool() /* is_enabled_for_advanced_protection */);

// Tests that HFM is enabled if the user is under Advanced Protection.
IN_PROC_BROWSER_TEST_P(HttpsOnlyModeForAdvancedProtectionBrowserTest,
                       AdvancedProtectionEnabled) {
  safe_browsing::AdvancedProtectionStatusManager* ap_manager =
      safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
          browser()->profile());

  EXPECT_FALSE(ap_manager->IsUnderAdvancedProtection());
  EXPECT_FALSE(GetPref());

  GURL http_url = http_server()->GetURL("foo.test", "/simple.html");
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();

  ap_manager->SetAdvancedProtectionStatusForTesting(true);
  if (is_enabled_for_advanced_protection()) {
    EXPECT_TRUE(GetPref());

    EXPECT_FALSE(content::NavigateToURL(contents, http_url));
    EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(contents));
    EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
        contents->GetPrimaryMainFrame(), "Advanced Protection"));
  } else {
    EXPECT_FALSE(GetPref());

    EXPECT_TRUE(content::NavigateToURL(contents, http_url));
    EXPECT_FALSE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  }
}

class HttpsOnlyModeTestSubresourceNotifications
    : public HttpsOnlyModeBrowserTest {
 public:
  HttpsOnlyModeTestSubresourceNotifications() = default;
  ~HttpsOnlyModeTestSubresourceNotifications() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kHttpsOnlyMode,
                              features::kReduceSubresourceResponseStartedIPC},
        /*disabled_features=*/{features::kHttpsFirstModeV2,
                               features::kHttpsUpgrades});
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that if the user bypasses the HTTPS-First Mode interstitial, and then
// later the server fixes their HTTPS support and the user successfully connects
// over HTTPS, the allowlist entry is cleared. Once
// `renderer_preferences_.send_subresource_notification_` is set, we keep on
// sending subresource notification until we have cleared all exceptions and the
// browser is restarted.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeTestSubresourceNotifications,
                       PRE_BadHttpsFollowedByGoodHttpsFollowedByRestart) {
  GURL http_url = http_server()->GetURL("foo.test", "/close-socket");
  GURL bad_https_url = https_server()->GetURL("foo.test", "/close-socket");
  GURL good_https_url = https_server()->GetURL("foo.test", "/ssl/google.html");

  ASSERT_EQ(http_url.host(), bad_https_url.host());
  ASSERT_EQ(bad_https_url.host(), good_https_url.host());

  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  auto* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  auto* state = static_cast<StatefulSSLHostStateDelegate*>(
      profile->GetSSLHostStateDelegate());

  ASSERT_FALSE(tab->GetSendSubresourceNotification());

  // Main frame requests revoke the decision.

  // Navigate to `http_url`, which will get upgraded to `bad_https_url`.
  EXPECT_FALSE(content::NavigateToURL(tab, http_url));

  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(tab));
  ProceedThroughInterstitial(tab);

  EXPECT_TRUE(state->HasAllowExceptionForAnyHost(
      tab->GetPrimaryMainFrame()->GetStoragePartition()));
  EXPECT_TRUE(tab->GetSendSubresourceNotification());

  EXPECT_TRUE(content::NavigateToURL(tab, good_https_url));
  EXPECT_FALSE(state->HasAllowExceptionForAnyHost(
      tab->GetPrimaryMainFrame()->GetStoragePartition()));
  // Revoking decisions does not change `send_subresource_notifications_` in the
  // same browsing session. This is because allowing and revoking exceptions
  // happens rarely. So we continue sending subresource notifications to the
  // browser.
  EXPECT_TRUE(tab->GetSendSubresourceNotification());
}

IN_PROC_BROWSER_TEST_F(HttpsOnlyModeTestSubresourceNotifications,
                       BadHttpsFollowedByGoodHttpsFollowedByRestart) {
  // Verifies `renderer_preferences_.send_subresource_notification_` is updated
  // w.r.t. the state of allowed exceptions when the browser restarts.
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  auto* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  auto* state = static_cast<StatefulSSLHostStateDelegate*>(
      profile->GetSSLHostStateDelegate());

  EXPECT_FALSE(state->HasAllowExceptionForAnyHost(
      tab->GetPrimaryMainFrame()->GetStoragePartition()));
  EXPECT_FALSE(tab->GetSendSubresourceNotification());
}
