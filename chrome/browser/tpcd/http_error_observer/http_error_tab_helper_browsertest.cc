// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/http_error_observer/http_error_tab_helper.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"

constexpr char kHostA[] = "a.test";
constexpr char kHostB[] = "b.test";

namespace {
// Handles Favicon requests so they don't produce a 404 and augment http error
// metrics during a test
std::unique_ptr<net::test_server::HttpResponse> HandleFaviconRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != "/favicon.ico") {
    return nullptr;
  }
  // The response doesn't have to be a valid favicon to avoid logging a
  // console error. Any 200 response will do.
  return std::make_unique<net::test_server::BasicHttpResponse>();
}
}  // namespace

class HTTPErrProcBrowserTest : public InProcessBrowserTest {
 public:
  HTTPErrProcBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    https_server_.RegisterRequestHandler(
        base::BindRepeating(&HandleFaviconRequest));
    ASSERT_TRUE(https_server_.Start());
  }

  // Sets third party cookie blocking in settings
  void SetThirdPartyCookieBlocking(bool enabled) {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            enabled ? content_settings::CookieControlsMode::kBlockThirdParty
                    : content_settings::CookieControlsMode::kOff));
    scoped_refptr<content_settings::CookieSettings> settings =
        CookieSettingsFactory::GetForProfile(browser()->profile());
    ASSERT_EQ(
        settings->ShouldBlockThirdPartyCookies(),
        enabled || base::FeatureList::IsEnabled(
                       content_settings::features::kTrackingProtection3pcd));
  }

  // Checks that the HTTP Error has been properly recorded in the metrics
  void CheckServerErrBreakageMetrics(
      ukm::TestAutoSetUkmRecorder& ukm_recorder,
      size_t size,
      size_t index,
      bool blocked,
      bool settings_blocked,
      const base::Location& location = FROM_HERE) {
    auto entries =
        ukm_recorder.GetEntries("ThirdPartyCookies.BreakageIndicator.HTTPError",
                                {"TPCBlocked", "TPCBlockedInSettings"});
    EXPECT_EQ(entries.size(), size)
        << "(expected at " << location.ToString() << ")";
    EXPECT_EQ(entries.at(index).metrics.at("TPCBlocked"), blocked)
        << "(expected at " << location.ToString() << ")";
    EXPECT_EQ(entries.at(index).metrics.at("TPCBlockedInSettings"),
              settings_blocked)
        << "(expected at " << location.ToString() << ")";
  }

  // Navigates to a page with an iframe, then navigates the iframe to the given
  // GURL. Can also set TPC blocking cookie.
  void NavigateToURLAndIFrame(std::string_view host, const GURL iframe_url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server()->GetURL(host, "/iframe.html")));
    ASSERT_TRUE(NavigateIframeToURL(
        browser()->tab_strip_model()->GetActiveWebContents(), "test",
        iframe_url));
  }

 private:
  net::EmbeddedTestServer https_server_;
};

// Verify that Metric only registers HTTP errors
IN_PROC_BROWSER_TEST_F(HTTPErrProcBrowserTest, NoErr) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SetThirdPartyCookieBlocking(false);
  NavigateToURLAndIFrame(
      /*host=*/kHostA,
      /*iframe_url=*/https_server()->GetURL(kHostA, "/title1.html"));
  EXPECT_EQ(ukm_recorder
                .GetEntries("ThirdPartyCookies.BreakageIndicator.HTTPError",
                            {"TPCBlocked", "TPCBlockedInSettings"})
                .size(),
            0u);
}

// Check that the ThirdPartyCookieBreakageIndicator UKM is sent on HTTP Error
// without cookies blocked
IN_PROC_BROWSER_TEST_F(HTTPErrProcBrowserTest, WithCookiesWithErr) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetThirdPartyCookieSetting(
          https_server()->GetURL(kHostB, "/page404.html"),
          CONTENT_SETTING_ALLOW);
  NavigateToURLAndIFrame(
      /*host=*/kHostA,
      /*iframe_url=*/https_server()->GetURL(kHostB, "/page404.html"));
  CheckServerErrBreakageMetrics(
      /*ukm_recorder=*/ukm_recorder,
      /*size=*/1,
      /*index=*/0,
      /*blocked=*/false,
      /*settings_blocked=*/false);
}

// Check that the ThirdPartyCookieBreakageIndicator UKM is sent on HTTP Error
// with cookies blocked in settings
IN_PROC_BROWSER_TEST_F(HTTPErrProcBrowserTest, TPCBlockedInSettings4xxErr) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SetThirdPartyCookieBlocking(true);
  NavigateToURLAndIFrame(
      /*host=*/kHostA,
      /*iframe_url=*/https_server()->GetURL(kHostB, "/page404.html"));
  CheckServerErrBreakageMetrics(
      /*ukm_recorder=*/ukm_recorder,
      /*size=*/1,
      /*index=*/0,
      /*blocked=*/false,
      /*settings_blocked=*/true);
}

// Check that the ThirdPartyCookieBreakageIndicator UKM works with image
// subresources
IN_PROC_BROWSER_TEST_F(HTTPErrProcBrowserTest, TPCBlockedImageErr) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SetThirdPartyCookieBlocking(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(kHostA, "/no_real_image.html")));
  CheckServerErrBreakageMetrics(
      /*ukm_recorder=*/ukm_recorder,
      /*size=*/1,
      /*index=*/0,
      /*blocked=*/false,
      /*settings_blocked=*/true);
}

// Check that the ThirdPartyCookieBreakageIndicator UKM works with fetches
// TODO(crbug.com/40938887): Fix flakiness and re-enable.
IN_PROC_BROWSER_TEST_F(HTTPErrProcBrowserTest, DISABLED_TPCBlockedFetchErr) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SetThirdPartyCookieBlocking(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(kHostA, "/no_real_fetch.html")));
  CheckServerErrBreakageMetrics(
      /*ukm_recorder=*/ukm_recorder,
      /*size=*/1,
      /*index=*/0,
      /*blocked=*/false,
      /*settings_blocked=*/true);
}

// Check that ThirdPartyCookieBreakageIndicator UKM works with subresource loads
// within iframes
IN_PROC_BROWSER_TEST_F(HTTPErrProcBrowserTest, TPCBlockedIframeErr) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SetThirdPartyCookieBlocking(true);
  content::SetCookie(browser()->profile(),
                     https_server()->GetURL(kHostB, "/no_real_image.html"),
                     "thirdparty=1;SameSite=None;Secure");
  NavigateToURLAndIFrame(
      /*host=*/kHostA,
      /*iframe_url=*/https_server()->GetURL(kHostB, "/no_real_image.html"));
  CheckServerErrBreakageMetrics(
      /*ukm_recorder=*/ukm_recorder,
      /*size=*/1,
      /*index=*/0,
      /*blocked=*/true,
      /*settings_blocked=*/true);
}

// Check that the ThirdPartyCookieBreakageIndicator UKM is sent on Server Error.
IN_PROC_BROWSER_TEST_F(HTTPErrProcBrowserTest, TPCBlocked5xxErr) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SetThirdPartyCookieBlocking(true);
  content::SetCookie(
      browser()->profile(),
      https_server()->GetURL(kHostB, "/echo-cookie-with-status?status=500"),
      "thirdparty=1;SameSite=None;Secure");
  NavigateToURLAndIFrame(
      /*host=*/kHostA,
      /*iframe_url=*/
      https_server()->GetURL(kHostB, "/echo-cookie-with-status?status=500"));
  CheckServerErrBreakageMetrics(
      /*ukm_recorder=*/ukm_recorder,
      /*size=*/1,
      /*index=*/0,
      /*blocked=*/true,
      /*settings_blocked=*/true);
}

// Verify that ThirdPartyCookieBreakageIndicator UKM has correct value
// when TPC are blocked in settings and by site
IN_PROC_BROWSER_TEST_F(HTTPErrProcBrowserTest, TPCBlocked4xxErr) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SetThirdPartyCookieBlocking(true);
  content::SetCookie(browser()->profile(),
                     https_server()->GetURL(kHostB, "/not-real.html"),
                     "thirdparty=1;SameSite=None;Secure");
  NavigateToURLAndIFrame(
      /*host=*/kHostA,
      /*iframe_url=*/https_server()->GetURL(kHostB, "/not-real.html"));

  CheckServerErrBreakageMetrics(
      /*ukm_recorder=*/ukm_recorder,
      /*size=*/1,
      /*index=*/0,
      /*blocked=*/true,
      /*settings_blocked=*/true);
}

// Check that multiple entries are entered correctly.
// TODO(crbug.com/40287588): Fix flakiness and re-enable.
IN_PROC_BROWSER_TEST_F(HTTPErrProcBrowserTest, DISABLED_MultiErrs) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SetThirdPartyCookieBlocking(true);
  content::SetCookie(browser()->profile(),
                     https_server()->GetURL(kHostB, "/page404.html"),
                     "thirdparty=1;SameSite=None;Secure");
  NavigateToURLAndIFrame(
      /*host=*/kHostA,
      /*iframe_url=*/https_server()->GetURL(kHostB, "/page404.html"));
  CheckServerErrBreakageMetrics(
      /*ukm_recorder=*/ukm_recorder,
      /*size=*/1,
      /*index=*/0,
      /*blocked=*/true,
      /*settings_blocked=*/true);

  SetThirdPartyCookieBlocking(false);
  NavigateToURLAndIFrame(
      /*host=*/kHostA,
      /*iframe_url=*/https_server()->GetURL(kHostA, "/not-real.html"));
  CheckServerErrBreakageMetrics(
      /*ukm_recorder=*/ukm_recorder,
      /*size=*/2,
      /*index=*/1,
      /*blocked=*/false,
      /*settings_blocked=*/false);
}
