// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ssl/connection_help_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verify_result.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

class ConnectionHelpTabHelperTestBase : public InProcessBrowserTest {
 public:
  ConnectionHelpTabHelperTestBase()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        https_expired_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    https_expired_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    https_expired_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
    ASSERT_TRUE(https_expired_server_.Start());
  }

 protected:
  void SetHelpCenterUrl(Browser* browser, const GURL& url) {
    ConnectionHelpTabHelper::FromWebContents(
        browser->tab_strip_model()->GetActiveWebContents())
        ->SetHelpCenterUrlForTesting(url);
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  net::EmbeddedTestServer* https_expired_server() {
    return &https_expired_server_;
  }

 private:
  net::EmbeddedTestServer https_server_;
  net::EmbeddedTestServer https_expired_server_;
  DISALLOW_COPY_AND_ASSIGN(ConnectionHelpTabHelperTestBase);
};

class ConnectionHelpTabHelperTest : public ConnectionHelpTabHelperTestBase {
 public:
  ConnectionHelpTabHelperTest() {
    feature_list_.InitAndEnableFeature(features::kBundledConnectionHelpFeature);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class ConnectionHelpTabHelperTestWithFeatureDisabled
    : public ConnectionHelpTabHelperTestBase {
 public:
  ConnectionHelpTabHelperTestWithFeatureDisabled() {
    feature_list_.InitAndDisableFeature(
        features::kBundledConnectionHelpFeature);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the chrome://connection-help redirect is not triggered (and
// metrics are not logged) for an interstitial on a site that is not the help
// center.
IN_PROC_BROWSER_TEST_F(ConnectionHelpTabHelperTest,
                       InterstitialOnNonSupportURL) {
  const char kHistogramName[] = "SSL.CertificateErrorHelpCenterVisited";
  base::HistogramTester histograms;

  GURL expired_non_support_url = https_expired_server()->GetURL("/title2.html");
  GURL good_support_url = https_server()->GetURL("/title2.html");
  SetHelpCenterUrl(browser(), good_support_url);
  ui_test_utils::NavigateToURL(browser(), expired_non_support_url);

  base::string16 tab_title;
  ui_test_utils::GetCurrentTabTitle(browser(), &tab_title);
  EXPECT_EQ(base::UTF16ToUTF8(tab_title), "Privacy error");

  histograms.ExpectTotalCount(kHistogramName, 0);
}

// Tests that the chrome://connection-help redirect is not triggered (and
// metrics are logged) for the help center URL if there was no interstitial.
IN_PROC_BROWSER_TEST_F(ConnectionHelpTabHelperTest,
                       SupportURLWithNoInterstitial) {
  const char kHistogramName[] = "SSL.CertificateErrorHelpCenterVisited";
  base::HistogramTester histograms;

  GURL good_support_url = https_server()->GetURL("/title2.html");
  SetHelpCenterUrl(browser(), good_support_url);
  ui_test_utils::NavigateToURL(browser(), good_support_url);

  base::string16 tab_title;
  ui_test_utils::GetCurrentTabTitle(browser(), &tab_title);
  EXPECT_EQ(base::UTF16ToUTF8(tab_title), "Title Of Awesomeness");

  histograms.ExpectUniqueSample(
      kHistogramName, ConnectionHelpTabHelper::LearnMoreClickResult::kSucceeded,
      1);
}

// Tests that the chrome://connection-help redirect is triggered (and metrics
// are logged) for the help center URL if there was an interstitial.
IN_PROC_BROWSER_TEST_F(ConnectionHelpTabHelperTest, InterstitialOnSupportURL) {
  const char kHistogramName[] = "SSL.CertificateErrorHelpCenterVisited";
  base::HistogramTester histograms;

  GURL expired_url = https_expired_server()->GetURL("/title2.html");
  SetHelpCenterUrl(browser(), expired_url);

  ui_test_utils::NavigateToURL(browser(), expired_url);

  base::string16 tab_title;
  ui_test_utils::GetCurrentTabTitle(browser(), &tab_title);
  EXPECT_EQ(base::UTF16ToUTF8(tab_title),
            l10n_util::GetStringUTF8(IDS_CONNECTION_HELP_TITLE));

  histograms.ExpectUniqueSample(
      kHistogramName,
      ConnectionHelpTabHelper::LearnMoreClickResult::kFailedWithInterstitial,
      1);
}

// Tests that histogram logs correctly when an interstitial is triggered on the
// support URL if the feature is disabled.
IN_PROC_BROWSER_TEST_F(ConnectionHelpTabHelperTestWithFeatureDisabled,
                       InterstitialOnSupportURL) {
  const char kHistogramName[] = "SSL.CertificateErrorHelpCenterVisited";
  base::HistogramTester histograms;

  GURL expired_url = https_expired_server()->GetURL("/title2.html");
  SetHelpCenterUrl(browser(), expired_url);
  ui_test_utils::NavigateToURL(browser(), expired_url);

  base::string16 tab_title;
  ui_test_utils::GetCurrentTabTitle(browser(), &tab_title);
  EXPECT_EQ(base::UTF16ToUTF8(tab_title), "Privacy error");

  histograms.ExpectUniqueSample(
      kHistogramName,
      ConnectionHelpTabHelper::LearnMoreClickResult::kFailedWithInterstitial,
      1);
}

// Tests that a non-interstitial error on the support URL is logged correctly,
// by setting the support URL to an invalid URL and attempting to navigate to
// it.
IN_PROC_BROWSER_TEST_F(ConnectionHelpTabHelperTest, NetworkErrorOnSupportURL) {
  const char kHistogramName[] = "SSL.CertificateErrorHelpCenterVisited";
  base::HistogramTester histograms;
  GURL invalid_url("http://invalid-url.test");
  SetHelpCenterUrl(browser(), invalid_url);
  ui_test_utils::NavigateToURL(browser(), invalid_url);
  histograms.ExpectUniqueSample(
      kHistogramName,
      ConnectionHelpTabHelper::LearnMoreClickResult::kFailedOther, 1);
}

// Tests that if the help content site is opened with an error code that refers
// to a certificate error, the certificate error section is automatically
// expanded.
IN_PROC_BROWSER_TEST_F(ConnectionHelpTabHelperTest,
                       CorrectlyExpandsCertErrorSection) {
  GURL expired_url = https_expired_server()->GetURL("/title2.html#-200");
  GURL::Replacements replacements;
  replacements.ClearRef();
  SetHelpCenterUrl(browser(), expired_url.ReplaceComponents(replacements));

  ui_test_utils::NavigateToURL(browser(), expired_url);

  // Check that we got redirected to the offline help content.
  base::string16 tab_title;
  ui_test_utils::GetCurrentTabTitle(browser(), &tab_title);
  EXPECT_EQ(base::UTF16ToUTF8(tab_title),
            l10n_util::GetStringUTF8(IDS_CONNECTION_HELP_TITLE));

  // Check that the cert error details section is not hidden.
  std::string cert_error_is_hidden_js =
      "var certSection = document.getElementById('details-certerror'); "
      "window.domAutomationController.send(certSection.className == "
      "'hidden');";
  bool cert_error_is_hidden;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      cert_error_is_hidden_js, &cert_error_is_hidden));
  EXPECT_FALSE(cert_error_is_hidden);
}

// Tests that if the help content site is opened with an error code that refers
// to an expired certificate, the clock section is automatically expanded.
IN_PROC_BROWSER_TEST_F(ConnectionHelpTabHelperTest,
                       CorrectlyExpandsClockSection) {
  GURL expired_url = https_expired_server()->GetURL("/title2.html#-201");
  GURL::Replacements replacements;
  replacements.ClearRef();
  SetHelpCenterUrl(browser(), expired_url.ReplaceComponents(replacements));

  ui_test_utils::NavigateToURL(browser(), expired_url);

  // Check that we got redirected to the offline help content.
  base::string16 tab_title;
  ui_test_utils::GetCurrentTabTitle(browser(), &tab_title);
  EXPECT_EQ(base::UTF16ToUTF8(tab_title),
            l10n_util::GetStringUTF8(IDS_CONNECTION_HELP_TITLE));

  // Check that the clock details section is not hidden.
  std::string clock_is_hidden_js =
      "var clockSection = document.getElementById('details-clock');  "
      "window.domAutomationController.send(clockSection.className == "
      "'hidden');";
  bool clock_is_hidden;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(), clock_is_hidden_js,
      &clock_is_hidden));
  EXPECT_FALSE(clock_is_hidden);
}
