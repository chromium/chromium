// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/https_only_mode_navigation_throttle.h"
#include "chrome/browser/ssl/https_only_mode_upgrade_interceptor.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
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
#include "ui/base/l10n/l10n_util.h"
#include "url/url_constants.h"

class HttpsOnlyModeBrowserTest : public InProcessBrowserTest {
 public:
  HttpsOnlyModeBrowserTest() = default;
  ~HttpsOnlyModeBrowserTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kHttpsOnlyMode);
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
    ASSERT_TRUE(content::ExecuteScript(tab, javascript));
    nav_observer.Wait();
  }

  void DontProceedThroughInterstitial(content::WebContents* tab) {
    content::TestNavigationObserver nav_observer(tab, 1);
    std::string javascript =
        "window.certificateErrorPageController.dontProceed();";
    ASSERT_TRUE(content::ExecuteScript(tab, javascript));
    nav_observer.Wait();
  }

  net::EmbeddedTestServer* http_server() { return &http_server_; }
  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer http_server_{net::EmbeddedTestServer::TYPE_HTTP};
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  content::ContentMockCertVerifier mock_cert_verifier_;
};

// If the user navigates to an HTTP URL for a site that supports HTTPS, the
// navigation should end up on the HTTPS version of the URL.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
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
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));
}

// If the user navigates to an HTTPS URL for a site that supports HTTPS, the
// navigation should end up on that exact URL.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
                       UrlWithHttpsScheme_ShouldLoad) {
  GURL https_url = https_server()->GetURL("foo.com", "/simple.html");
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::NavigateToURL(contents, https_url));
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

  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      contents->GetMainFrame(),
      l10n_util::GetStringUTF8(IDS_HTTPS_ONLY_MODE_PRIMARY_PARAGRAPH)));
}

// If the user triggers an HTTPS-Only Mode interstitial for a host and then
// clicks through the interstitial, they should end up on the HTTP URL.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
                       InterstitialBypassed_HttpFallbackLoaded) {
  GURL http_url = http_server()->GetURL("bad-https.test", "/simple.html");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      contents->GetMainFrame(),
      l10n_util::GetStringUTF8(IDS_HTTPS_ONLY_MODE_PRIMARY_PARAGRAPH)));

  // Proceed through the interstitial, which will add the host to the allowlist
  // and navigate to the HTTP fallback URL.
  ProceedThroughInterstitial(contents);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
}

// If the upgraded HTTPS URL is not available due to a net error, it should
// trigger the HTTPS-Only Mode interstitial and offer fallback.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
                       NetErrorOnUpgrade_ShouldInterstitial) {
  GURL http_url = http_server()->GetURL("foo.com", "/close-socket");
  GURL https_url = https_server()->GetURL("foo.com", "/close-socket");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());

  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      contents->GetMainFrame(),
      l10n_util::GetStringUTF8(IDS_HTTPS_ONLY_MODE_PRIMARY_PARAGRAPH)));
}

// Navigations in subframes should not get upgraded by HTTPS-Only Mode. They
// should be blocked as mixed content.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
                       HttpsParentHttpSubframeNavigation_Blocked) {
  const GURL parent_url(
      https_server()->GetURL("foo.com", "/iframe_blank.html"));
  const GURL iframe_url(http_server()->GetURL("foo.com", "/simple.html"));

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::NavigateToURL(contents, parent_url));

  content::TestNavigationObserver nav_observer(contents, 1);
  EXPECT_TRUE(content::NavigateIframeToURL(contents, "test", iframe_url));
  nav_observer.Wait();
  EXPECT_NE(iframe_url, nav_observer.last_navigation_url());
}

// Navigating to an HTTP URL in a subframe of an HTTP page should not upgrade
// the subframe navigation to HTTPS (even if the subframe navigation is to a
// different host than the parent frame).
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
                       HttpParentHttpSubframeNavigation_NotUpgraded) {
  // The parent frame will fail to upgrade to HTTPS.
  const GURL parent_url(
      http_server()->GetURL("bad-https.test", "/iframe_blank.html"));
  const GURL iframe_url(http_server()->GetURL("bar.com", "/simple.html"));

  // Navigate to `parent_url` and bypass the HTTPS-Only Mode warning.
  // TODO(crbug.com/1218526): Update this to act on the interstitial once it is
  // added.
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(contents, parent_url));

  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      contents->GetMainFrame(),
      l10n_util::GetStringUTF8(IDS_HTTPS_ONLY_MODE_PRIMARY_PARAGRAPH)));
  // Proceeding through the interstitial will add the hostname to the allowlist.
  ProceedThroughInterstitial(contents);

  // Navigate the iframe to `iframe_url`. It should successfully navigate and
  // not get upgraded to HTTPS as the hostname is now in the allowlist.
  content::TestNavigationObserver nav_observer(contents, 1);
  EXPECT_TRUE(content::NavigateIframeToURL(contents, "test", iframe_url));
  nav_observer.Wait();
  EXPECT_EQ(iframe_url, nav_observer.last_navigation_url());
}

// Tests that a navigation to the HTTP version of a site with an HTTPS version
// that is slow to respond gets upgraded to HTTPS but times out and shows the
// HTTPS-Only Mode interstitial.
// TODO(crbug.com/1218526): Re-enable once fast-timeout is working.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
                       DISABLED_SlowHttps_ShouldInterstitial) {
  // Set timeout to zero so that HTTPS upgrades immediately timeout.
  HttpsOnlyModeNavigationThrottle::set_timeout_for_testing(0);

  const GURL url = http_server()->GetURL("foo.com", "/hung");
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(contents, url));

  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      contents->GetMainFrame(),
      l10n_util::GetStringUTF8(IDS_HTTPS_ONLY_MODE_PRIMARY_PARAGRAPH)));
}

// Tests that an HTTP POST form navigation to "bar.com" from an HTTP page on
// "foo.com" is not upgraded to HTTPS. (HTTP form navigations from HTTPS are
// blocked by the Mixed Forms warning.)
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest, HttpPageHttpPost_NotUpgraded) {
  // Point the HTTP form target to "bar.com".
  base::StringPairs replacement_text;
  replacement_text.push_back(make_pair(
      "REPLACE_WITH_HOST_AND_PORT",
      net::HostPortPair::FromURL(http_server()->GetURL("foo.com", "/"))
          .ToString()));
  auto replacement_path = net::test_server::GetFilePathWithReplacements(
      "/ssl/page_with_form_targeting_http_url.html", replacement_text);

  // Navigate to the page hosting the form on "foo.com". The HTTPS-Only Mode
  // interstitial should trigger.
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(
      contents, http_server()->GetURL("bad-https.test", replacement_path)));
  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      contents->GetMainFrame(),
      l10n_util::GetStringUTF8(IDS_HTTPS_ONLY_MODE_PRIMARY_PARAGRAPH)));

  // Proceed through the interstitial to add the hostname to the allowlist.
  ProceedThroughInterstitial(contents);

  // Submit the form and wait for the navigation to complete.
  content::TestNavigationObserver nav_observer(contents, 1);
  ASSERT_TRUE(content::ExecuteScript(
      contents, "document.getElementById('submit').click();"));
  nav_observer.Wait();

  // Check that the navigation has ended up on the HTTP target.
  EXPECT_EQ("foo.com", contents->GetLastCommittedURL().host());
  EXPECT_TRUE(contents->GetLastCommittedURL().SchemeIs(url::kHttpScheme));
}

// Tests that if an HTTPS navigation redirects to HTTP on a different host, it
// should upgrade to HTTPS on that new host. (A downgrade redirect on the same
// host would imply a redirect loop.)
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
                       HttpsToHttpRedirect_ShouldUpgrade) {
  GURL target_url = http_server()->GetURL("bar.com", "/title1.html");
  GURL url = https_server()->GetURL("foo.com",
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
  EXPECT_EQ("bar.com", contents->GetLastCommittedURL().host());
}

// Tests that navigating to an HTTPS page that downgrades to HTTP on the same
// host will fail and trigger the HTTPS-Only Mode interstitial (due to the
// redirect loop hitting the redirect limit).
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
                       RedirectLoop_ShouldInterstitial) {
  // Set up a new test server instance so it can have a custom handler.
  net::EmbeddedTestServer downgrading_server{
      net::EmbeddedTestServer::TYPE_HTTPS};
  // Downgrade by just swapping the scheme. HTTPS-Only Mode will upgrade it
  // back to HTTPS.
  downgrading_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        GURL::Replacements http_scheme;
        http_scheme.SetSchemeStr(url::kHttpScheme);
        auto redirect_url = request.GetURL().ReplaceComponents(http_scheme);
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(net::HTTP_TEMPORARY_REDIRECT);
        response->AddCustomHeader("Location", redirect_url.spec());
        return response;
      }));
  HttpsOnlyModeUpgradeInterceptor::SetHttpsPortForTesting(
      downgrading_server.port());
  ASSERT_TRUE(downgrading_server.Start());

  GURL url = downgrading_server.GetURL("foo.com", "/");
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(contents, url));
  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      contents->GetMainFrame(),
      l10n_util::GetStringUTF8(IDS_HTTPS_ONLY_MODE_PRIMARY_PARAGRAPH)));
}

// Tests that (if no testing port is specified), the upgraded HTTPS version of
// an HTTP navigation will use the default HTTPS port 443.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest, HttpsUpgrade_DefaultPort) {
  // Unset the custom testing port so that the redirect uses the default
  // behavior of clearing the port.
  HttpsOnlyModeUpgradeInterceptor::SetHttpsPortForTesting(0);

  GURL http_url = http_server()->GetURL("foo.com", "/simple.html");

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

  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      contents->GetMainFrame(),
      l10n_util::GetStringUTF8(IDS_HTTPS_ONLY_MODE_PRIMARY_PARAGRAPH)));
}

// Tests that the security level is WARNING when the HTTPS-Only Mode
// interstitial is shown for a net error on HTTPS. (Without HTTPS-Only Mode, a
// net error would be a security level of NONE.) After clicking through the
// interstitial, the security level should still be WARNING.
IN_PROC_BROWSER_TEST_F(HttpsOnlyModeBrowserTest,
                       NetErrorOnUpgrade_SecurityLevelWarning) {
  GURL http_url = http_server()->GetURL("foo.com", "/close-socket");
  GURL https_url = https_server()->GetURL("foo.com", "/close-socket");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());

  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      contents->GetMainFrame(),
      l10n_util::GetStringUTF8(IDS_HTTPS_ONLY_MODE_PRIMARY_PARAGRAPH)));

  auto* helper = SecurityStateTabHelper::FromWebContents(contents);
  EXPECT_EQ(security_state::WARNING, helper->GetSecurityLevel());

  // Proceed through the interstitial to navigate to the HTTP page. The security
  // level should still be WARNING.
  ProceedThroughInterstitial(contents);
  EXPECT_EQ(security_state::WARNING, helper->GetSecurityLevel());
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

  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      contents->GetMainFrame(),
      l10n_util::GetStringUTF8(IDS_HTTPS_ONLY_MODE_PRIMARY_PARAGRAPH)));

  auto* helper = SecurityStateTabHelper::FromWebContents(contents);
  EXPECT_EQ(security_state::WARNING, helper->GetSecurityLevel());

  // Proceed through the interstitial to navigate to the HTTP page. The security
  // level should still be WARNING.
  ProceedThroughInterstitial(contents);
  EXPECT_EQ(security_state::WARNING, helper->GetSecurityLevel());
}
