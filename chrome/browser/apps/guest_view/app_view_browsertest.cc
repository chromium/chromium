// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_factory.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/app_runtime/app_runtime_api.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/guest_view/app_view/app_view_constants.h"
#include "extensions/browser/guest_view/app_view/app_view_guest.h"
#include "extensions/browser/guest_view/extensions_guest_view_manager_delegate.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::ExtensionsAPIClient;
using guest_view::GuestViewManager;
using guest_view::TestGuestViewManagerFactory;

class AppViewTest : public extensions::PlatformAppBrowserTest {
 public:
  AppViewTest() = default;
  AppViewTest(const AppViewTest&) = delete;
  AppViewTest& operator=(const AppViewTest&) = delete;

  enum TestServer { NEEDS_TEST_SERVER, NO_TEST_SERVER };

  void TestHelper(const std::string& test_name,
                  const std::string& app_location,
                  const std::string& app_to_embed,
                  TestServer test_server) {
    // For serving guest pages.
    if (test_server == NEEDS_TEST_SERVER) {
      if (!StartEmbeddedTestServer()) {
        LOG(ERROR) << "FAILED TO START TEST SERVER.";
        return;
      }
    }

    LoadAndLaunchPlatformApp(app_location.c_str(), "Launched");

    // Flush any pending events to make sure we start with a clean slate.
    content::RunAllPendingInMessageLoop();

    content::WebContents* embedder_web_contents =
        GetFirstAppWindowWebContents();
    if (!embedder_web_contents) {
      LOG(ERROR) << "UNABLE TO FIND EMBEDDER WEB CONTENTS.";
      return;
    }

    ExtensionTestMessageListener done_listener("TEST_PASSED");
    done_listener.set_failure_message("TEST_FAILED");
    if (!content::ExecJs(
            embedder_web_contents,
            base::StringPrintf("runTest('%s', '%s')", test_name.c_str(),
                               app_to_embed.c_str()))) {
      LOG(ERROR) << "UNABLE TO START TEST.";
      return;
    }
    ASSERT_TRUE(done_listener.WaitUntilSatisfied());
  }

  guest_view::TestGuestViewManager* test_guest_view_manager() const {
    return test_guest_view_manager_;
  }

  void CloseAppWindow() {
    content::WebContents* embedder_web_contents =
        GetFirstAppWindowWebContents();
    content::WebContentsDestroyedWatcher destroyed_watcher(
        embedder_web_contents);
    EXPECT_TRUE(content::ExecJs(embedder_web_contents, "window.close()"));
    destroyed_watcher.Wait();
  }

  // Completes an onEmbedRequested request in `app`. Assumes the app has a
  // `continueEmbedding` function which does this.
  void ContinueEmbedding(const extensions::Extension* app, bool allow_request) {
    content::WebContents* host_contents =
        extensions::ProcessManager::Get(browser()->profile())
            ->GetBackgroundHostForExtension(app->id())
            ->host_contents();
    ASSERT_TRUE(content::WaitForLoadStop(host_contents));
    ASSERT_TRUE(content::ExecJs(
        host_contents,
        content::JsReplace("continueEmbedding($1);", allow_request)));
  }

 private:
  void SetUpOnMainThread() override {
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();
    test_guest_view_manager_ = factory_.GetOrCreateTestGuestViewManager(
        browser()->profile(),
        ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate());
  }

  void TearDownOnMainThread() override {
    test_guest_view_manager_ = nullptr;
    extensions::PlatformAppBrowserTest::TearDownOnMainThread();
  }

  TestGuestViewManagerFactory factory_;
  raw_ptr<guest_view::TestGuestViewManager> test_guest_view_manager_ = nullptr;
};

// Tests that <appview> is able to navigate to another installed app.
IN_PROC_BROWSER_TEST_F(AppViewTest, TestAppViewWithUndefinedDataShouldSucceed) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testAppViewWithUndefinedDataShouldSucceed", "app_view/shim",
             skeleton_app->id(), NO_TEST_SERVER);
}

// Tests that <appview> correctly processes parameters passed on connect.
IN_PROC_BROWSER_TEST_F(AppViewTest, TestAppViewRefusedDataShouldFail) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testAppViewRefusedDataShouldFail", "app_view/shim",
             skeleton_app->id(), NO_TEST_SERVER);
}

// Tests that <appview> correctly processes parameters passed on connect.
IN_PROC_BROWSER_TEST_F(AppViewTest, TestAppViewGoodDataShouldSucceed) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testAppViewGoodDataShouldSucceed", "app_view/shim",
             skeleton_app->id(), NO_TEST_SERVER);
}

// Tests that <appview> correctly handles multiple successive connects.
IN_PROC_BROWSER_TEST_F(AppViewTest, TestAppViewMultipleConnects) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testAppViewMultipleConnects", "app_view/shim", skeleton_app->id(),
             NO_TEST_SERVER);
}

// Tests that <appview> correctly handles connects that occur after the
// completion of a previous connect.
IN_PROC_BROWSER_TEST_F(AppViewTest,
                       TestAppViewConnectFollowingPreviousConnect) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testAppViewConnectFollowingPreviousConnect", "app_view/shim",
             skeleton_app->id(), NO_TEST_SERVER);
}

// Tests that <appview> does not embed self (the app which owns appview).
IN_PROC_BROWSER_TEST_F(AppViewTest, TestAppViewEmbedSelfShouldFail) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testAppViewEmbedSelfShouldFail", "app_view/shim",
             skeleton_app->id(), NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(AppViewTest, TestCloseWithPendingEmbedRequestDeny) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testCloseWithPendingEmbedRequest", "app_view/shim",
             skeleton_app->id(), NO_TEST_SERVER);
  CloseAppWindow();
  ContinueEmbedding(skeleton_app, false);
}

IN_PROC_BROWSER_TEST_F(AppViewTest, TestCloseWithPendingEmbedRequestAllow) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testCloseWithPendingEmbedRequest", "app_view/shim",
             skeleton_app->id(), NO_TEST_SERVER);
  CloseAppWindow();
  ContinueEmbedding(skeleton_app, true);
}

IN_PROC_BROWSER_TEST_F(AppViewTest, KillGuestWithInvalidInstanceID) {
  const extensions::Extension* bad_app =
      LoadAndLaunchPlatformApp("app_view/bad_app", "AppViewTest.LAUNCHED");

  content::RenderProcessHost* bad_app_render_process_host =
      extensions::AppWindowRegistry::Get(browser()->profile())
          ->GetCurrentAppWindowForApp(bad_app->id())
          ->web_contents()
          ->GetPrimaryMainFrame()
          ->GetProcess();

  // Monitor |bad_app|'s RenderProcessHost for its exiting.
  content::RenderProcessHostWatcher exit_observer(
      bad_app_render_process_host,
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  // Choosing a |guest_instance_id| which does not exist.
  int invalid_guest_instance_id =
      test_guest_view_manager()->GetNextInstanceID();
  // Call the desired function to verify that the |bad_app| gets killed if
  // the provided |guest_instance_id| is not mapped to any "GuestView"'s.
  extensions::AppViewGuest::CompletePendingRequest(
      browser()->profile(), GURL("about:blank"), invalid_guest_instance_id,
      bad_app->id(), bad_app_render_process_host);
  exit_observer.Wait();
  EXPECT_FALSE(exit_observer.did_exit_normally());
}

// TODO(crbug.com/40169473): this is flaky on wayland-ozone.
#if BUILDFLAG(IS_OZONE)
#define MAYBE_KillGuestCommunicatingWithWrongAppView \
  DISABLED_KillGuestCommunicatingWithWrongAppView
#else
#define MAYBE_KillGuestCommunicatingWithWrongAppView \
  KillGuestCommunicatingWithWrongAppView
#endif
IN_PROC_BROWSER_TEST_F(AppViewTest,
                       MAYBE_KillGuestCommunicatingWithWrongAppView) {
  const extensions::Extension* host_app =
      LoadAndLaunchPlatformApp("app_view/host_app", "AppViewTest.LAUNCHED");
  const extensions::Extension* guest_app =
      InstallPlatformApp("app_view/guest_app");
  const extensions::Extension* bad_app =
      LoadAndLaunchPlatformApp("app_view/bad_app", "AppViewTest.LAUNCHED");
  // The host app attemps to embed the guest
  EXPECT_TRUE(
      content::ExecJs(extensions::AppWindowRegistry::Get(browser()->profile())
                          ->GetCurrentAppWindowForApp(host_app->id())
                          ->web_contents(),
                      base::StringPrintf("onAppCommand('%s', '%s');", "EMBED",
                                         guest_app->id().c_str())));
  ExtensionTestMessageListener on_embed_requested_listener(
      "AppViewTest.EmbedRequested");
  EXPECT_TRUE(on_embed_requested_listener.WaitUntilSatisfied());
  // While the host is waiting for the guest to accept/deny embedding, the bad
  // app sends a request to the host.
  int guest_instance_id =
      extensions::AppViewGuest::GetAllRegisteredInstanceIdsForTesting()[0];
  content::RenderProcessHostWatcher bad_app_obs(
      extensions::ProcessManager::Get(browser()->profile())
          ->GetBackgroundHostForExtension(bad_app->id())
          ->render_process_host(),
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  base::Value::Dict fake_embed_request_param;
  fake_embed_request_param.Set(appview::kGuestInstanceID, guest_instance_id);
  fake_embed_request_param.Set(appview::kEmbedderID, host_app->id());
  extensions::AppRuntimeEventRouter::DispatchOnEmbedRequestedEvent(
      browser()->profile(), std::move(fake_embed_request_param), bad_app);
  bad_app_obs.Wait();
  EXPECT_FALSE(bad_app_obs.did_exit_normally());
  // Now ask the guest to continue embedding.
  ContinueEmbedding(guest_app, true);
}

// Load an AppView which loads a WebView with a text field. The embedding app
// calls `focus()` on the AppView. The AppView calls `focus()` on the WebView.
// This should be enough to focus the content of the WebView without further
// user input like needing to click on the WebView.
IN_PROC_BROWSER_TEST_F(AppViewTest, FocusWebViewInAppView) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testFocusWebViewInAppView", "app_view/shim", skeleton_app->id(),
             NO_TEST_SERVER);

  extensions::WebViewGuest* webview_guest = nullptr;
  content::WebContents* embedder_web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    content::RenderFrameHost* focused =
        embedder_web_contents->GetFocusedFrame();
    webview_guest = extensions::WebViewGuest::FromRenderFrameHost(focused);
    return !!webview_guest;
  })) << "Timeout waiting for webview focus";

  content::SimulateKeyPress(embedder_web_contents,
                            ui::DomKey::FromCharacter('F'), ui::DomCode::US_F,
                            ui::VKEY_F, false, false, false, false);
  EXPECT_TRUE(
      content::ExecJs(webview_guest->GetGuestMainFrame(), "waitForInput();"));
}

IN_PROC_BROWSER_TEST_F(AppViewTest, TestAppViewCannotOpenNewWindow) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testBasicConnect", "app_view/shim", skeleton_app->id(),
             NO_TEST_SERVER);

  auto* guest_rfh =
      test_guest_view_manager()->WaitForSingleGuestRenderFrameHostCreated();
  // Try to open new windows. Since it's inside an appview, nothing should
  // happen.
  EXPECT_TRUE(content::ExecJs(guest_rfh, "window.open('', 'attempt1');"));
  // Also test with noopener, since the browser side code paths have substantial
  // differences between the opener and noopener cases.
  EXPECT_TRUE(
      content::ExecJs(guest_rfh, "window.open('', 'attempt2', 'noopener');"));

  // Verify that the window open attempts didn't create additional guests.
  EXPECT_EQ(1u, test_guest_view_manager()->num_guests_created());
}
