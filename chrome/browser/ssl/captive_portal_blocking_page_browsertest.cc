// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/captive_portal_blocking_page.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/interstitials/security_interstitial_idn_test.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/chrome_security_blocking_page_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/captive_portal/core/captive_portal_detector.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "components/variations/variations_params_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
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

// A NavigationThrottle that observes failed requests and shows a captive portal
// interstitial.
class CaptivePortalTestingNavigationThrottle
    : public content::NavigationThrottle {
 public:
  CaptivePortalTestingNavigationThrottle(
      content::NavigationHandle* handle,
      const GURL& login_url,
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
  bool is_wifi_connection_;
  std::string wifi_ssid_;
};

CaptivePortalTestingNavigationThrottle::CaptivePortalTestingNavigationThrottle(
    content::NavigationHandle* handle,
    const GURL& login_url,
    bool is_wifi_connection,
    const std::string& wifi_ssid)
    : content::NavigationThrottle(handle),
      login_url_(login_url),
      is_wifi_connection_(is_wifi_connection),
      wifi_ssid_(wifi_ssid) {}

content::NavigationThrottle::ThrottleCheckResult
CaptivePortalTestingNavigationThrottle::WillFailRequest() {
  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;
  ChromeSecurityBlockingPageFactory blocking_page_factory;
  std::unique_ptr<CaptivePortalBlockingPage> blocking_page =
      blocking_page_factory.CreateCaptivePortalBlockingPage(
          navigation_handle()->GetWebContents(), GURL(kBrokenSSL), login_url_,
          ssl_info, net::ERR_CERT_COMMON_NAME_INVALID);
  blocking_page->OverrideWifiInfoForTesting(is_wifi_connection_, wifi_ssid_);

  std::string html = blocking_page->GetHTMLContents();
  // Hand the blocking page back to the WebContents's
  // security_interstitials::SecurityInterstitialTabHelper to own.
  security_interstitials::SecurityInterstitialTabHelper::AssociateBlockingPage(
      navigation_handle(), std::move(blocking_page));
  return {CANCEL, net::ERR_CERT_COMMON_NAME_INVALID, html};
}

// A WebContentsObserver which installs a navigation throttle that creates
// CaptivePortalBlockingPages.
class TestingThrottleInstaller : public content::WebContentsObserver {
 public:
  TestingThrottleInstaller(content::WebContents* web_contents,
                           const GURL& login_url,
                           bool is_wifi_connection,
                           const std::string& wifi_ssid);
  ~TestingThrottleInstaller() override {}

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  const GURL login_url_;
  bool is_wifi_connection_;
  std::string wifi_ssid_;
};

TestingThrottleInstaller::TestingThrottleInstaller(
    content::WebContents* web_contents,
    const GURL& login_url,
    bool is_wifi_connection,
    const std::string& wifi_ssid)
    : content::WebContentsObserver(web_contents),
      login_url_(login_url),
      is_wifi_connection_(is_wifi_connection),
      wifi_ssid_(wifi_ssid) {}

void TestingThrottleInstaller::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  navigation_handle->RegisterThrottleForTesting(
      std::make_unique<CaptivePortalTestingNavigationThrottle>(
          navigation_handle, login_url_, is_wifi_connection_, wifi_ssid_));
}

}  // namespace

class CaptivePortalBlockingPageTest : public InProcessBrowserTest {
 protected:
  void TestInterstitial(bool is_wifi_connection,
                        const std::string& wifi_ssid,
                        const GURL& login_url,
                        ExpectWiFi expect_wifi,
                        ExpectWiFiSSID expect_wifi_ssid,
                        ExpectLoginURL expect_login_url,
                        const std::string& expected_login_hostname);

  void TestInterstitial(bool is_wifi_connection,
                        const std::string& wifi_ssid,
                        const GURL& login_url,
                        ExpectWiFi expect_wifi,
                        ExpectWiFiSSID expect_wifi_ssid,
                        ExpectLoginURL expect_login_url);

 private:
  std::unique_ptr<TestingThrottleInstaller> testing_throttle_installer_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

void CaptivePortalBlockingPageTest::TestInterstitial(
    bool is_wifi_connection,
    const std::string& wifi_ssid,
    const GURL& login_url,
    ExpectWiFi expect_wifi,
    ExpectWiFiSSID expect_wifi_ssid,
    ExpectLoginURL expect_login_url,
    const std::string& expected_login_hostname) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DCHECK(contents);

  testing_throttle_installer_ = std::make_unique<TestingThrottleInstaller>(
      contents, login_url, is_wifi_connection, wifi_ssid);
  // We cancel the navigation with ERR_BLOCKED_BY_CLIENT so it doesn't get
  // handled by the normal SSLErrorNavigationThrotttle since this test only
  // checks the behavior of the Blocking Page, not the integration with that
  // throttle.
  //
  // TODO(crbug.com/40647477): Clean this code up now that committed
  // interstitials have shipped.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://mock.failed.request/start=-20")));
  content::RenderFrameHost* frame;
  frame = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(WaitForRenderFrameReady(frame));

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
}

void CaptivePortalBlockingPageTest::TestInterstitial(
    bool is_wifi_connection,
    const std::string& wifi_ssid,
    const GURL& login_url,
    ExpectWiFi expect_wifi,
    ExpectWiFiSSID expect_wifi_ssid,
    ExpectLoginURL expect_login_url) {
  TestInterstitial(is_wifi_connection, wifi_ssid, login_url, expect_wifi,
                   expect_wifi_ssid, expect_login_url, login_url.host());
}

// If the connection is not a Wi-Fi connection, the wired network version of the
// captive portal interstitial should be displayed.
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageTest, WiredNetwork_LoginURL) {
  TestInterstitial(false, std::string(),
                   GURL("http://captive.portal/landing_url"), EXPECT_WIFI_NO,
                   EXPECT_WIFI_SSID_NO, EXPECT_LOGIN_URL_YES);
}

// Same as above, but SSID is available, so the connection should be assumed to
// be Wi-Fi.
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageTest,
                       WiredNetwork_LoginURL_With_SSID) {
  TestInterstitial(false, kWiFiSSID, GURL("http://captive.portal/landing_url"),
                   EXPECT_WIFI_YES, EXPECT_WIFI_SSID_YES, EXPECT_LOGIN_URL_YES);
}

// Same as above, expect the login URL is the same as the captive portal ping
// url (i.e. the portal intercepts requests without using HTTP redirects), in
// which case the login URL shouldn't be displayed.
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageTest, WiredNetwork_NoLoginURL) {
  const GURL kLandingUrl(captive_portal::CaptivePortalDetector::kDefaultURL);
  TestInterstitial(false, std::string(), kLandingUrl, EXPECT_WIFI_NO,
                   EXPECT_WIFI_SSID_NO, EXPECT_LOGIN_URL_NO);
}

// Same as above, but SSID is available, so the connection should be assumed to
// be Wi-Fi.
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageTest,
                       WiredNetwork_NoLoginURL_With_SSID) {
  const GURL kLandingUrl(captive_portal::CaptivePortalDetector::kDefaultURL);
  TestInterstitial(false, kWiFiSSID, kLandingUrl, EXPECT_WIFI_YES,
                   EXPECT_WIFI_SSID_YES, EXPECT_LOGIN_URL_NO);
}

// If the connection is a Wi-Fi connection, the Wi-Fi version of the captive
// portal interstitial should be displayed.
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageTest, WiFi_SSID_LoginURL) {
  TestInterstitial(true, kWiFiSSID, GURL("http://captive.portal/landing_url"),
                   EXPECT_WIFI_YES, EXPECT_WIFI_SSID_YES, EXPECT_LOGIN_URL_YES);
}

// Flaky on mac: https://crbug.com/690170
#if BUILDFLAG(IS_MAC)
#define MAYBE_WiFi_NoSSID_LoginURL DISABLED_WiFi_NoSSID_LoginURL
#else
#define MAYBE_WiFi_NoSSID_LoginURL WiFi_NoSSID_LoginURL
#endif

// Same as above, with login URL but no SSID.
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageTest,
                       MAYBE_WiFi_NoSSID_LoginURL) {
  TestInterstitial(true, std::string(),
                   GURL("http://captive.portal/landing_url"), EXPECT_WIFI_YES,
                   EXPECT_WIFI_SSID_NO, EXPECT_LOGIN_URL_YES);
}

// Flaky on mac: https://crbug.com/690125
#if BUILDFLAG(IS_MAC)
#define MAYBE_WiFi_SSID_NoLoginURL DISABLED_WiFi_SSID_NoLoginURL
#else
#define MAYBE_WiFi_SSID_NoLoginURL WiFi_SSID_NoLoginURL
#endif

// Same as above, with SSID but no login URL.
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageTest,
                       MAYBE_WiFi_SSID_NoLoginURL) {
  const GURL kLandingUrl(captive_portal::CaptivePortalDetector::kDefaultURL);
  TestInterstitial(true, kWiFiSSID, kLandingUrl,
                   EXPECT_WIFI_YES, EXPECT_WIFI_SSID_YES, EXPECT_LOGIN_URL_NO);
}

// Same as above, with no SSID and no login URL.
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageTest, WiFi_NoSSID_NoLoginURL) {
  const GURL kLandingUrl(captive_portal::CaptivePortalDetector::kDefaultURL);
  TestInterstitial(true, std::string(), kLandingUrl, EXPECT_WIFI_YES,
                   EXPECT_WIFI_SSID_NO, EXPECT_LOGIN_URL_NO);
}

class CaptivePortalBlockingPageIDNTest : public SecurityInterstitialIDNTest {
 protected:
  // SecurityInterstitialIDNTest implementation
  security_interstitials::SecurityInterstitialPage* CreateInterstitial(
      content::WebContents* contents,
      const GURL& request_url) const override {
    net::SSLInfo empty_ssl_info;
    // Blocking page is owned by the interstitial.
    ChromeSecurityBlockingPageFactory blocking_page_factory;
    std::unique_ptr<CaptivePortalBlockingPage> blocking_page =
        blocking_page_factory.CreateCaptivePortalBlockingPage(
            contents, GURL(kBrokenSSL), request_url, empty_ssl_info,
            net::ERR_CERT_COMMON_NAME_INVALID);
    blocking_page->OverrideWifiInfoForTesting(false, "");
    return blocking_page.release();
  }
};

// Test that an IDN login domain is decoded properly.
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageIDNTest,
                       ShowLoginIDNIfPortalRedirectsDetectionURL) {
  EXPECT_TRUE(VerifyIDNDecoded());
}
