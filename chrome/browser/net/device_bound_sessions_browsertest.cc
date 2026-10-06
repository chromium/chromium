// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_feature_histogram_tester.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/unexportable_keys/features.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/features.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/device_bound_sessions/session_access.h"
#include "net/device_bound_sessions/session_key.h"
#include "net/device_bound_sessions/session_usage.h"
#include "net/device_bound_sessions/test_support.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/scoped_test_mv2_enabler.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/test/test_extension_dir.h"
#endif

using net::device_bound_sessions::SessionAccess;
using net::device_bound_sessions::SessionKey;

namespace {

class DeviceBoundSessionAccessObserver : public content::WebContentsObserver {
 public:
  DeviceBoundSessionAccessObserver(
      content::WebContents* web_contents,
      base::RepeatingCallback<void(const SessionAccess&)> on_access_callback)
      : WebContentsObserver(web_contents),
        on_access_callback_(std::move(on_access_callback)) {}

  void OnDeviceBoundSessionAccessed(content::NavigationHandle* navigation,
                                    const SessionAccess& access) override {
    on_access_callback_.Run(access);
  }
  void OnDeviceBoundSessionAccessed(content::RenderFrameHost* rfh,
                                    const SessionAccess& access) override {
    on_access_callback_.Run(access);
  }

 private:
  base::RepeatingCallback<void(const SessionAccess&)> on_access_callback_;
};

class DeviceBoundSessionBrowserTest : public InProcessBrowserTest {
 public:
  DeviceBoundSessionBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {net::features::kDeviceBoundSessions,
         net::features::kDeviceBoundSessionsBypassDeferralsForRefreshRequests,
         unexportable_keys::
             kEnableBoundSessionCredentialsSoftwareKeysForManualTesting},
        {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server().SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    EXPECT_TRUE(embedded_https_test_server().InitializeAndListen());
    embedded_https_test_server().RegisterRequestHandler(
        net::device_bound_sessions::GetTestRequestHandler(GetURL("/")));
    embedded_https_test_server().StartAcceptingConnections();
  }

  GURL GetURL(std::string_view relative_url) {
    // We use one of the SSL certificates configured by CERT_TEST_NAMES
    // so we can do a DBSC session in a secure context.
    return GetURLForHost("a.test", relative_url);
  }

  GURL GetURLForHost(std::string_view host, std::string_view relative_url) {
    return embedded_https_test_server().GetURL(host, relative_url);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        "origin-trial-public-key",
        net::device_bound_sessions::kTestOriginTrialPublicKey);
  }

  bool NavigateToUrl(GURL url) {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    return WasLatestNavigationValid();
  }

 private:
  bool WasLatestNavigationValid() {
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    return tab->GetController().GetLastCommittedEntry()->GetPageType() ==
           content::PAGE_TYPE_NORMAL;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest,
                       AccessCalledOnRegistrationFromNavigation) {
  base::test::TestFuture<SessionAccess> future;
  DeviceBoundSessionAccessObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      future.GetRepeatingCallback<const SessionAccess&>());
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(NavigateToUrl(GetURL("/dbsc_login_page")));
  ASSERT_TRUE(
      content::ExecJs(web_contents, "document.location = \"/dbsc_required\""));

  SessionAccess access = future.Take();
  EXPECT_EQ(access.session_key.site, net::SchemefulSite(GetURL("/")));
  EXPECT_EQ(access.session_key.id, SessionKey::Id("session_id"));
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest,
                       AccessCalledOnRegistrationFromResource) {
  base::test::TestFuture<SessionAccess> future;
  DeviceBoundSessionAccessObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      future.GetRepeatingCallback<const SessionAccess&>());
  ASSERT_TRUE(NavigateToUrl(GetURL("/resource_triggered_dbsc_registration")));

  SessionAccess access = future.Take();
  EXPECT_EQ(access.session_key.site, net::SchemefulSite(GetURL("/")));
  EXPECT_EQ(access.session_key.id, SessionKey::Id("session_id"));

  EXPECT_THAT(GetCanonicalCookies(browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetBrowserContext(),
                                  GetURL("/dbsc_required")),
              testing::Contains(net::MatchesCookieWithName("auth_cookie")));
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest, UseCounterOnNavigation) {
  WebFeatureHistogramTester histograms;

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(NavigateToUrl(GetURL("/dbsc_login_page")));
  ASSERT_TRUE(
      content::ExecJs(web_contents, "document.location = \"/dbsc_required\""));

  // Navigate away in order to flush use counters.
  ASSERT_TRUE(NavigateToUrl(GURL(url::kAboutBlankURL)));

  EXPECT_EQ(histograms.GetCount(
                blink::mojom::WebFeature::kDeviceBoundSessionRegistered),
            1);
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest, UseCounterOnResource) {
  WebFeatureHistogramTester histograms;

  base::test::TestFuture<SessionAccess> future;
  DeviceBoundSessionAccessObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      future.GetRepeatingCallback<const SessionAccess&>());
  ASSERT_TRUE(NavigateToUrl(GetURL("/resource_triggered_dbsc_registration")));

  ASSERT_TRUE(future.Wait());

  // Navigate away in order to flush use counters.
  ASSERT_TRUE(NavigateToUrl(GURL(url::kAboutBlankURL)));

  EXPECT_EQ(histograms.GetCount(
                blink::mojom::WebFeature::kDeviceBoundSessionRegistered),
            1);
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest,
                       UseCounterForNotDeferred) {
  WebFeatureHistogramTester histograms;

  base::test::TestFuture<SessionAccess> future;
  DeviceBoundSessionAccessObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      future.GetRepeatingCallback<const SessionAccess&>());
  ASSERT_TRUE(NavigateToUrl(GetURL("/resource_triggered_dbsc_registration")));

  ASSERT_TRUE(future.Wait());

  ASSERT_TRUE(NavigateToUrl(GetURL("/ensure_authenticated")));

  // Navigate away in order to flush use counters.
  ASSERT_TRUE(NavigateToUrl(GURL(url::kAboutBlankURL)));

  EXPECT_EQ(histograms.GetCount(
                blink::mojom::WebFeature::kDeviceBoundSessionRequestInScope),
            1);
  EXPECT_EQ(histograms.GetCount(
                blink::mojom::WebFeature::kDeviceBoundSessionRequestDeferral),
            0);
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest, UseCounterForDeferred) {
  WebFeatureHistogramTester histograms;

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(NavigateToUrl(GetURL("/resource_triggered_dbsc_registration")));
    ASSERT_TRUE(future.Wait());
  }

  // Force a refresh
  ASSERT_TRUE(
      content::ExecJs(web_contents, "cookieStore.delete('auth_cookie')"));
  ASSERT_TRUE(NavigateToUrl(GetURL("/ensure_authenticated")));

  // Navigate away in order to flush use counters.
  ASSERT_TRUE(NavigateToUrl(GURL(url::kAboutBlankURL)));

  EXPECT_EQ(histograms.GetCount(
                blink::mojom::WebFeature::kDeviceBoundSessionRequestInScope),
            1);
  EXPECT_EQ(histograms.GetCount(
                blink::mojom::WebFeature::kDeviceBoundSessionRequestDeferral),
            1);
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest,
                       UseCounterForMultipleRequestsOnePage) {
  WebFeatureHistogramTester histograms;

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(NavigateToUrl(GetURL("/resource_triggered_dbsc_registration")));
    ASSERT_TRUE(future.Wait());
  }

  // Make several requests with JS
  ASSERT_TRUE(content::ExecJs(web_contents, "fetch('/ensure_authenticated')"));
  ASSERT_TRUE(content::ExecJs(web_contents, "fetch('/ensure_authenticated')"));
  ASSERT_TRUE(content::ExecJs(web_contents, "fetch('/ensure_authenticated')"));

  // Navigate away in order to flush use counters.
  ASSERT_TRUE(NavigateToUrl(GURL(url::kAboutBlankURL)));

  // Expect only one use counter
  EXPECT_EQ(histograms.GetCount(
                blink::mojom::WebFeature::kDeviceBoundSessionRequestInScope),
            1);
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest,
                       UseCounterForMultipleRequestsTwoPages) {
  WebFeatureHistogramTester histograms;

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(NavigateToUrl(GetURL("/resource_triggered_dbsc_registration")));
    ASSERT_TRUE(future.Wait());
  }

  // Make several requests with JS
  ASSERT_TRUE(content::ExecJs(web_contents, "fetch('/ensure_authenticated')"));
  ASSERT_TRUE(content::ExecJs(web_contents, "fetch('/ensure_authenticated')"));
  ASSERT_TRUE(content::ExecJs(web_contents, "fetch('/ensure_authenticated')"));

  // Navigate again
  ASSERT_TRUE(NavigateToUrl(GetURL("/ensure_authenticated")));

  // Make several more in-scope requests
  ASSERT_TRUE(content::ExecJs(web_contents, "fetch('/ensure_authenticated')"));
  ASSERT_TRUE(content::ExecJs(web_contents, "fetch('/ensure_authenticated')"));
  ASSERT_TRUE(content::ExecJs(web_contents, "fetch('/ensure_authenticated')"));

  // Navigate away in order to flush use counters.
  ASSERT_TRUE(NavigateToUrl(GURL(url::kAboutBlankURL)));

  // Expect two use counters, one for each page load
  EXPECT_EQ(histograms.GetCount(
                blink::mojom::WebFeature::kDeviceBoundSessionRequestInScope),
            2);
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest, NotDeferredLogs) {
  base::HistogramTester histogram_tester;

  base::test::TestFuture<SessionAccess> future;
  DeviceBoundSessionAccessObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      future.GetRepeatingCallback<const SessionAccess&>());
  ASSERT_TRUE(NavigateToUrl(GetURL("/resource_triggered_dbsc_registration")));

  ASSERT_TRUE(future.Wait());

  ASSERT_TRUE(NavigateToUrl(GetURL("/ensure_authenticated")));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.RequestDeferralDecision3",
      /*sample=*/
      net::device_bound_sessions::SessionUsage::kInScopeRefreshNotYetNeeded,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest, DeferredLogs) {
  base::HistogramTester histogram_tester;

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(NavigateToUrl(GetURL("/resource_triggered_dbsc_registration")));
    ASSERT_TRUE(future.Wait());
  }

  // Force a refresh.
  ASSERT_TRUE(
      content::ExecJs(web_contents, "cookieStore.delete('auth_cookie')"));
  ASSERT_TRUE(NavigateToUrl(GetURL("/ensure_authenticated")));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.RequestDeferralDecision3",
      /*sample=*/net::device_bound_sessions::SessionUsage::kDeferred,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest,
                       OneSessionDeferredOneNot) {
  base::HistogramTester histogram_tester;

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  // Create session 1.
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(NavigateToUrl(GetURL("/resource_triggered_dbsc_registration")));
    ASSERT_TRUE(future.Wait());
  }
  // Create session 2.
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(
        NavigateToUrl(GetURL("/resource_triggered_dbsc_registration?session_id="
                             "session2&cookie_name=cookie2")));
    ASSERT_TRUE(future.Wait());
  }

  // Set up proactive refresh for session 2.
  ASSERT_TRUE(content::ExecJs(
      web_contents,
      "cookieStore.set({name: 'cookie2', value: 'abcdef0123', expires: "
      "Date.now() + 60000, sameSite: 'strict', secure: true})"));
  ASSERT_TRUE(NavigateToUrl(GetURL("/ensure_authenticated")));
  // We get a proactive refresh attempted log.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.RequestDeferralDecision3",
      /*sample=*/net::device_bound_sessions::SessionUsage::kDeferred,
      /*expected_count=*/0);
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.RequestDeferralDecision3",
      /*sample=*/
      net::device_bound_sessions::SessionUsage::
          kInScopeProactiveRefreshAttempted,
      /*expected_count=*/1);

  // Set up proactive refresh for session 2 and force a deferred refresh for
  // session 1.
  ASSERT_TRUE(
      content::ExecJs(web_contents, "cookieStore.delete('auth_cookie')"));
  ASSERT_TRUE(content::ExecJs(
      web_contents,
      "cookieStore.set({name: 'cookie2', value: 'abcdef0123', expires: "
      "Date.now() + 60000, sameSite: 'none', secure: true})"));
  ASSERT_TRUE(NavigateToUrl(GetURL("/ensure_authenticated")));
  // We get a deferred log but no additional proactive refresh log because the
  // deferred log takes precedence.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.RequestDeferralDecision3",
      /*sample=*/net::device_bound_sessions::SessionUsage::kDeferred,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.RequestDeferralDecision3",
      /*sample=*/
      net::device_bound_sessions::SessionUsage::
          kInScopeProactiveRefreshAttempted,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest,
                       RefreshWithoutResigningMultipleTimes) {
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  // Register a session. When "OriginTrialFeedback" is enabled, this triggers
  // one signing occurrence.
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(NavigateToUrl(GetURL("/resource_triggered_dbsc_registration")));
    ASSERT_TRUE(future.Wait());
  }
  // Set an early challenge.
  ASSERT_TRUE(
      NavigateToUrl(GetURL("/set_early_challenge?consistent_challenge")));

  // Force a refresh 6 times with the same challenge.
  for (size_t i = 0; i < 6; i++) {
    ASSERT_TRUE(
        content::ExecJs(web_contents, "cookieStore.delete('auth_cookie')"));
    ASSERT_TRUE(NavigateToUrl(GetURL("/ensure_authenticated")));
  }

  // Force one more refresh.
  ASSERT_TRUE(
      content::ExecJs(web_contents, "cookieStore.delete('auth_cookie')"));
  // The signing quota is not exceeded because the consistent challenge
  // has allowed reusing the stored signed challenge.
  ASSERT_TRUE(NavigateToUrl(GetURL("/ensure_authenticated")));
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest,
                       RefreshWithResigningMultipleTimes) {
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  // Register a session. This causes the first signing, only when
  // "OriginTrialFeedback" is enabled.
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(NavigateToUrl(GetURL("/resource_triggered_dbsc_registration")));
    ASSERT_TRUE(future.Wait());
  }

  // Force a refresh 5 times with different early challenges for each.
  for (size_t i = 0; i < 5; i++) {
    ASSERT_TRUE(NavigateToUrl(
        GetURL("/set_early_challenge?challenge" + base::NumberToString(i))));
    ASSERT_TRUE(
        content::ExecJs(web_contents, "cookieStore.delete('auth_cookie')"));
    ASSERT_TRUE(NavigateToUrl(GetURL("/ensure_authenticated")));
  }

  // The initial registration signing counts towards the quota, so the next
  // refresh hits the quota.
  ASSERT_TRUE(NavigateToUrl(GetURL("/set_early_challenge?challenge5")));
  ASSERT_TRUE(
      content::ExecJs(web_contents, "cookieStore.delete('auth_cookie')"));
  // This hits the signing quota.
  std::string signing_quota_query_param = base::EscapeQueryParamValue(
      "quota_exceeded;session_identifier=\"session_id\"", /*use_plus=*/false);
  ASSERT_FALSE(NavigateToUrl(GetURL("/ensure_authenticated?debug_header=" +
                                    signing_quota_query_param)));
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest,
                       RefreshRequestPassesChallenge) {
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  // Register session.
  // We use a query param that will trigger a challenge on refresh.
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(
        NavigateToUrl(GetURL("/resource_triggered_dbsc_registration?trigger_"
                             "challenge=test_challenge")));
    ASSERT_TRUE(future.Wait());
  }

  // Force a refresh that will require signing a challenge.
  ASSERT_TRUE(
      content::ExecJs(web_contents, "cookieStore.delete('auth_cookie')"));
  ASSERT_TRUE(NavigateToUrl(GetURL("/ensure_authenticated")));
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest,
                       InterlockedDeviceBoundSessions) {
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  // Register session 1.
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(NavigateToUrl(GetURL(
        "/resource_triggered_dbsc_registration?trigger_challenge=challenge1")));
    ASSERT_TRUE(future.Wait());
  }

  // Register session 2.
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    // It's important to pass a different refresh path for the second session
    // for it to be in scope of the first session.
    ASSERT_TRUE(NavigateToUrl(
        GetURL("/resource_triggered_dbsc_registration?session_id="
               "session2&cookie_name=cookie2&refresh_path=/"
               "dbsc_refresh_session_2&trigger_challenge=challenge2")));
    ASSERT_TRUE(future.Wait());
  }

  // Delete both cookies to require refresh on next access.
  ASSERT_TRUE(
      content::ExecJs(web_contents, "cookieStore.delete('auth_cookie')"));
  ASSERT_TRUE(content::ExecJs(web_contents, "cookieStore.delete('cookie2')"));

  // Trigger refresh for both sessions.
  ASSERT_TRUE(NavigateToUrl(GetURL("/ensure_authenticated")));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest,
                       ExtensionTriggersRefresh) {
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  // Register a session.
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(NavigateToUrl(GetURL("/resource_triggered_dbsc_registration")));
    ASSERT_TRUE(future.Wait());
  }

  // Set an early challenge.
  ASSERT_TRUE(
      NavigateToUrl(GetURL("/set_early_challenge?consistent_challenge")));

  // Load an extension.
  extensions::ScopedTestMV2Enabler mv2_enabler;
  extensions::TestExtensionDir extension_dir;
  extension_dir.WriteManifest(R"({
    "name": "DBSC Test",
    "manifest_version": 2,
    "version": "1.0",
    "background": {
      "scripts": ["background.js"]
    },
    "incognito": "split",
    "permissions": ["<all_urls>"]
   })");
  extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "");
  extensions::ChromeTestExtensionLoader loader(browser()->GetProfile());
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Delete auth cookie to trigger refresh.
  ASSERT_TRUE(
      content::ExecJs(web_contents, "cookieStore.delete('auth_cookie')"));

  GURL url = GetURL("/ensure_authenticated");

  std::string script = R"((url => {
  fetch(url, {method: 'GET', credentials: 'include'})
    .then(response => chrome.test.sendScriptResult(response.status))
    .catch(err => chrome.test.sendScriptResult(err.message));
    }))";
  base::Value result =
      extensions::browsertest_util::ExecuteScriptInBackgroundPage(
          browser()->GetProfile(), extension->id(),
          script + "('" + url.spec() + "')");

  // DBSC cookie was set successfully because force_ignore_site_for_cookies
  // was correctly threaded to the refresh request.
  EXPECT_EQ(200, result.GetInt());
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest,
                       ExtensionWithoutPermissionFailsRefresh) {
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  // Register a session.
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(NavigateToUrl(GetURL("/resource_triggered_dbsc_registration")));
    ASSERT_TRUE(future.Wait());
  }

  // Set an early challenge.
  ASSERT_TRUE(
      NavigateToUrl(GetURL("/set_early_challenge?consistent_challenge")));

  // Load an extension without <all_urls> permission.
  extensions::ScopedTestMV2Enabler mv2_enabler;
  extensions::TestExtensionDir extension_dir;
  extension_dir.WriteManifest(R"({
    "name": "DBSC Test",
    "manifest_version": 2,
    "version": "1.0",
    "background": {
      "scripts": ["background.js"]
    },
    "incognito": "split"
   })");
  extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "");
  extensions::ChromeTestExtensionLoader loader(browser()->GetProfile());
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Delete auth cookie to trigger refresh.
  ASSERT_TRUE(
      content::ExecJs(web_contents, "cookieStore.delete('auth_cookie')"));

  GURL url = GetURL("/ensure_authenticated");

  std::string script = R"((url => {
  fetch(url, {method: 'GET', credentials: 'include'})
    .then(response => chrome.test.sendScriptResult(response.status))
    .catch(err => chrome.test.sendScriptResult(err.message));
    }))";
  base::Value result =
      extensions::browsertest_util::ExecuteScriptInBackgroundPage(
          browser()->GetProfile(), extension->id(),
          script + "('" + url.spec() + "')");

  // Fetch should fail due to missing permissions.
  EXPECT_TRUE(result.is_string());
  EXPECT_EQ("Failed to fetch", result.GetString());
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// Regression test for https://crbug.com/545350631.
// Verifies that when an omnibox query triggers a DBSC refresh (where
// IsolationInfo has an empty site_for_cookies, but the URLRequest has a valid
// SiteForCookies), the refresh request includes same-site cookies and
// successfully refreshes the session.
IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest,
                       OmniboxQueryWithExpiredCookieTriggersRefresh) {
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  // Register a session.
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(NavigateToUrl(GetURL("/resource_triggered_dbsc_registration")));
    ASSERT_TRUE(future.Wait());
  }

  ASSERT_THAT(GetCanonicalCookies(browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetBrowserContext(),
                                  GetURL("/dbsc_required")),
              testing::Contains(net::MatchesCookieWithName("auth_cookie")));

  // Configure default search engine with suggest URL in scope of the session.
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
  search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);

  TemplateURLData data;
  data.SetShortName(u"Test DSE");
  data.SetKeyword(u"testdse");
  data.SetURL(GetURL("/search?q={searchTerms}").spec());
  data.suggestions_url = GetURL("/suggest?q={searchTerms}").spec();
  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

  // Delete the auth cookie to force a refresh on the next request.
  ASSERT_TRUE(
      content::ExecJs(web_contents, "cookieStore.delete('auth_cookie')"));
  ASSERT_THAT(GetCanonicalCookies(browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetBrowserContext(),
                                  GetURL("/dbsc_required")),
              testing::Not(testing::Contains(
                  net::MatchesCookieWithName("auth_cookie"))));

  // Trigger an Omnibox autocomplete search suggestion request.
  LocationBar* location_bar =
      BrowserWindow::FromBrowser(browser())->GetLocationBar();
  location_bar->FocusLocation(/*is_user_initiated=*/true,
                              /*clear_focus_if_failed=*/false);
  OmniboxView* omnibox_view = location_bar->GetOmniboxView();
  omnibox_view->OnBeforePossibleChange();
  omnibox_view->SetUserText(u"search_term");
  omnibox_view->OnAfterPossibleChange(/*allow_keyword_ui_change=*/true);
  ui_test_utils::WaitForAutocompleteDone(browser());

  // The bound cookie should be restored via DBSC refresh.
  EXPECT_THAT(GetCanonicalCookies(browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetBrowserContext(),
                                  GetURL("/dbsc_required")),
              testing::Contains(net::MatchesCookieWithName("auth_cookie")));

  // Navigating to an authenticated endpoint succeeds.
  ASSERT_TRUE(NavigateToUrl(GetURL("/ensure_authenticated")));
}

// Tests for cross-site subresource fetches: a page on b.test executing a
// fetch() with credentials to a.test, verifying DBSC deferral and refresh
// behavior under cross-site IsolationInfo constraints.

// When a session's bound cookie is SameSite=Strict, a cross-site subresource
// fetch from b.test to a.test cannot send SameSite=Strict cookies under
// cross-site IsolationInfo constraints. Consequently, the missing cookie does
// not defer the request.
IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest,
                       CrossSiteFetchWithSameSiteStrictNotDeferred) {
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  // Register a session on a.test with the default SameSite=Strict cookie.
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(NavigateToUrl(GetURL("/resource_triggered_dbsc_registration")));
    ASSERT_TRUE(future.Wait());
  }

  ASSERT_THAT(GetCanonicalCookies(web_contents->GetBrowserContext(),
                                  GetURL("/dbsc_required")),
              testing::Contains(net::MatchesCookieWithName("auth_cookie")));

  // Delete the auth cookie to test deferral behavior.
  ASSERT_TRUE(
      content::ExecJs(web_contents, "cookieStore.delete('auth_cookie')"));
  ASSERT_THAT(GetCanonicalCookies(web_contents->GetBrowserContext(),
                                  GetURL("/dbsc_required")),
              testing::Not(testing::Contains(
                  net::MatchesCookieWithName("auth_cookie"))));

  // Navigate to a page on b.test.
  ASSERT_TRUE(NavigateToUrl(GetURLForHost("b.test", "/dbsc_login_page")));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  base::HistogramTester histogram_tester;

  // Perform cross-site fetch from b.test to a.test with credentials.
  // Because SameSite=Strict cookies are never included in cross-site requests,
  // the cookie craving is not included and the request is not deferred.
  EXPECT_EQ(401, content::EvalJs(
                     web_contents,
                     content::JsReplace(
                         "fetch($1, {method: 'GET', credentials: 'include'})"
                         ".then(resp => resp.status)",
                         GetURL("/ensure_authenticated?cors=1"))));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.RequestDeferralDecision3",
      net::device_bound_sessions::SessionUsage::kInScopeRefreshNotYetNeeded, 1);
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.RequestDeferralDecision3",
      net::device_bound_sessions::SessionUsage::kDeferred, 0);

  // The cookie remains deleted since no refresh occurred.
  EXPECT_THAT(GetCanonicalCookies(web_contents->GetBrowserContext(),
                                  GetURL("/dbsc_required")),
              testing::Not(testing::Contains(
                  net::MatchesCookieWithName("auth_cookie"))));
}

// When a session is registered with a SameSite=None; Secure bound cookie,
// a cross-site subresource fetch from b.test to a.test includes the cookie
// craving. When the cookie is missing, the request is deferred and triggers
// a DBSC refresh under cross-site IsolationInfo constraints. The refresh
// request sets the SameSite=None cookie (which is valid cross-site), and the
// restarted fetch completes authenticated.
IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest,
                       CrossSiteFetchWithSameSiteNoneTriggersRefresh) {
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  // Register a session on a.test with SameSite=None; Secure cookie.
  std::string registration_query =
      base::StrCat({"cookie_attributes=",
                    base::EscapeQueryParamValue("SameSite=None; Secure",
                                                /*use_plus=*/false)});
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(NavigateToUrl(
        GetURL("/resource_triggered_dbsc_registration?" + registration_query)));
    ASSERT_TRUE(future.Wait());
  }

  ASSERT_THAT(GetCanonicalCookies(web_contents->GetBrowserContext(),
                                  GetURL("/dbsc_required")),
              testing::Contains(net::MatchesCookieWithName("auth_cookie")));

  // Delete the auth cookie to force a refresh on the next request.
  ASSERT_TRUE(
      content::ExecJs(web_contents, "cookieStore.delete('auth_cookie')"));
  ASSERT_THAT(GetCanonicalCookies(web_contents->GetBrowserContext(),
                                  GetURL("/dbsc_required")),
              testing::Not(testing::Contains(
                  net::MatchesCookieWithName("auth_cookie"))));

  // Navigate to a page on b.test.
  ASSERT_TRUE(NavigateToUrl(GetURLForHost("b.test", "/dbsc_login_page")));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  base::HistogramTester histogram_tester;

  // Perform cross-site fetch from b.test to a.test with credentials.
  // The request is deferred, refreshed with cross-site IsolationInfo, and
  // succeeds with HTTP 200.
  EXPECT_EQ(200, content::EvalJs(
                     web_contents,
                     content::JsReplace(
                         "fetch($1, {method: 'GET', credentials: 'include'})"
                         ".then(resp => resp.status)",
                         GetURL("/ensure_authenticated?cors=1"))));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.RequestDeferralDecision3",
      net::device_bound_sessions::SessionUsage::kDeferred, 1);

  // The bound cookie was restored via cross-site DBSC refresh.
  EXPECT_THAT(GetCanonicalCookies(web_contents->GetBrowserContext(),
                                  GetURL("/dbsc_required")),
              testing::Contains(net::MatchesCookieWithName("auth_cookie")));
}

// When a session restricts allowed_refresh_initiators to a.test, a cross-site
// subresource fetch initiated by b.test is not permitted to trigger a refresh,
// even if the missing cookie is SameSite=None.
IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest,
                       CrossSiteFetchBlockedByAllowedInitiators) {
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  // Register a session on a.test with SameSite=None; Secure cookie, but only
  // allow refresh requests initiated by a.test.
  std::string registration_query =
      base::StrCat({"cookie_attributes=",
                    base::EscapeQueryParamValue("SameSite=None; Secure",
                                                /*use_plus=*/false),
                    "&allowed_refresh_initiators=a.test"});
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(NavigateToUrl(
        GetURL("/resource_triggered_dbsc_registration?" + registration_query)));
    ASSERT_TRUE(future.Wait());
  }

  ASSERT_THAT(GetCanonicalCookies(web_contents->GetBrowserContext(),
                                  GetURL("/dbsc_required")),
              testing::Contains(net::MatchesCookieWithName("auth_cookie")));

  // Delete the auth cookie.
  ASSERT_TRUE(
      content::ExecJs(web_contents, "cookieStore.delete('auth_cookie')"));
  ASSERT_THAT(GetCanonicalCookies(web_contents->GetBrowserContext(),
                                  GetURL("/dbsc_required")),
              testing::Not(testing::Contains(
                  net::MatchesCookieWithName("auth_cookie"))));

  // Navigate to a page on b.test.
  ASSERT_TRUE(NavigateToUrl(GetURLForHost("b.test", "/dbsc_login_page")));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  base::HistogramTester histogram_tester;

  // Cross-site fetch initiated by b.test should not defer because b.test is not
  // an allowed refresh initiator.
  EXPECT_EQ(401, content::EvalJs(
                     web_contents,
                     content::JsReplace(
                         "fetch($1, {method: 'GET', credentials: 'include'})"
                         ".then(resp => resp.status)",
                         GetURL("/ensure_authenticated?cors=1"))));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.RequestDeferralDecision3",
      net::device_bound_sessions::SessionUsage::kInScopeRefreshNotAllowed, 1);
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.RequestDeferralDecision3",
      net::device_bound_sessions::SessionUsage::kDeferred, 0);

  // The cookie remains deleted since no refresh occurred.
  EXPECT_THAT(GetCanonicalCookies(web_contents->GetBrowserContext(),
                                  GetURL("/dbsc_required")),
              testing::Not(testing::Contains(
                  net::MatchesCookieWithName("auth_cookie"))));
}

// When a session is registered with both a SameSite=Strict cookie and a
// SameSite=None cookie (a realistic multi-cookie session configuration), a
// cross-site subresource fetch only craves the SameSite=None cookie. If the
// cookies are missing, the cross-site fetch triggers a DBSC refresh under
// cross-site IsolationInfo constraints. The refresh response attempts to set
// both cookies, but only the SameSite=None cookie is stored; the
// SameSite=Strict cookie is rejected due to cross-site constraints. The
// deferred fetch succeeds with the refreshed SameSite=None cookie.
IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest,
                       CrossSiteFetchRefreshesSameSiteNoneCookieOnly) {
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  // Register a session on a.test with two cookies:
  // - strict_cookie: SameSite=Strict; Secure
  // - none_cookie: SameSite=None; Secure
  std::string registration_query =
      base::StrCat({"cookie_name=strict_cookie&cookie_attributes=",
                    base::EscapeQueryParamValue("SameSite=Strict; Secure",
                                                /*use_plus=*/false),
                    "&extra_cookie_name=none_cookie&extra_cookie_attributes=",
                    base::EscapeQueryParamValue("SameSite=None; Secure",
                                                /*use_plus=*/false)});
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(NavigateToUrl(
        GetURL("/resource_triggered_dbsc_registration?" + registration_query)));
    ASSERT_TRUE(future.Wait());
  }

  ASSERT_THAT(GetCanonicalCookies(web_contents->GetBrowserContext(),
                                  GetURL("/dbsc_required")),
              testing::Contains(net::MatchesCookieWithName("strict_cookie")));
  ASSERT_THAT(GetCanonicalCookies(web_contents->GetBrowserContext(),
                                  GetURL("/dbsc_required")),
              testing::Contains(net::MatchesCookieWithName("none_cookie")));

  // Delete both cookies.
  ASSERT_TRUE(
      content::ExecJs(web_contents, "cookieStore.delete('strict_cookie')"));
  ASSERT_TRUE(
      content::ExecJs(web_contents, "cookieStore.delete('none_cookie')"));
  ASSERT_THAT(GetCanonicalCookies(web_contents->GetBrowserContext(),
                                  GetURL("/dbsc_required")),
              testing::Not(testing::Contains(
                  net::MatchesCookieWithName("strict_cookie"))));
  ASSERT_THAT(GetCanonicalCookies(web_contents->GetBrowserContext(),
                                  GetURL("/dbsc_required")),
              testing::Not(testing::Contains(
                  net::MatchesCookieWithName("none_cookie"))));

  // Navigate to a page on b.test.
  ASSERT_TRUE(NavigateToUrl(GetURLForHost("b.test", "/dbsc_login_page")));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  base::HistogramTester histogram_tester;

  // Perform cross-site fetch from b.test to a.test with credentials.
  // Because strict_cookie is SameSite=Strict, it is not craved for a cross-site
  // request. none_cookie is SameSite=None, so it is craved; its absence defers
  // the fetch and triggers a DBSC refresh with cross-site IsolationInfo.
  // The refresh response sets both cookies, but under cross-site constraints
  // only none_cookie is stored in the cookie jar, while strict_cookie is
  // rejected. The deferred request is restarted with none_cookie and succeeds.
  EXPECT_EQ(200, content::EvalJs(
                     web_contents,
                     content::JsReplace(
                         "fetch($1, {method: 'GET', credentials: 'include'})"
                         ".then(resp => resp.status)",
                         GetURL("/ensure_authenticated?cors=1&expected_cookie="
                                "none_cookie"))));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.RequestDeferralDecision3",
      net::device_bound_sessions::SessionUsage::kDeferred, 1);

  // The SameSite=None cookie was restored via cross-site DBSC refresh.
  EXPECT_THAT(GetCanonicalCookies(web_contents->GetBrowserContext(),
                                  GetURL("/dbsc_required")),
              testing::Contains(net::MatchesCookieWithName("none_cookie")));

  // The SameSite=Strict cookie was not restored because it cannot be set in a
  // cross-site context.
  EXPECT_THAT(GetCanonicalCookies(web_contents->GetBrowserContext(),
                                  GetURL("/dbsc_required")),
              testing::Not(testing::Contains(
                  net::MatchesCookieWithName("strict_cookie"))));
}

// Tests for cross-site iframe refresh: a page on b.test containing an iframe
// to a.test with an expired bound cookie, verifying DBSC deferral/refresh
// behavior under cross-site IsolationInfo constraints and cookie updates
// permitted in third-party contexts (SameSite=None; Secure).

// When a session is registered with a SameSite=None; Secure bound cookie,
// navigating to a page on b.test containing an iframe to a.test triggers
// a DBSC deferral and refresh for the subframe navigation under cross-site
// IsolationInfo constraints. The refresh sets the SameSite=None cookie (which
// is permitted in third-party contexts), the deferred navigation resumes, and
// the subframe loads authenticated.
IN_PROC_BROWSER_TEST_F(
    DeviceBoundSessionBrowserTest,
    CrossSiteIframeNavigationWithSameSiteNoneTriggersRefresh) {
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  // Register a session on a.test with SameSite=None; Secure cookie.
  std::string registration_query =
      base::StrCat({"cookie_attributes=",
                    base::EscapeQueryParamValue("SameSite=None; Secure",
                                                /*use_plus=*/false)});
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(NavigateToUrl(
        GetURL("/resource_triggered_dbsc_registration?" + registration_query)));
    ASSERT_TRUE(future.Wait());
  }

  GURL iframe_url = GetURL("/ensure_authenticated");
  ASSERT_THAT(
      GetCanonicalCookies(web_contents->GetBrowserContext(), iframe_url),
      testing::Contains(net::MatchesCookieWithName("auth_cookie")));

  // Delete the auth cookie to test deferral behavior.
  ASSERT_TRUE(
      content::ExecJs(web_contents, "cookieStore.delete('auth_cookie')"));
  ASSERT_THAT(
      GetCanonicalCookies(web_contents->GetBrowserContext(), iframe_url),
      testing::Not(
          testing::Contains(net::MatchesCookieWithName("auth_cookie"))));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  base::HistogramTester histogram_tester;

  // Navigate to b.test containing an iframe to a.test/ensure_authenticated.
  content::TestNavigationObserver subframe_observer(iframe_url);
  subframe_observer.WatchExistingWebContents();
  ASSERT_TRUE(NavigateToUrl(GetURLForHost(
      "b.test", "/page_with_subframe?iframe_src=" +
                    base::EscapeQueryParamValue(iframe_url.spec(),
                                                /*use_plus=*/false))));
  subframe_observer.Wait();
  EXPECT_TRUE(subframe_observer.last_navigation_succeeded());
  EXPECT_EQ(net::HTTP_OK, subframe_observer.last_http_response_code());

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.RequestDeferralDecision3",
      /*sample=*/net::device_bound_sessions::SessionUsage::kDeferred,
      /*expected_count=*/1);

  // Verify that the cookie was restored in the cookie jar.
  EXPECT_THAT(
      GetCanonicalCookies(web_contents->GetBrowserContext(), iframe_url),
      testing::Contains(net::MatchesCookieWithName("auth_cookie")));

  // Verify that the subframe committed.
  content::RenderFrameHost* subframe =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_NE(subframe, nullptr);
  EXPECT_EQ(iframe_url, subframe->GetLastCommittedURL());

  // Subsequent fetch inside the cross-site subframe to a.test also succeeds.
  EXPECT_EQ(200, content::EvalJs(subframe,
                                 "fetch('/ensure_authenticated')"
                                 ".then(resp => resp.status)"));

  // Subsequent fetch reuses the session without deferral.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.RequestDeferralDecision3",
      /*sample=*/net::device_bound_sessions::SessionUsage::kDeferred,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.RequestDeferralDecision3",
      /*sample=*/
      net::device_bound_sessions::SessionUsage::kInScopeRefreshNotYetNeeded,
      /*expected_count=*/1);
}

// When a session is registered with a SameSite=Strict bound cookie, navigating
// to a page on b.test containing an iframe to a.test cannot send
// SameSite=Strict cookies under cross-site IsolationInfo constraints.
// Consequently, the missing cookie does not defer the subframe navigation, and
// the iframe loads 401.
IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest,
                       CrossSiteIframeNavigationWithSameSiteStrictNotDeferred) {
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  // Register a session on a.test with the default SameSite=Strict cookie.
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(NavigateToUrl(GetURL("/resource_triggered_dbsc_registration")));
    ASSERT_TRUE(future.Wait());
  }

  GURL iframe_url = GetURL("/ensure_authenticated");
  ASSERT_THAT(
      GetCanonicalCookies(web_contents->GetBrowserContext(), iframe_url),
      testing::Contains(net::MatchesCookieWithName("auth_cookie")));

  // Delete the auth cookie.
  ASSERT_TRUE(
      content::ExecJs(web_contents, "cookieStore.delete('auth_cookie')"));
  ASSERT_THAT(
      GetCanonicalCookies(web_contents->GetBrowserContext(), iframe_url),
      testing::Not(
          testing::Contains(net::MatchesCookieWithName("auth_cookie"))));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  base::HistogramTester histogram_tester;

  // Navigate to b.test containing an iframe pointing to
  // a.test/ensure_authenticated.
  content::TestNavigationObserver subframe_observer(iframe_url);
  subframe_observer.WatchExistingWebContents();
  ASSERT_TRUE(NavigateToUrl(GetURLForHost(
      "b.test", "/page_with_subframe?iframe_src=" +
                    base::EscapeQueryParamValue(iframe_url.spec(),
                                                /*use_plus=*/false))));
  subframe_observer.Wait();
  // An HTTP 401 response is still considered a successful navigation at the
  // network level (net::OK).
  EXPECT_TRUE(subframe_observer.last_navigation_succeeded());
  EXPECT_EQ(net::HTTP_UNAUTHORIZED,
            subframe_observer.last_http_response_code());

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.RequestDeferralDecision3",
      net::device_bound_sessions::SessionUsage::kInScopeRefreshNotYetNeeded, 1);
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.RequestDeferralDecision3",
      net::device_bound_sessions::SessionUsage::kDeferred, 0);

  // The cookie remains deleted since no refresh occurred.
  EXPECT_THAT(
      GetCanonicalCookies(web_contents->GetBrowserContext(), iframe_url),
      testing::Not(
          testing::Contains(net::MatchesCookieWithName("auth_cookie"))));

  // The subframe committed 401 unauthorized.
  content::RenderFrameHost* subframe =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_NE(subframe, nullptr);
  EXPECT_EQ(iframe_url, subframe->GetLastCommittedURL());
}

// A subresource fetch() to a.test from inside a cross-site iframe on b.test
// triggers DBSC deferral and refresh when the bound cookie is SameSite=None;
// Secure, restoring the cookie and completing successfully.
IN_PROC_BROWSER_TEST_F(DeviceBoundSessionBrowserTest,
                       CrossSiteIframeFetchTriggersRefresh) {
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  // Register a session on a.test with SameSite=None; Secure cookie.
  std::string registration_query =
      base::StrCat({"cookie_attributes=",
                    base::EscapeQueryParamValue("SameSite=None; Secure",
                                                /*use_plus=*/false)});
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(NavigateToUrl(
        GetURL("/resource_triggered_dbsc_registration?" + registration_query)));
    ASSERT_TRUE(future.Wait());
  }

  GURL iframe_url = GetURL("/ensure_authenticated");
  ASSERT_THAT(
      GetCanonicalCookies(web_contents->GetBrowserContext(), iframe_url),
      testing::Contains(net::MatchesCookieWithName("auth_cookie")));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  base::HistogramTester histogram_tester;

  // Navigate to b.test containing an iframe to a.test/ensure_authenticated
  // while the auth cookie is present so the navigation itself does not defer.
  content::TestNavigationObserver subframe_observer(iframe_url);
  subframe_observer.WatchExistingWebContents();
  ASSERT_TRUE(NavigateToUrl(GetURLForHost(
      "b.test", "/page_with_subframe?iframe_src=" +
                    base::EscapeQueryParamValue(iframe_url.spec(),
                                                /*use_plus=*/false))));
  subframe_observer.Wait();
  EXPECT_TRUE(subframe_observer.last_navigation_succeeded());
  EXPECT_EQ(net::HTTP_OK, subframe_observer.last_http_response_code());

  content::RenderFrameHost* subframe =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_NE(subframe, nullptr);
  EXPECT_EQ(iframe_url, subframe->GetLastCommittedURL());

  // Delete the auth cookie to test subresource fetch deferral.
  network::mojom::CookieDeletionFilter filter;
  filter.cookie_name = "auth_cookie";
  filter.url = iframe_url;
  ASSERT_EQ(1u, content::DeleteCookies(web_contents->GetBrowserContext(),
                                       std::move(filter)));
  ASSERT_THAT(
      GetCanonicalCookies(web_contents->GetBrowserContext(), iframe_url),
      testing::Not(
          testing::Contains(net::MatchesCookieWithName("auth_cookie"))));

  // Prior to the fetch, the initial subframe navigation succeeded with the
  // existing bound cookie, recording kInScopeRefreshNotYetNeeded without
  // deferral.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.RequestDeferralDecision3",
      /*sample=*/net::device_bound_sessions::SessionUsage::kDeferred,
      /*expected_count=*/0);
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.RequestDeferralDecision3",
      /*sample=*/
      net::device_bound_sessions::SessionUsage::kInScopeRefreshNotYetNeeded,
      /*expected_count=*/1);

  // A subresource fetch() inside the cross-site subframe to a.test triggers
  // DBSC deferral and refresh, restoring the cookie and completing with 200.
  EXPECT_EQ(200, content::EvalJs(subframe,
                                 "fetch('/ensure_authenticated')"
                                 ".then(resp => resp.status)"));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.RequestDeferralDecision3",
      /*sample=*/net::device_bound_sessions::SessionUsage::kDeferred,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.RequestDeferralDecision3",
      /*sample=*/
      net::device_bound_sessions::SessionUsage::kInScopeRefreshNotYetNeeded,
      /*expected_count=*/1);

  // Verify that the cookie was restored in the cookie jar.
  EXPECT_THAT(
      GetCanonicalCookies(web_contents->GetBrowserContext(), iframe_url),
      testing::Contains(net::MatchesCookieWithName("auth_cookie")));
}

}  // namespace
