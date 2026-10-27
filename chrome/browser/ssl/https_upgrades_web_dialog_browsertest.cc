// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests that captive portal sign-in dialogs stay exempt from HTTPS-First
// Mode upgrades.
//
// ChromeOS shows its captive portal sign-in pages in a views::WebDialogView
// rather than a browser tab. Captive portals routinely answer the probe request
// with a page that immediately navigates the user onward to the real login page
// over HTTP, so a per-navigation opt-out is not enough: the exemption has to
// apply to the dialog's WebContents for as long as it lives.
//
// chrome::ShowWebDialog() takes a captive_portal::CaptivePortalWindowType for
// that purpose. ChromeWebContentsHandler attaches a CaptivePortalTabHelper with
// that type before the dialog's first navigation, and HttpsUpgradesInterceptor
// exempts every navigation in a WebContents so marked. These tests hold
// everything constant except that window type.

#include <memory>
#include <tuple>

#include "base/memory/raw_ptr.h"
#include "base/strings/escape.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/https_upgrades_interceptor.h"
#include "chrome/browser/ui/dialogs/browser_dialogs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#include "components/captive_portal/core/captive_portal_types.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/ssl_blocking_page.h"
#include "components/security_interstitials/content/ssl_error_handler.h"
#include "components/security_interstitials/core/features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"
#include "ui/web_dialogs/test/test_web_dialog_delegate.h"
#include "url/gurl.h"

namespace {

// A host whose certificate fails verification, so that an HTTPS upgrade
// attempt fails and HTTPS-First Mode falls back to blocking the HTTP
// navigation.
constexpr char kBadHttpsHost[] = "bad-https.com";

// A second such host, standing in for the page a captive portal sends the user
// to after the probe URL. It must be distinct from kBadHttpsHost: allowlisting
// or exempting one host would otherwise mask whether the second navigation was
// handled on its own merits.
constexpr char kPortalLoginHost[] = "portal-login.com";

// Intermediate hosts for a portal that chains several redirects before
// reaching its login page. Also distinct, for the same reason.
constexpr char kPortalHop1Host[] = "portal-hop1.com";
constexpr char kPortalHop2Host[] = "portal-hop2.com";

// Stands in for ash::NetworkPortalSigninController's SigninWebDialogDelegate.
// The delegate itself knows nothing about captive portals; what distinguishes
// a sign-in dialog is the window type its creator passes to ShowWebDialog().
class TestDialogDelegate : public ui::test::TestWebDialogDelegate {
 public:
  explicit TestDialogDelegate(const GURL& url)
      : ui::test::TestWebDialogDelegate(url) {
    set_size(600, 400);
  }

  TestDialogDelegate(const TestDialogDelegate&) = delete;
  TestDialogDelegate& operator=(const TestDialogDelegate&) = delete;

  ~TestDialogDelegate() override = default;
};

// Included in assertion failures so that an unexpected document is visible in
// the test log rather than just a boolean.
std::string DocumentHtml(content::WebContents* contents) {
  return content::EvalJs(contents, "document.documentElement.outerHTML")
      .ExtractString();
}

bool IsDisplayingInterstitial(content::WebContents* contents) {
  auto* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          contents);
  return helper && helper->IsDisplayingInterstitial();
}

}  // namespace

// The production captive portal dialogs run with HTTPS-First Mode on, but
// never in "strict" mode, which matters because HttpsUpgradesInterceptor only
// consults the captive portal exemption when strict mode is off. They run on
// incognito profiles of the ChromeOS sign-in and lock screen profiles, and
// nothing can set kHttpsOnlyModeEnabled on those: they have no settings page,
// the HttpsOnlyMode policy does not apply to them, and Advanced Protection
// needs a signed-in account. These tests therefore run in Balanced Mode.
class HttpsUpgradesCaptivePortalDialogBrowserTest
    : public InProcessBrowserTest {
 public:
  HttpsUpgradesCaptivePortalDialogBrowserTest() {
    // kHttpsFirstBalancedMode only makes Balanced Mode *available*; whether it
    // is on is decided by the pref set in SetUpOnMainThread() below.
    feature_list_.InitWithFeatures(
        {security_interstitials::features::kHttpsFirstDialogUi,
         features::kHttpsFirstBalancedMode},
        {});
  }

  HttpsUpgradesCaptivePortalDialogBrowserTest(
      const HttpsUpgradesCaptivePortalDialogBrowserTest&) = delete;
  HttpsUpgradesCaptivePortalDialogBrowserTest& operator=(
      const HttpsUpgradesCaptivePortalDialogBrowserTest&) = delete;

  ~HttpsUpgradesCaptivePortalDialogBrowserTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
    InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);

    http_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(http_server_.Start());

    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    net::CertVerifyResult verify_result;
    verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;
    verify_result.verified_cert = https_server_.GetCertificate();
    for (const char* host :
         {kBadHttpsHost, kPortalLoginHost, kPortalHop1Host, kPortalHop2Host}) {
      mock_cert_verifier_.mock_cert_verifier()->AddResultForCertAndHost(
          https_server_.GetCertificate(), host, verify_result,
          net::ERR_CERT_COMMON_NAME_INVALID);
    }

    HttpsUpgradesInterceptor::SetHttpsPortForTesting(https_server_.port());
    HttpsUpgradesInterceptor::SetHttpPortForTesting(http_server_.port());

    GetProfile()->GetPrefs()->SetBoolean(prefs::kHttpsFirstBalancedMode, true);
  }

  void TearDownOnMainThread() override {
    if (dialog_widget_) {
      // CloseNow() synchronously destroys the Widget, so clear the raw_ptr
      // first rather than letting it dangle across the call.
      dialog_widget_.ExtractAsDangling()->CloseNow();
    }
    HttpsUpgradesInterceptor::SetHttpsPortForTesting(0);
    HttpsUpgradesInterceptor::SetHttpPortForTesting(0);
    // Clears SetOSReportsCaptivePortalForTesting(), which is global state.
    SSLErrorHandler::ResetConfigForTesting();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  // Opens a views::WebDialogView hosting `url` and waits for that navigation to
  // finish, successfully or not. Deliberately avoids
  // ui_test_utils::NavigateToURL and content::WaitForLoadStop: both report
  // failure on an error-page commit, which is an expected outcome here.
  content::WebContents* ShowDialogAndWaitForNavigation(
      const GURL& url,
      captive_portal::CaptivePortalWindowType captive_portal_window_type,
      int* out_net_error) {
    dialog_delegate_ = std::make_unique<TestDialogDelegate>(url);

    content::WebContentsAddedObserver contents_observer;
    content::TestNavigationObserver navigation_observer(url);
    navigation_observer.StartWatchingNewWebContents();

    gfx::NativeWindow dialog_window = chrome::ShowWebDialog(
        /*parent=*/gfx::NativeView(), GetProfile(), dialog_delegate_.get(),
        /*show=*/true, captive_portal_window_type);
    dialog_widget_ = views::Widget::GetWidgetForNativeWindow(dialog_window);

    content::WebContents* dialog_contents = contents_observer.GetWebContents();
    navigation_observer.WaitForNavigationFinished();

    // The navigation has committed, but the document has not necessarily been
    // parsed yet. Ignore the return value: it reports whether the last
    // navigation succeeded, and an error-page commit is an expected outcome
    // for some of these tests.
    std::ignore = content::WaitForLoadStop(dialog_contents);

    *out_net_error = navigation_observer.last_net_error_code();
    return dialog_contents;
  }

  // Exempts `host` from HTTPS-First Mode for the rest of the test, the same
  // way clicking through the HTTPS-First Mode interstitial would. The negative
  // controls below use this to get a non-captive-portal dialog past its initial
  // load, so that the navigation under test is the one that is upgraded.
  void AllowHttpForHost(const char* host) {
    GetProfile()->GetSSLHostStateDelegate()->AllowHttpForHost(
        host, GetProfile()->GetDefaultStoragePartition());
  }

  net::EmbeddedTestServer& http_server() { return http_server_; }
  net::EmbeddedTestServer& https_server() { return https_server_; }

  // The fixture owns the delegate; TearDownOnMainThread() closes the dialog
  // first so that the Widget never outlives it.
  std::unique_ptr<TestDialogDelegate> dialog_delegate_;
  raw_ptr<views::Widget> dialog_widget_ = nullptr;

 private:
  base::test::ScopedFeatureList feature_list_;
  content::ContentMockCertVerifier mock_cert_verifier_;
  net::EmbeddedTestServer http_server_{net::EmbeddedTestServer::TYPE_HTTP};
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

// A captive portal serves a page that immediately navigates the user onward to
// the real login page over HTTP. That second navigation is renderer-initiated
// and is where a per-navigation opt-out would stop applying. Marking the
// dialog's WebContents is what keeps it exempt, and without that the user never
// reaches the login page at all.
IN_PROC_BROWSER_TEST_F(HttpsUpgradesCaptivePortalDialogBrowserTest,
                       CaptivePortalDialogExemptsRendererInitiatedNavigation) {
  const GURL probe_url = http_server().GetURL(kBadHttpsHost, "/simple.html");
  const GURL login_url = http_server().GetURL(kPortalLoginHost, "/title2.html");

  int net_error = net::ERR_FAILED;
  content::WebContents* dialog_contents = ShowDialogAndWaitForNavigation(
      probe_url, captive_portal::CaptivePortalWindowType::kPopup, &net_error);
  ASSERT_NE(nullptr, dialog_contents);
  ASSERT_EQ(nullptr, tabs::TabInterface::MaybeGetFromContents(dialog_contents));
  ASSERT_EQ(net::OK, net_error);

  content::TestNavigationObserver second_navigation(dialog_contents);
  content::ExecuteScriptAsync(
      dialog_contents, content::JsReplace("location.href = $1;", login_url));
  second_navigation.WaitForNavigationFinished();
  std::ignore = content::WaitForLoadStop(dialog_contents);

  EXPECT_EQ(net::OK, second_navigation.last_net_error_code());
  // Still plain HTTP on the host the portal sent us to, i.e. not upgraded.
  EXPECT_EQ(login_url, dialog_contents->GetLastCommittedURL())
      << DocumentHtml(dialog_contents);

  // Nothing in this flow may leave a persistent HTTP allowlist entry behind.
  // Most exemptions taken in HttpsUpgradesInterceptor also allowlist the
  // navigation's host via a content setting that outlives the dialog, and the
  // hosts below are ones the portal chose, not ones the user named. Both
  // navigations take the captive portal window exemption instead, which
  // deliberately does not allowlist.
  content::SSLHostStateDelegate* state =
      GetProfile()->GetSSLHostStateDelegate();
  ASSERT_NE(nullptr, state);
  content::StoragePartition* storage_partition =
      dialog_contents->GetPrimaryMainFrame()->GetStoragePartition();
  EXPECT_FALSE(
      state->IsHttpAllowedForHost(probe_url.GetHost(), storage_partition));
  EXPECT_FALSE(
      state->IsHttpAllowedForHost(login_url.GetHost(), storage_partition));
}

// Portals may chain several cleartext redirects before reaching the login page.
// The exemption applies to the dialog's WebContents rather than to any one
// navigation, so each server redirect in the chain is exempt too.
IN_PROC_BROWSER_TEST_F(HttpsUpgradesCaptivePortalDialogBrowserTest,
                       CaptivePortalDialogExemptsRedirectChain) {
  const GURL probe_url = http_server().GetURL(kBadHttpsHost, "/simple.html");
  const GURL login_url = http_server().GetURL(kPortalLoginHost, "/title2.html");
  const GURL hop2_url = http_server().GetURL(
      kPortalHop2Host, "/server-redirect-302?" +
                           base::EscapeQueryParamValue(login_url.spec(),
                                                       /*use_plus=*/false));
  const GURL hop1_url = http_server().GetURL(
      kPortalHop1Host, "/server-redirect-302?" +
                           base::EscapeQueryParamValue(hop2_url.spec(),
                                                       /*use_plus=*/false));

  int net_error = net::ERR_FAILED;
  content::WebContents* dialog_contents = ShowDialogAndWaitForNavigation(
      probe_url, captive_portal::CaptivePortalWindowType::kPopup, &net_error);
  ASSERT_NE(nullptr, dialog_contents);
  ASSERT_EQ(net::OK, net_error);

  content::TestNavigationObserver navigation(dialog_contents);
  content::ExecuteScriptAsync(
      dialog_contents, content::JsReplace("location.href = $1;", hop1_url));
  navigation.WaitForNavigationFinished();
  std::ignore = content::WaitForLoadStop(dialog_contents);

  EXPECT_EQ(net::OK, navigation.last_net_error_code());
  EXPECT_EQ(login_url, dialog_contents->GetLastCommittedURL())
      << DocumentHtml(dialog_contents);
}

// Negative control for CaptivePortalDialogExemptsRedirectChain. The probe host
// and the first hop are allowlisted so that the navigation starts, which shows
// that a server redirect target is upgraded on its own unless the dialog is
// exempt.
IN_PROC_BROWSER_TEST_F(HttpsUpgradesCaptivePortalDialogBrowserTest,
                       NonCaptivePortalDialogUpgradesRedirectTarget) {
  const GURL probe_url = http_server().GetURL(kBadHttpsHost, "/simple.html");
  const GURL login_url = http_server().GetURL(kPortalLoginHost, "/title2.html");
  const GURL hop1_url = http_server().GetURL(
      kPortalHop1Host, "/server-redirect-302?" +
                           base::EscapeQueryParamValue(login_url.spec(),
                                                       /*use_plus=*/false));
  AllowHttpForHost(kBadHttpsHost);
  AllowHttpForHost(kPortalHop1Host);

  int net_error = net::ERR_FAILED;
  content::WebContents* dialog_contents = ShowDialogAndWaitForNavigation(
      probe_url, captive_portal::CaptivePortalWindowType::kNone, &net_error);
  ASSERT_NE(nullptr, dialog_contents);
  ASSERT_EQ(net::OK, net_error);

  content::TestNavigationObserver navigation(dialog_contents);
  content::ExecuteScriptAsync(
      dialog_contents, content::JsReplace("location.href = $1;", hop1_url));
  navigation.WaitForNavigationFinished();

  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, navigation.last_net_error_code());
}

// Negative control for CaptivePortalDialogExemptsRendererInitiatedNavigation.
// The dialog is not a captive portal sign-in surface, so its renderer-initiated
// navigation must still be subject to HTTPS-First Mode. This is what keeps the
// exemption from becoming a blanket opt-out for every WebDialog. The probe host
// is allowlisted only so that the dialog gets past its initial load.
IN_PROC_BROWSER_TEST_F(
    HttpsUpgradesCaptivePortalDialogBrowserTest,
    NonCaptivePortalDialogUpgradesRendererInitiatedNavigation) {
  const GURL probe_url = http_server().GetURL(kBadHttpsHost, "/simple.html");
  const GURL login_url = http_server().GetURL(kPortalLoginHost, "/title2.html");
  AllowHttpForHost(kBadHttpsHost);

  int net_error = net::ERR_FAILED;
  content::WebContents* dialog_contents = ShowDialogAndWaitForNavigation(
      probe_url, captive_portal::CaptivePortalWindowType::kNone, &net_error);
  ASSERT_NE(nullptr, dialog_contents);
  ASSERT_EQ(nullptr, tabs::TabInterface::MaybeGetFromContents(dialog_contents));
  ASSERT_EQ(net::OK, net_error);

  content::TestNavigationObserver second_navigation(dialog_contents);
  content::ExecuteScriptAsync(
      dialog_contents, content::JsReplace("location.href = $1;", login_url));
  second_navigation.WaitForNavigationFinished();
  std::ignore = content::WaitForLoadStop(dialog_contents);

  // The upgrade was attempted and failed, so the navigation was blocked
  // rather than being allowed through over plain HTTP. Contrast with the
  // net::OK asserted by the captive portal case above. Note that the blocked
  // navigation still commits at the same URL, so the committed URL is not a
  // useful signal here.
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT,
            second_navigation.last_net_error_code());
}

// The dialog's very first navigation is the probe URL itself, which is plain
// HTTP by design. WebDialogView::InitDialog() runs the embedder's hook, and so
// attaches the tab helper, before LoadInitialURL(), which is what makes the
// exemption cover that navigation as well.
IN_PROC_BROWSER_TEST_F(HttpsUpgradesCaptivePortalDialogBrowserTest,
                       CaptivePortalDialogExemptsInitialNavigation) {
  const GURL probe_url = http_server().GetURL(kBadHttpsHost, "/simple.html");

  int net_error = net::ERR_FAILED;
  content::WebContents* dialog_contents = ShowDialogAndWaitForNavigation(
      probe_url, captive_portal::CaptivePortalWindowType::kPopup, &net_error);
  ASSERT_NE(nullptr, dialog_contents);
  ASSERT_EQ(nullptr, tabs::TabInterface::MaybeGetFromContents(dialog_contents));

  EXPECT_EQ(net::OK, net_error);
  EXPECT_EQ(probe_url, dialog_contents->GetLastCommittedURL())
      << DocumentHtml(dialog_contents);
}

// Negative control for the test above. Without it that test could pass simply
// because nothing upgrades a dialog's initial navigation, which would make it
// no evidence at all. Same navigation, only the captive portal marking removed:
// HTTPS-First Mode upgrades it and blocks the fallback.
IN_PROC_BROWSER_TEST_F(HttpsUpgradesCaptivePortalDialogBrowserTest,
                       NonCaptivePortalDialogUpgradesInitialNavigation) {
  const GURL probe_url = http_server().GetURL(kBadHttpsHost, "/simple.html");

  int net_error = net::ERR_FAILED;
  content::WebContents* dialog_contents = ShowDialogAndWaitForNavigation(
      probe_url, captive_portal::CaptivePortalWindowType::kNone, &net_error);
  ASSERT_NE(nullptr, dialog_contents);
  ASSERT_EQ(nullptr, tabs::TabInterface::MaybeGetFromContents(dialog_contents));

  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, net_error);
}

// Everything above is end-to-end, so a regression in the attach step would
// show up as several unrelated-looking navigation failures. Assert the
// mechanism the rest of the file rests on directly.
IN_PROC_BROWSER_TEST_F(HttpsUpgradesCaptivePortalDialogBrowserTest,
                       CaptivePortalDialogMarksWebContentsAsPortalWindow) {
  const GURL probe_url = http_server().GetURL(kBadHttpsHost, "/simple.html");

  int net_error = net::ERR_FAILED;
  content::WebContents* dialog_contents = ShowDialogAndWaitForNavigation(
      probe_url, captive_portal::CaptivePortalWindowType::kPopup, &net_error);
  ASSERT_NE(nullptr, dialog_contents);

  captive_portal::CaptivePortalTabHelper* helper =
      captive_portal::CaptivePortalTabHelper::FromWebContents(dialog_contents);
  ASSERT_NE(nullptr, helper);
  EXPECT_TRUE(helper->is_captive_portal_window());
  // The dialog is a window, not a login tab. The two are distinct, and only
  // the window bit drives the exemption.
  EXPECT_FALSE(helper->is_captive_portal_tab());
}

// Many captive portals serve their login page over HTTPS with a certificate
// that does not validate. Without the captive portal window check in
// SSLErrorHandler, that lands the dialog on a CaptivePortalBlockingPage, which
// deliberately has no "proceed anyway" affordance: its only way out is an
// "open the login page" button. For this dialog that button leads nowhere,
// because OpenLoginPageForBrowser() needs a tabbed browser and the ChromeOS
// sign-in and lock screens have none, so the dialog becomes a dead end. The
// overridable SSL interstitial keeps the user able to inspect the certificate
// problem and continue to the portal.
IN_PROC_BROWSER_TEST_F(HttpsUpgradesCaptivePortalDialogBrowserTest,
                       CaptivePortalDialogShowsOverridableSslInterstitial) {
  // Stand in for the platform reporting a captive portal, which is what
  // ChromeOS does when the user is behind one.
  SSLErrorHandler::SetOSReportsCaptivePortalForTesting(true);

  const GURL portal_url = https_server().GetURL(kBadHttpsHost, "/simple.html");

  int net_error = net::ERR_FAILED;
  content::WebContents* dialog_contents = ShowDialogAndWaitForNavigation(
      portal_url, captive_portal::CaptivePortalWindowType::kPopup, &net_error);
  ASSERT_NE(nullptr, dialog_contents);
  ASSERT_EQ(nullptr, tabs::TabInterface::MaybeGetFromContents(dialog_contents));
  // The navigation must have failed on the certificate, which is the whole
  // premise of the test; an interstitial for any other reason would not prove
  // anything about captive portal handling.
  EXPECT_TRUE(net::IsCertificateError(net_error))
      << net::ErrorToString(net_error);

  auto* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          dialog_contents);
  ASSERT_NE(nullptr, helper);
  ASSERT_TRUE(helper->IsDisplayingInterstitial());

  security_interstitials::SecurityInterstitialPage* blocking_page =
      helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting();
  ASSERT_NE(nullptr, blocking_page);
  ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
            blocking_page->GetTypeForTesting());

  // The point of the interstitial type is the way out of it. Take it, and
  // confirm the portal page actually loads. CaptivePortalBlockingPage has no
  // proceed command at all, so this step cannot succeed without the fix.
  content::TestNavigationObserver proceed_observer(dialog_contents);
  ASSERT_TRUE(content::ExecJs(
      dialog_contents, "window.certificateErrorPageController.proceed();"));
  proceed_observer.WaitForNavigationFinished();
  std::ignore = content::WaitForLoadStop(dialog_contents);

  EXPECT_FALSE(IsDisplayingInterstitial(dialog_contents));
  EXPECT_EQ(portal_url, dialog_contents->GetLastCommittedURL())
      << DocumentHtml(dialog_contents);
}
