// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_guest_navigation_throttle.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic_no_webview_contents_manager.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace glic {
namespace {

std::unique_ptr<net::test_server::HttpResponse> MakeRedirectResponse(
    const GURL& target_url) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_FOUND);
  response->AddCustomHeader("Location", target_url.spec());
  return response;
}

std::unique_ptr<net::test_server::HttpResponse>
MakeMinimalGlicClientResponse() {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  response->set_content(
      "<!DOCTYPE html><html><body>"
      "<script type=\"module\" src=\"/glic/browser_tests/minimal_client.js\">"
      "</script></body></html>");
  return response;
}

std::unique_ptr<net::test_server::HttpResponse> MakeHtmlResponse(
    std::string_view html) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  response->set_content(html);
  return response;
}

class GlicGuestNavigationThrottleBrowserTest
    : public GlicBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  GlicGuestNavigationThrottleBrowserTest() {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kGlicDisconnectedWebview,
                                features::kGlicNoWebview},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kGlicDisconnectedWebview},
          /*disabled_features=*/{features::kGlicNoWebview});
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(::switches::kGlicSkipReloadAfterNavigation);
  }

  void SetUpOnMainThread() override {
    GlicBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("b.com", "127.0.0.1");
    SetFRECompletion(GetProfile(), prefs::FreStatus::kCompleted);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// A subframe in the guest cannot move an off-allowlist origin into the guest's
// main frame by navigating it to about:blank. Such a navigation commits without
// a URL loader and would inherit the subframe's origin; the throttle cancels
// it via WillCommitWithoutUrlLoader.
IN_PROC_BROWSER_TEST_P(GlicGuestNavigationThrottleBrowserTest,
                       CancelsAboutBlankMainFrameNavigationFromSubframe) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);

  GURL initial_guest_url = guest_contents->GetLastCommittedURL();
  url::Origin initial_guest_origin =
      guest_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();

  GURL subframe_url = embedded_https_test_server().GetURL(
      "b.com", "/glic/browser_tests/minimal_client.html");

  // Embed an off-allowlist cross-origin subframe in the guest.
  ASSERT_TRUE(
      content::ExecJs(guest_contents, content::JsReplace(R"(
    new Promise((resolve) => {
      const f = document.createElement('iframe');
      f.src = $1;
      f.onload = () => resolve(true);
      document.body.appendChild(f);
    })
  )",
                                                         subframe_url)));

  content::RenderFrameHost* subframe =
      content::ChildFrameAt(guest_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(subframe);
  ASSERT_EQ(url::Origin::Create(subframe_url),
            subframe->GetLastCommittedOrigin());

  // The subframe navigates the main frame to about:blank. The throttle cancels
  // it, so the main frame stays on the allowlisted origin.
  content::TestNavigationManager nav_manager(guest_contents,
                                             GURL("about:blank"));
  ASSERT_TRUE(content::ExecJs(subframe, "top.location.href = 'about:blank';"));
  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  EXPECT_FALSE(nav_manager.was_committed());
  EXPECT_EQ(initial_guest_url, guest_contents->GetLastCommittedURL());
  EXPECT_EQ(initial_guest_origin,
            guest_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  EXPECT_TRUE(instance->host().IsWebClientConnected());
}

// Same-document navigations in the guest main frame commit without a URL loader
// and should proceed normally.
IN_PROC_BROWSER_TEST_P(GlicGuestNavigationThrottleBrowserTest,
                       AllowsSameDocumentNavigation) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);

  url::Origin initial_origin =
      guest_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();

  ASSERT_TRUE(content::ExecJs(guest_contents,
                              "window.location.hash = '#test-section';"));
  EXPECT_TRUE(content::WaitForLoadStop(guest_contents));

  EXPECT_TRUE(
      guest_contents->GetLastCommittedURL().spec().ends_with("#test-section"));
  EXPECT_EQ(initial_origin,
            guest_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  EXPECT_TRUE(instance->host().IsWebClientConnected());
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    GlicGuestNavigationThrottleBrowserTest,
    ::testing::Bool(),
    [](const testing::TestParamInfo<bool>& info) {
      return info.param ? "NoWebview" : "Webview";
    });

class GlicGeicGuestNavigationThrottleBrowserTest : public GlicBrowserTest {
 public:
  using RequestHandler =
      base::RepeatingCallback<std::unique_ptr<net::test_server::HttpResponse>(
          const GURL& url)>;

  GlicGeicGuestNavigationThrottleBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kGeic,
          {{features::kGeicGuestURL.name,
            "https://vertexaisearch.cloud.google/side-panel?configId=test"}}},
         {features::kGlicDisconnectedWebview, {}},
         {features::kGlicNoWebview, {}}},
        {});
    embedded_https_test_server().SetCertHostnames(
        {"vertexaisearch.cloud.google", "auth.cloud.google",
         "accounts.google.com", "idp.example.com", "evil.example.com",
         "127.0.0.1"});
    embedded_https_test_server().RegisterRequestHandler(base::BindRepeating(
        &GlicGeicGuestNavigationThrottleBrowserTest::HandleRequest,
        base::Unretained(this)));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(::switches::kGlicSkipReloadAfterNavigation);
    const int port = embedded_https_test_server().port();
    command_line->AppendSwitchASCII(
        network::switches::kHostResolverRules,
        base::StringPrintf("MAP vertexaisearch.cloud.google 127.0.0.1:%d, "
                           "MAP auth.cloud.google 127.0.0.1:%d, "
                           "MAP accounts.google.com 127.0.0.1:%d, "
                           "MAP idp.example.com 127.0.0.1:%d, "
                           "MAP evil.example.com 127.0.0.1:%d",
                           port, port, port, port, port));
  }

  void SetRequestHandler(RequestHandler handler) {
    base::AutoLock lock(lock_);
    request_handler_ = std::move(handler);
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    RequestHandler handler;
    {
      base::AutoLock lock(lock_);
      handler = request_handler_;
    }
    if (!handler) {
      return nullptr;
    }
    auto host_it = request.headers.find("Host");
    const GURL url =
        (host_it != request.headers.end() && !host_it->second.empty())
            ? GURL(base::StrCat(
                  {"https://", host_it->second, request.relative_url}))
            : request.GetURL();
    return handler.Run(url);
  }

  base::Lock lock_;
  RequestHandler request_handler_ GUARDED_BY(lock_);
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies Case 2a: `/side-panel` -> `302 auth.cloud.google/select-session` ->
// `302 /auth/setocookie` -> `302 /side-panel?csesidx=1` -> `200 OK` commits
// cleanly inside `PrivilegedWebContents`.
IN_PROC_BROWSER_TEST_F(GlicGeicGuestNavigationThrottleBrowserTest,
                       AllowsSilentWorkforceAuthPortal302ChainWhenSignedIn) {
  SetRequestHandler(base::BindRepeating(
      [](const GURL& url) -> std::unique_ptr<net::test_server::HttpResponse> {
        if (url.host() == "vertexaisearch.cloud.google" &&
            url.path() == "/side-panel" &&
            url.query().find("csesidx=") == std::string::npos) {
          return MakeRedirectResponse(
              GURL("https://auth.cloud.google/select-session?continueUrl="
                   "https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel"));
        }
        if (url.host() == "auth.cloud.google" &&
            url.path() == "/select-session") {
          return MakeRedirectResponse(
              GURL("https://vertexaisearch.cloud.google/auth/setocookie?oct=1&"
                   "continueUrl=https%3A%2F%2Fvertexaisearch.cloud.google"
                   "%2Fside-panel%3FconfigId%3Dtest%26csesidx%3D1"));
        }
        if (url.host() == "vertexaisearch.cloud.google" &&
            url.path() == "/auth/setocookie") {
          return MakeRedirectResponse(
              GURL("https://vertexaisearch.cloud.google/side-panel?"
                   "configId=test&csesidx=1&hl=en"));
        }
        if (url.host() == "vertexaisearch.cloud.google" &&
            url.path() == "/side-panel" &&
            url.query().find("csesidx=1") != std::string::npos) {
          return MakeMinimalGlicClientResponse();
        }
        return nullptr;
      }));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);
  EXPECT_EQ(guest_contents->GetLastCommittedURL(),
            GURL("https://vertexaisearch.cloud.google/side-panel?"
                 "configId=test&csesidx=1&hl=en"));
  EXPECT_TRUE(instance->host().IsWebClientConnected());
}

// Verifies Case 2b & 2c: silent 302 hops through `/refreshcookies` and
// `/submit` on the paired Workforce Auth Portal origin proceed cleanly.
IN_PROC_BROWSER_TEST_F(GlicGeicGuestNavigationThrottleBrowserTest,
                       AllowsSilentRefreshCookiesAndSubmit302Chains) {
  base::Lock path_lock;
  std::string auth_hop_path = "/refreshcookies";
  SetRequestHandler(base::BindLambdaForTesting(
      [&](const GURL& url) -> std::unique_ptr<net::test_server::HttpResponse> {
        std::string hop_path;
        {
          base::AutoLock lock(path_lock);
          hop_path = auth_hop_path;
        }
        if (url.host() == "vertexaisearch.cloud.google" &&
            url.path() == "/side-panel" &&
            url.query().find("csesidx=") == std::string::npos) {
          return MakeRedirectResponse(
              GURL("https://auth.cloud.google" + hop_path +
                   "?continueUrl=https%3A%2F%2Fvertexaisearch.cloud."
                   "google%2Fside-panel"));
        }
        if (url.host() == "auth.cloud.google" &&
            (url.path() == "/refreshcookies" || url.path() == "/submit")) {
          return MakeRedirectResponse(
              GURL("https://vertexaisearch.cloud.google/side-panel?"
                   "configId=test&csesidx=1&hl=en"));
        }
        if (url.host() == "vertexaisearch.cloud.google" &&
            url.path() == "/side-panel" &&
            url.query().find("csesidx=1") != std::string::npos) {
          return MakeMinimalGlicClientResponse();
        }
        return nullptr;
      }));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  // Now test a `/submit` bounce on reload.
  {
    base::AutoLock lock(path_lock);
    auth_hop_path = "/submit";
  }
  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);
  ASSERT_TRUE(content::NavigateToURL(
      guest_contents,
      GURL("https://vertexaisearch.cloud.google/side-panel?configId=test"),
      GURL("https://vertexaisearch.cloud.google/side-panel?"
           "configId=test&csesidx=1&hl=en")));
  ASSERT_OK(WaitForGlicClient(instance));
}

// Verifies Case 3c: When `/select-session` on `auth.cloud.google` returns a
// terminal `200 OK` HTML response (e.g. 0 or >=2 sessions),
// `WillProcessResponse()` cancels the navigation before commit and transitions
// the overlay to `ErrorPanelType::kSignIn`.
IN_PROC_BROWSER_TEST_F(
    GlicGeicGuestNavigationThrottleBrowserTest,
    CancelsTerminal200OkOnWorkforceAuthPortalAndShowsSignInOverlay) {
  SetRequestHandler(base::BindRepeating(
      [](const GURL& url) -> std::unique_ptr<net::test_server::HttpResponse> {
        if (url.host() == "vertexaisearch.cloud.google" &&
            url.path() == "/side-panel") {
          return MakeRedirectResponse(
              GURL("https://auth.cloud.google/select-session?continueUrl="
                   "https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel"));
        }
        if (url.host() == "auth.cloud.google" &&
            url.path() == "/select-session") {
          return MakeHtmlResponse("<html><body>Choose a session</body></html>");
        }
        return nullptr;
      }));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForErrorPanelType(mojom::ErrorPanelType::kSignIn, instance));
  ASSERT_OK(WaitForDisplayState(
      GlicNoWebviewContentsManager::DisplayState::kShowingOverlay, instance));

  // Verify no document from `auth.cloud.google` ever committed in the guest.
  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);
  EXPECT_NE(guest_contents->GetLastCommittedURL().host(), "auth.cloud.google");
}

// Verifies Case 3a: Redirecting to `/signin/...` on `auth.cloud.google` is
// cancelled in `WillRedirectRequest()` and transitions the overlay to
// `ErrorPanelType::kSignIn`.
IN_PROC_BROWSER_TEST_F(GlicGeicGuestNavigationThrottleBrowserTest,
                       CancelsInteractiveSignInPathOnWorkforceAuthPortal) {
  SetRequestHandler(base::BindRepeating(
      [](const GURL& url) -> std::unique_ptr<net::test_server::HttpResponse> {
        if (url.host() == "vertexaisearch.cloud.google" &&
            url.path() == "/side-panel") {
          return MakeRedirectResponse(
              GURL("https://auth.cloud.google/signin/locations/global/"
                   "workforcePools/pool-1/providers/idp-1?continueUrl="
                   "https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel"));
        }
        return nullptr;
      }));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForErrorPanelType(mojom::ErrorPanelType::kSignIn, instance));
  ASSERT_OK(WaitForDisplayState(
      GlicNoWebviewContentsManager::DisplayState::kShowingOverlay, instance));
}

// Verifies Case 3b: A `302` redirect from `auth.cloud.google` to an external
// 3P IdP (`https://idp.example.com/authorize`) is classified as
// `kSignInRequired` (`ErrorPanelType::kSignIn`), whereas a direct redirect from
// `/side-panel` to an arbitrary off-allowlist origin
// (`https://evil.example.com`) without visiting the Workforce Auth Portal is
// classified as `kOriginBlocked`
// (`ErrorPanelType::kError`).
// TODO(crbug.com/587110784): Re-enable this test
#if defined(ADDRESS_SANITIZER)
#define MAYBE_CancelsExternal3pIdpRedirectAfterWorkforceAuthPortalAsSignInRequired \
  DISABLED_CancelsExternal3pIdpRedirectAfterWorkforceAuthPortalAsSignInRequired
#else
#define MAYBE_CancelsExternal3pIdpRedirectAfterWorkforceAuthPortalAsSignInRequired \
  CancelsExternal3pIdpRedirectAfterWorkforceAuthPortalAsSignInRequired
#endif
IN_PROC_BROWSER_TEST_F(
    GlicGeicGuestNavigationThrottleBrowserTest,
    MAYBE_CancelsExternal3pIdpRedirectAfterWorkforceAuthPortalAsSignInRequired) {
  base::Lock mode_lock;
  bool redirect_via_auth_portal = true;
  SetRequestHandler(base::BindLambdaForTesting(
      [&](const GURL& url) -> std::unique_ptr<net::test_server::HttpResponse> {
        bool via_auth_portal = true;
        {
          base::AutoLock lock(mode_lock);
          via_auth_portal = redirect_via_auth_portal;
        }
        if (url.host() == "vertexaisearch.cloud.google" &&
            url.path() == "/side-panel") {
          if (via_auth_portal) {
            return MakeRedirectResponse(
                GURL("https://auth.cloud.google/select-session?continueUrl="
                     "https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel"));
          }
          return MakeRedirectResponse(GURL("https://evil.example.com/"));
        }
        if (url.host() == "auth.cloud.google" &&
            url.path() == "/select-session") {
          return MakeRedirectResponse(
              GURL("https://idp.example.com/oauth2/authorize"));
        }
        return nullptr;
      }));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForErrorPanelType(mojom::ErrorPanelType::kSignIn, instance));

  // Now test a direct redirect from `/side-panel` to `https://evil.example.com`
  // without visiting `auth.cloud.google`: must transition to `kError`
  // (`kOriginBlocked`).
  {
    base::AutoLock lock(mode_lock);
    redirect_via_auth_portal = false;
  }
  auto* manager = static_cast<GlicNoWebviewContentsManager*>(
      instance->host().contents_manager());
  ASSERT_TRUE(manager);
  manager->guest_contents()->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(GURL(
          "https://vertexaisearch.cloud.google/side-panel?configId=test")));
  ASSERT_OK(WaitForErrorPanelType(mojom::ErrorPanelType::kError, instance));
}

// Verifies that if the paired Workforce Auth Portal redirects to a 1P login
// origin allowed by `IsGuestOriginAllowed()` (such as `accounts.google.com`),
// `GlicGuestNavigationThrottle` still cancels the redirect as `kSignInRequired`
// (`ErrorPanelType::kSignIn`) rather than letting `accounts.google.com` commit
// in the guest as `GuestState::kLogin`.
IN_PROC_BROWSER_TEST_F(
    GlicGeicGuestNavigationThrottleBrowserTest,
    Cancels1pLoginOriginRedirectAfterWorkforceAuthPortalAsSignInRequired) {
  SetRequestHandler(base::BindRepeating(
      [](const GURL& url) -> std::unique_ptr<net::test_server::HttpResponse> {
        if (url.host() == "vertexaisearch.cloud.google" &&
            url.path() == "/side-panel") {
          return MakeRedirectResponse(
              GURL("https://auth.cloud.google/select-session?continueUrl="
                   "https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel"));
        }
        if (url.host() == "auth.cloud.google" &&
            url.path() == "/select-session") {
          return MakeRedirectResponse(
              GURL("https://accounts.google.com/o/oauth2/v2/auth"));
        }
        if (url.host() == "accounts.google.com") {
          return MakeHtmlResponse("<html><body>Google Sign-In</body></html>");
        }
        return nullptr;
      }));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForErrorPanelType(mojom::ErrorPanelType::kSignIn, instance));
  ASSERT_OK(WaitForDisplayState(
      GlicNoWebviewContentsManager::DisplayState::kShowingOverlay, instance));
}

}  // namespace
}  // namespace glic
