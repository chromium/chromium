// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/password_manager/remote_actor_credential_sharing.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/gurl.h"

namespace {

constexpr char kAllowedHost[] = "gemini.google.com";
constexpr char kDisallowedHost[] = "google.com";

}  // namespace

class RemoteActorCredentialSharingBrowserTest : public InProcessBrowserTest {
 public:
  explicit RemoteActorCredentialSharingBrowserTest(bool enable_feature = true) {
    if (enable_feature) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          password_manager::features::kRemoteActorCredentialSharing,
          {{"allowed_host_for_testing", kAllowedHost}});
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          password_manager::features::kRemoteActorCredentialSharing);
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("ignore-certificate-errors");
    // Force strict site isolation for all sites in the test to ensure
    // IsLockedToSite() passes.
    command_line->AppendSwitch(switches::kSitePerProcess);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    // Start HTTPS server because remote actor credential sharing API requires a
    // secure context
    ssl_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(ssl_server_.Start());
  }

 protected:
  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  GURL GetURLForHost(const std::string& host, const std::string& path) {
    return ssl_server_.GetURL(host, path);
  }

 private:
  autofill::test::AutofillBrowserTestEnvironment autofill_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer ssl_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

// 1. Verify JS API Injection (only exposed on allowed origin)
IN_PROC_BROWSER_TEST_F(RemoteActorCredentialSharingBrowserTest,
                       ApiInjectionRestriction) {
  // Navigate to disallowed origin
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetURLForHost(kDisallowedHost, "/title1.html")));
  EXPECT_EQ(false, content::EvalJs(
                       GetWebContents(),
                       "typeof chrome !== 'undefined' && typeof "
                       "chrome.requestAgentAuthentication !== 'undefined'"));

  // Navigate to allowed origin
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetURLForHost(kAllowedHost, "/title1.html")));
  EXPECT_EQ(true, content::EvalJs(
                      GetWebContents(),
                      "typeof chrome !== 'undefined' && typeof "
                      "chrome.requestAgentAuthentication === 'function'"));
}

// 2. Verify callback receives mock result
IN_PROC_BROWSER_TEST_F(RemoteActorCredentialSharingBrowserTest,
                       AuthenticationResolvesWithFalseForInvalidCredentials) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetURLForHost(kAllowedHost, "/title1.html")));

  // TODO(crbug.com/532482931): Update assertions once real backend
  // authentication is integrated. Trigger requestAgentAuthentication API
  // call
  EXPECT_EQ(false, content::EvalJs(GetWebContents(), R"(
    new Promise((resolve) => {
      chrome.requestAgentAuthentication('gaia_id_123', 'google.com', 'task_id_456', (success) => {
        resolve(success);
      });
    });
  )"));
}

IN_PROC_BROWSER_TEST_F(RemoteActorCredentialSharingBrowserTest,
                       PopupAllowsSingleCallWithoutAdditionalUserGesture) {
  constexpr char kResultHistogram[] =
      "PasswordManager.RemoteActorCredentialSharing.Result";
  constexpr char kRequestScript[] = R"(
    new Promise((resolve) => {
      chrome.requestAgentAuthentication(
          'gaia_id_123', 'google.com', 'task_id_456', (success) => {
            resolve(success);
          });
    });
  )";

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetURLForHost(kAllowedHost, "/title1.html")));

  // In a regular (non-popup) tab, calling without a user gesture is rejected in
  // the renderer and never reaches the browser.
  base::HistogramTester histogram_tester;
  EXPECT_EQ(false, content::EvalJs(GetWebContents(), kRequestScript,
                                   content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  histogram_tester.ExpectTotalCount(kResultHistogram, 0);

  // Open a popup window via `window.open` with a user gesture (simulating an
  // `onclick` handler).
  const GURL popup_url = GetURLForHost(kAllowedHost, "/title2.html");
  content::WebContentsAddedObserver popup_observer;
  ASSERT_TRUE(content::ExecJs(
      GetWebContents(),
      content::JsReplace("window.open($1, '_blank', 'popup');", popup_url)));
  content::WebContents* popup_contents = popup_observer.GetWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(popup_contents));

  // The first call inside the popup without an additional click succeeds in
  // reaching the browser (using the popup-opening user gesture).
  EXPECT_EQ(false, content::EvalJs(popup_contents, kRequestScript,
                                   content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  histogram_tester.ExpectUniqueSample(
      kResultHistogram,
      password_manager::RemoteActorCredentialSharingResult::
          kUserIdentityOrSyncStateInvalid,
      1);

  // A second call inside the same popup without a user gesture is rejected by
  // the renderer because the one-time popup activation was consumed.
  EXPECT_EQ(false, content::EvalJs(popup_contents, kRequestScript,
                                   content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  histogram_tester.ExpectTotalCount(kResultHistogram, 1);

  // A subsequent call with an explicit user gesture in the popup still works.
  EXPECT_EQ(false, content::EvalJs(popup_contents, kRequestScript));
  histogram_tester.ExpectUniqueSample(
      kResultHistogram,
      password_manager::RemoteActorCredentialSharingResult::
          kUserIdentityOrSyncStateInvalid,
      2);
}

IN_PROC_BROWSER_TEST_F(RemoteActorCredentialSharingBrowserTest,
                       CrossSitePopupRequiresUserGesture) {
  constexpr char kResultHistogram[] =
      "PasswordManager.RemoteActorCredentialSharing.Result";
  constexpr char kRequestScript[] = R"(
    new Promise((resolve) => {
      chrome.requestAgentAuthentication(
          'gaia_id_123', 'google.com', 'task_id_456', (success) => {
            resolve(success);
          });
    });
  )";

  // Open a popup from a cross-site opener (`example.com`). The popup activation
  // fallback must not apply across sites.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetURLForHost("example.com", "/title1.html")));
  const GURL popup_url = GetURLForHost(kAllowedHost, "/title2.html");
  content::WebContentsAddedObserver popup_observer;
  ASSERT_TRUE(content::ExecJs(
      GetWebContents(),
      content::JsReplace("window.open($1, '_blank', 'popup');", popup_url)));
  content::WebContents* popup_contents = popup_observer.GetWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(popup_contents));

  base::HistogramTester histogram_tester;
  EXPECT_EQ(false, content::EvalJs(popup_contents, kRequestScript,
                                   content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  histogram_tester.ExpectTotalCount(kResultHistogram, 0);

  // Calling with an explicit user gesture in the popup still works.
  EXPECT_EQ(false, content::EvalJs(popup_contents, kRequestScript));
  histogram_tester.ExpectUniqueSample(
      kResultHistogram,
      password_manager::RemoteActorCredentialSharingResult::
          kUserIdentityOrSyncStateInvalid,
      1);
}

// 3. Verify Bad Message Handling on Origin Violation (Compromised Renderer
// simulation)
IN_PROC_BROWSER_TEST_F(RemoteActorCredentialSharingBrowserTest,
                       BadMessageProcessTerminationOnOriginViolation) {
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;

  // Navigate to the disallowed domain.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetURLForHost(kDisallowedHost, "/title1.html")));

  content::RenderFrameHost* compromised_rfh =
      GetWebContents()->GetPrimaryMainFrame();
  content::RenderProcessHostWatcher crash_observer(
      compromised_rfh->GetProcess(),
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  // Request the Mojo interface from the disallowed origin (bypassing JS
  // checks). The browser-side binder registry must check the origin and
  // terminate the process.
  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
  password_manager::RemoteActorCredentialSharingImpl::BindReceiver(
      remote.BindNewEndpointAndPassReceiver(), compromised_rfh);

  crash_observer.Wait();
  EXPECT_FALSE(crash_observer.did_exit_normally());
}

class RemoteActorCredentialSharingDisabledBrowserTest
    : public RemoteActorCredentialSharingBrowserTest {
 public:
  RemoteActorCredentialSharingDisabledBrowserTest()
      : RemoteActorCredentialSharingBrowserTest(false) {}
};

IN_PROC_BROWSER_TEST_F(RemoteActorCredentialSharingDisabledBrowserTest,
                       ApiNotExposedWhenDisabled) {
  // Navigate to allowed origin
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetURLForHost(kAllowedHost, "/title1.html")));
  // Verify JS API is NOT exposed even on allowed origin
  EXPECT_EQ(false, content::EvalJs(
                       GetWebContents(),
                       "typeof chrome !== 'undefined' && typeof "
                       "chrome.requestAgentAuthentication !== 'undefined'"));
}
