// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/captive_portal_blocking_page.h"

#include <string>
#include <utility>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service_test_utils.h"
#include "chrome/browser/ssl/cert_report_helper.h"
#include "chrome/browser/ssl/certificate_reporting_test_utils.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ssl/ssl_cert_reporter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/captive_portal/captive_portal_detector.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "components/variations/variations_params_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using chrome_browser_interstitials::IsInterstitialDisplayingText;
using chrome_browser_interstitials::SecurityInterstitialIDNTest;

namespace {
// Partial text in the captive portal interstitial's main paragraph when the
// login domain isn't displayed.
const char kGenericLoginURLText[] = "its login page";
const char kBrokenSSL[] = "https://broken.ssl";
const char kWiFiSSID[] = "WiFiSSID";

enum ExpectWiFi {
  EXPECT_WIFI_NO,
  EXPECT_WIFI_YES
};

enum ExpectWiFiSSID {
  EXPECT_WIFI_SSID_NO,
  EXPECT_WIFI_SSID_YES
};

enum ExpectLoginURL {
  EXPECT_LOGIN_URL_NO,
  EXPECT_LOGIN_URL_YES
};

bool AreCommittedInterstitialsEnabled() {
  return base::FeatureList::IsEnabled(features::kSSLCommittedInterstitials);
}

class CaptivePortalBlockingPageForTesting : public CaptivePortalBlockingPage {
 public:
  CaptivePortalBlockingPageForTesting(
      content::WebContents* web_contents,
      const GURL& request_url,
      const GURL& login_url,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
      const net::SSLInfo& ssl_info,
      const base::Callback<void(content::CertificateRequestResultType)>&
          callback,
      bool is_wifi,
      const std::string& wifi_ssid)
      : CaptivePortalBlockingPage(web_contents,
                                  request_url,
                                  login_url,
                                  std::move(ssl_cert_reporter),
                                  ssl_info,
                                  net::ERR_CERT_COMMON_NAME_INVALID,
                                  callback),
        is_wifi_(is_wifi),
        wifi_ssid_(wifi_ssid) {}

 private:
  bool IsWifiConnection() const override { return is_wifi_; }
  std::string GetWiFiSSID() const override { return wifi_ssid_; }
  const bool is_wifi_;
  const std::string wifi_ssid_;
};

// A NavigationThrottle that observes failed requests and shows a captive portal
// interstitial, either transient or committed depending on whether committed
// interstitials are enabled.
class CaptivePortalTestingNavigationThrottle
    : public content::NavigationThrottle {
 public:
  CaptivePortalTestingNavigationThrottle(
      content::NavigationHandle* handle,
      const GURL& login_url,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
      bool is_wifi_connection,
      const std::string& wifi_ssid);
  ~CaptivePortalTestingNavigationThrottle() override {}

  // content::NavigationThrottle:
  const char* GetNameForLogging() override {
    return "CaptivePortalTestingNavigationThrottle";
  }

 private:
  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillFailRequest() override;

  const GURL login_url_;
  std::unique_ptr<SSLCertReporter> ssl_cert_reporter_;
  bool is_wifi_connection_;
  std::string wifi_ssid_;
};

CaptivePortalTestingNavigationThrottle::CaptivePortalTestingNavigationThrottle(
    content::NavigationHandle* handle,
    const GURL& login_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    bool is_wifi_connection,
    const std::string& wifi_ssid)
    : content::NavigationThrottle(handle),
      login_url_(login_url),
      ssl_cert_reporter_(std::move(ssl_cert_reporter)),
      is_wifi_connection_(is_wifi_connection),
      wifi_ssid_(wifi_ssid) {}

content::NavigationThrottle::ThrottleCheckResult
CaptivePortalTestingNavigationThrottle::WillFailRequest() {
  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;
  CaptivePortalBlockingPage* blocking_page =
      new CaptivePortalBlockingPageForTesting(
          navigation_handle()->GetWebContents(), GURL(kBrokenSSL), login_url_,
          std::move(ssl_cert_reporter_), ssl_info,
          base::Callback<void(content::CertificateRequestResultType)>(),
          is_wifi_connection_, wifi_ssid_);

  if (AreCommittedInterstitialsEnabled()) {
    std::string html = blocking_page->GetHTMLContents();
    // Hand the blocking page back to the WebContents's
    // security_interstitials::SecurityInterstitialTabHelper to own.
    security_interstitials::SecurityInterstitialTabHelper::
        AssociateBlockingPage(
            navigation_handle()->GetWebContents(),
            navigation_handle()->GetNavigationId(),
            std::unique_ptr<CaptivePortalBlockingPage>(blocking_page));
    return {CANCEL, net::ERR_CERT_COMMON_NAME_INVALID, html};
  }

  // |blocking_page| is owned by the interstitial.
  blocking_page->Show();
  WaitForInterstitialAttach(navigation_handle()->GetWebContents());
  EXPECT_TRUE(WaitForRenderFrameReady(navigation_handle()
                                          ->GetWebContents()
                                          ->GetInterstitialPage()
                                          ->GetMainFrame()));
  // When committed interstitials are disabled, defer the navigation while the
  // interstitial is showing.
  return DEFER;
}

// A WebContentsObserver which installs a navigation throttle that creates
// CaptivePortalBlockingPages.
class TestingThrottleInstaller : public content::WebContentsObserver {
 public:
  TestingThrottleInstaller(content::WebContents* web_contents,
                           const GURL& login_url,
                           std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
                           bool is_wifi_connection,
                           const std::string& wifi_ssid);
  ~TestingThrottleInstaller() override {}

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  const GURL login_url_;
  std::unique_ptr<SSLCertReporter> ssl_cert_reporter_;
  bool is_wifi_connection_;
  std::string wifi_ssid_;
};

TestingThrottleInstaller::TestingThrottleInstaller(
    content::WebContents* web_contents,
    const GURL& login_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    bool is_wifi_connection,
    const std::string& wifi_ssid)
    : content::WebContentsObserver(web_contents),
      login_url_(login_url),
      ssl_cert_reporter_(std::move(ssl_cert_reporter)),
      is_wifi_connection_(is_wifi_connection),
      wifi_ssid_(wifi_ssid) {}

void TestingThrottleInstaller::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  navigation_handle->RegisterThrottleForTesting(
      std::make_unique<CaptivePortalTestingNavigationThrottle>(
          navigation_handle, login_url_, std::move(ssl_cert_reporter_),
          is_wifi_connection_, wifi_ssid_));
}

void AddURLRequestFilterOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  net::URLRequestFailedJob::AddUrlHandler();
}

}  // namespace

class CaptivePortalBlockingPageTest : public InProcessBrowserTest,
                                      public testing::WithParamInterface<bool> {
 public:
  CaptivePortalBlockingPageTest() {
    CertReportHelper::SetFakeOfficialBuildForTesting();
  }

  void SetUpOnMainThread() override {
    base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::IO},
                             base::BindOnce(&AddURLRequestFilterOnIOThread));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Setting the sending threshold to 1.0 ensures reporting is enabled.
    variations::testing::VariationParamsManager::AppendVariationParams(
        "ReportCertificateErrors", "ShowAndPossiblySend",
        {{"sendingThreshold", "1.0"}}, command_line);

    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kSSLCommittedInterstitials);
    }
  }

  void TestInterstitial(bool is_wifi_connection,
                        const std::string& wifi_ssid,
                        const GURL& login_url,
                        ExpectWiFi expect_wifi,
                        ExpectWiFiSSID expect_wifi_ssid,
                        ExpectLoginURL expect_login_url,
                        std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
                        const std::string& expected_login_hostname);

  void TestInterstitial(bool is_wifi_connection,
                        const std::string& wifi_ssid,
                        const GURL& login_url,
                        ExpectWiFi expect_wifi,
                        ExpectWiFiSSID expect_wifi_ssid,
                        ExpectLoginURL expect_login_url);

  void TestInterstitial(bool is_wifi_connection,
                        const std::string& wifi_ssid,
                        const GURL& login_url,
                        ExpectWiFi expect_wifi,
                        ExpectWiFiSSID expect_wifi_ssid,
                        ExpectLoginURL expect_login_url,
                        std::unique_ptr<SSLCertReporter> ssl_cert_reporter);

  void TestCertReporting(certificate_reporting_test_utils::OptIn opt_in);

 private:
  std::unique_ptr<TestingThrottleInstaller> testing_throttle_installer_;
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(CaptivePortalBlockingPageTest);
};

INSTANTIATE_TEST_CASE_P(,
                        CaptivePortalBlockingPageTest,
                        ::testing::Values(false, true));

void CaptivePortalBlockingPageTest::TestInterstitial(
    bool is_wifi_connection,
    const std::string& wifi_ssid,
    const GURL& login_url,
    ExpectWiFi expect_wifi,
    ExpectWiFiSSID expect_wifi_ssid,
    ExpectLoginURL expect_login_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    const std::string& expected_login_hostname) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DCHECK(contents);

  testing_throttle_installer_.reset(new TestingThrottleInstaller(
      contents, login_url, std::move(ssl_cert_reporter), is_wifi_connection,
      wifi_ssid));
  ui_test_utils::NavigateToURL(browser(),
                               net::URLRequestFailedJob::GetMockHttpsUrl(
                                   net::ERR_CERT_COMMON_NAME_INVALID));
  content::RenderFrameHost* frame;
  if (!AreCommittedInterstitialsEnabled()) {
    ASSERT_TRUE(contents->GetInterstitialPage());
    frame = contents->GetInterstitialPage()->GetMainFrame();
  } else {
    frame = contents->GetMainFrame();
    ASSERT_TRUE(WaitForRenderFrameReady(frame));
  }

  EXPECT_EQ(expect_wifi == EXPECT_WIFI_YES,
            IsInterstitialDisplayingText(frame, "Wi-Fi"));
  if (!wifi_ssid.empty()) {
    EXPECT_EQ(expect_wifi_ssid == EXPECT_WIFI_SSID_YES,
              IsInterstitialDisplayingText(frame, wifi_ssid));
  }
  EXPECT_EQ(expect_login_url == EXPECT_LOGIN_URL_YES,
            IsInterstitialDisplayingText(frame, expected_login_hostname));
  EXPECT_EQ(expect_login_url == EXPECT_LOGIN_URL_NO,
            IsInterstitialDisplayingText(frame, kGenericLoginURLText));

  // Check that a red/dangerous lock icon is showing on the interstitial. This
  // only occurs when committed interstitials are disabled. With committed
  // interstitials enabled, the NavigationEntry's SSLStatus is updated to
  // reflect a certificate error by the navigation stack, not by the blocking
  // page itself, and that navigation code isn't exercised by this test. (It's
  // covered by other tests in ssl_browsertest.cc).
  if (!AreCommittedInterstitialsEnabled()) {
    SecurityStateTabHelper* helper =
        SecurityStateTabHelper::FromWebContents(contents);
    ASSERT_TRUE(helper);
    security_state::SecurityInfo security_info;
    helper->GetSecurityInfo(&security_info);
    EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);
  }
}

void CaptivePortalBlockingPageTest::TestInterstitial(
    bool is_wifi_connection,
    const std::string& wifi_ssid,
    const GURL& login_url,
    ExpectWiFi expect_wifi,
    ExpectWiFiSSID expect_wifi_ssid,
    ExpectLoginURL expect_login_url) {
  TestInterstitial(is_wifi_connection, wifi_ssid, login_url, expect_wifi,
                   expect_wifi_ssid, expect_login_url, nullptr,
                   login_url.host());
}

void CaptivePortalBlockingPageTest::TestInterstitial(
    bool is_wifi_connection,
    const std::string& wifi_ssid,
    const GURL& login_url,
    ExpectWiFi expect_wifi,
    ExpectWiFiSSID expect_wifi_ssid,
    ExpectLoginURL expect_login_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter) {
  TestInterstitial(is_wifi_connection, wifi_ssid, login_url, expect_wifi,
                   expect_wifi_ssid, expect_login_url,
                   std::move(ssl_cert_reporter), login_url.host());
}

void CaptivePortalBlockingPageTest::TestCertReporting(
    certificate_reporting_test_utils::OptIn opt_in) {
  certificate_reporting_test_utils::SetCertReportingOptIn(browser(), opt_in);
  base::RunLoop run_loop;
  certificate_reporting_test_utils::SSLCertReporterCallback reporter_callback(
      &run_loop);

  std::unique_ptr<SSLCertReporter> ssl_cert_reporter =
      certificate_reporting_test_utils::CreateMockSSLCertReporter(
          base::Bind(&certificate_reporting_test_utils::
                         SSLCertReporterCallback::ReportSent,
                     base::Unretained(&reporter_callback)),
          opt_in == certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN
              ? certificate_reporting_test_utils::CERT_REPORT_EXPECTED
              : certificate_reporting_test_utils::CERT_REPORT_NOT_EXPECTED);

  const GURL kLandingUrl(captive_portal::CaptivePortalDetector::kDefaultURL);
  TestInterstitial(true, std::string(), kLandingUrl, EXPECT_WIFI_YES,
                   EXPECT_WIFI_SSID_NO, EXPECT_LOGIN_URL_NO,
                   std::move(ssl_cert_reporter));

  EXPECT_EQ(std::string(), reporter_callback.GetLatestHostnameReported());

  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);

  if (opt_in == certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN) {
    // Check that the mock reporter received a request to send a report.
    run_loop.Run();
    EXPECT_EQ(GURL(kBrokenSSL).host(),
              reporter_callback.GetLatestHostnameReported());
  } else {
    EXPECT_EQ(std::string(), reporter_callback.GetLatestHostnameReported());
  }
}

// If the connection is not a Wi-Fi connection, the wired network version of the
// captive portal interstitial should be displayed.
IN_PROC_BROWSER_TEST_P(CaptivePortalBlockingPageTest, WiredNetwork_LoginURL) {
  TestInterstitial(false, std::string(),
                   GURL("http://captive.portal/landing_url"), EXPECT_WIFI_NO,
                   EXPECT_WIFI_SSID_NO, EXPECT_LOGIN_URL_YES);
}

// Same as above, but SSID is available, so the connection should be assumed to
// be Wi-Fi.
IN_PROC_BROWSER_TEST_P(CaptivePortalBlockingPageTest,
                       WiredNetwork_LoginURL_With_SSID) {
  TestInterstitial(false, kWiFiSSID, GURL("http://captive.portal/landing_url"),
                   EXPECT_WIFI_YES, EXPECT_WIFI_SSID_YES, EXPECT_LOGIN_URL_YES);
}

// Same as above, expect the login URL is the same as the captive portal ping
// url (i.e. the portal intercepts requests without using HTTP redirects), in
// which case the login URL shouldn't be displayed.
IN_PROC_BROWSER_TEST_P(CaptivePortalBlockingPageTest, WiredNetwork_NoLoginURL) {
  const GURL kLandingUrl(captive_portal::CaptivePortalDetector::kDefaultURL);
  TestInterstitial(false, std::string(), kLandingUrl, EXPECT_WIFI_NO,
                   EXPECT_WIFI_SSID_NO, EXPECT_LOGIN_URL_NO);
}

// Same as above, but SSID is available, so the connection should be assumed to
// be Wi-Fi.
IN_PROC_BROWSER_TEST_P(CaptivePortalBlockingPageTest,
                       WiredNetwork_NoLoginURL_With_SSID) {
  const GURL kLandingUrl(captive_portal::CaptivePortalDetector::kDefaultURL);
  TestInterstitial(false, kWiFiSSID, kLandingUrl, EXPECT_WIFI_YES,
                   EXPECT_WIFI_SSID_YES, EXPECT_LOGIN_URL_NO);
}

// If the connection is a Wi-Fi connection, the Wi-Fi version of the captive
// portal interstitial should be displayed.
IN_PROC_BROWSER_TEST_P(CaptivePortalBlockingPageTest, WiFi_SSID_LoginURL) {
  TestInterstitial(true, kWiFiSSID, GURL("http://captive.portal/landing_url"),
                   EXPECT_WIFI_YES, EXPECT_WIFI_SSID_YES, EXPECT_LOGIN_URL_YES);
}

// Flaky on mac: https://crbug.com/690170
#if defined(OS_MACOSX)
#define MAYBE_WiFi_NoSSID_LoginURL DISABLED_WiFi_NoSSID_LoginURL
#else
#define MAYBE_WiFi_NoSSID_LoginURL WiFi_NoSSID_LoginURL
#endif

// Same as above, with login URL but no SSID.
IN_PROC_BROWSER_TEST_P(CaptivePortalBlockingPageTest,
                       MAYBE_WiFi_NoSSID_LoginURL) {
  TestInterstitial(true, std::string(),
                   GURL("http://captive.portal/landing_url"), EXPECT_WIFI_YES,
                   EXPECT_WIFI_SSID_NO, EXPECT_LOGIN_URL_YES);
}

// Flaky on mac: https://crbug.com/690125
#if defined(OS_MACOSX)
#define MAYBE_WiFi_SSID_NoLoginURL DISABLED_WiFi_SSID_NoLoginURL
#else
#define MAYBE_WiFi_SSID_NoLoginURL WiFi_SSID_NoLoginURL
#endif

// Same as above, with SSID but no login URL.
IN_PROC_BROWSER_TEST_P(CaptivePortalBlockingPageTest,
                       MAYBE_WiFi_SSID_NoLoginURL) {
  const GURL kLandingUrl(captive_portal::CaptivePortalDetector::kDefaultURL);
  TestInterstitial(true, kWiFiSSID, kLandingUrl,
                   EXPECT_WIFI_YES, EXPECT_WIFI_SSID_YES, EXPECT_LOGIN_URL_NO);
}

// Same as above, with no SSID and no login URL.
IN_PROC_BROWSER_TEST_P(CaptivePortalBlockingPageTest, WiFi_NoSSID_NoLoginURL) {
  const GURL kLandingUrl(captive_portal::CaptivePortalDetector::kDefaultURL);
  TestInterstitial(true, std::string(), kLandingUrl, EXPECT_WIFI_YES,
                   EXPECT_WIFI_SSID_NO, EXPECT_LOGIN_URL_NO);
}

IN_PROC_BROWSER_TEST_P(CaptivePortalBlockingPageTest, CertReportingOptIn) {
  TestCertReporting(
      certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);
}

IN_PROC_BROWSER_TEST_P(CaptivePortalBlockingPageTest, CertReportingOptOut) {
  TestCertReporting(
      certificate_reporting_test_utils::EXTENDED_REPORTING_DO_NOT_OPT_IN);
}

class CaptivePortalBlockingPageIDNTest : public SecurityInterstitialIDNTest {
 protected:
  // SecurityInterstitialIDNTest implementation
  security_interstitials::SecurityInterstitialPage* CreateInterstitial(
      content::WebContents* contents,
      const GURL& request_url) const override {
    net::SSLInfo empty_ssl_info;
    // Blocking page is owned by the interstitial.
    CaptivePortalBlockingPage* blocking_page =
        new CaptivePortalBlockingPageForTesting(
            contents, GURL(kBrokenSSL), request_url, nullptr, empty_ssl_info,
            base::Callback<void(content::CertificateRequestResultType)>(),
            false, "");
    return blocking_page;
  }
};

// Test that an IDN login domain is decoded properly.
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageIDNTest,
                       ShowLoginIDNIfPortalRedirectsDetectionURL) {
  EXPECT_TRUE(VerifyIDNDecoded());
}
