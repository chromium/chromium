// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_factory.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/guest_page_holder.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/app_runtime/app_runtime_api.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_host.h"
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
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"

using extensions::ExtensionsAPIClient;
using guest_view::GuestViewManager;
using guest_view::TestGuestViewManagerFactory;

class AppViewTest : public extensions::PlatformAppBrowserTest,
                    public testing::WithParamInterface<bool> {
 public:
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "MPArch" : "InnerWebContents";
  }

  AppViewTest() {
    scoped_feature_list_.InitWithFeatureState(features::kGuestViewMPArch,
                                              GetParam());
  }

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
        extensions::ProcessManager::Get(browser()->GetProfile())
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
        browser()->GetProfile(),
        ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate());
  }

  void TearDownOnMainThread() override {
    test_guest_view_manager_ = nullptr;
    extensions::PlatformAppBrowserTest::TearDownOnMainThread();
  }

  TestGuestViewManagerFactory factory_;
  raw_ptr<guest_view::TestGuestViewManager> test_guest_view_manager_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         AppViewTest,
                         testing::Bool(),
                         AppViewTest::DescribeParams);

// Tests that <appview> is able to navigate to another installed app.
IN_PROC_BROWSER_TEST_P(AppViewTest, TestAppViewWithUndefinedDataShouldSucceed) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testAppViewWithUndefinedDataShouldSucceed", "app_view/shim",
             skeleton_app->id(), NO_TEST_SERVER);
  // Note that the callback of the appview connect method runs after guest
  // creation, but not necessarily after attachment. So we now ensure that the
  // guest successfully attaches and loads.
  EXPECT_TRUE(test_guest_view_manager()->WaitUntilAttachedAndLoaded(
      test_guest_view_manager()->WaitForSingleGuestViewCreated()));
}

// Tests that <appview> correctly processes parameters passed on connect.
IN_PROC_BROWSER_TEST_P(AppViewTest, TestAppViewRefusedDataShouldFail) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testAppViewRefusedDataShouldFail", "app_view/shim",
             skeleton_app->id(), NO_TEST_SERVER);
}

// Tests that <appview> correctly processes parameters passed on connect.
IN_PROC_BROWSER_TEST_P(AppViewTest, TestAppViewGoodDataShouldSucceed) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testAppViewGoodDataShouldSucceed", "app_view/shim",
             skeleton_app->id(), NO_TEST_SERVER);
  EXPECT_TRUE(test_guest_view_manager()->WaitUntilAttachedAndLoaded(
      test_guest_view_manager()->WaitForSingleGuestViewCreated()));
}

// Tests that <appview> correctly handles multiple successive connects.
IN_PROC_BROWSER_TEST_P(AppViewTest, TestAppViewMultipleConnects) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testAppViewMultipleConnects", "app_view/shim", skeleton_app->id(),
             NO_TEST_SERVER);
  EXPECT_TRUE(test_guest_view_manager()->WaitUntilAttachedAndLoaded(
      test_guest_view_manager()->WaitForSingleGuestViewCreated()));
}

// Tests that <appview> correctly handles connects that occur after the
// completion of a previous connect.
IN_PROC_BROWSER_TEST_P(AppViewTest,
                       TestAppViewConnectFollowingPreviousConnect) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testAppViewConnectFollowingPreviousConnect", "app_view/shim",
             skeleton_app->id(), NO_TEST_SERVER);
  EXPECT_TRUE(test_guest_view_manager()->WaitUntilAttachedAndLoaded(
      test_guest_view_manager()->WaitForSingleGuestViewCreated()));
}

// Tests that <appview> does not embed self (the app which owns appview).
IN_PROC_BROWSER_TEST_P(AppViewTest, TestAppViewEmbedSelfShouldFail) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testAppViewEmbedSelfShouldFail", "app_view/shim",
             skeleton_app->id(), NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(AppViewTest, TestCloseWithPendingEmbedRequestDeny) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testCloseWithPendingEmbedRequest", "app_view/shim",
             skeleton_app->id(), NO_TEST_SERVER);
  CloseAppWindow();
  ContinueEmbedding(skeleton_app, false);
}

IN_PROC_BROWSER_TEST_P(AppViewTest, TestCloseWithPendingEmbedRequestAllow) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testCloseWithPendingEmbedRequest", "app_view/shim",
             skeleton_app->id(), NO_TEST_SERVER);
  CloseAppWindow();
  ContinueEmbedding(skeleton_app, true);
}

IN_PROC_BROWSER_TEST_P(AppViewTest, KillGuestWithInvalidInstanceID) {
  const extensions::Extension* bad_app =
      LoadAndLaunchPlatformApp("app_view/bad_app", "AppViewTest.LAUNCHED");

  content::RenderProcessHost* bad_app_render_process_host =
      extensions::AppWindowRegistry::Get(browser()->GetProfile())
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
      browser()->GetProfile(), GURL("about:blank"), invalid_guest_instance_id,
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
IN_PROC_BROWSER_TEST_P(AppViewTest,
                       MAYBE_KillGuestCommunicatingWithWrongAppView) {
  const extensions::Extension* host_app =
      LoadAndLaunchPlatformApp("app_view/host_app", "AppViewTest.LAUNCHED");
  const extensions::Extension* guest_app =
      InstallPlatformApp("app_view/guest_app");
  const extensions::Extension* bad_app =
      LoadAndLaunchPlatformApp("app_view/bad_app", "AppViewTest.LAUNCHED");
  // The host app attemps to embed the guest
  EXPECT_TRUE(
      content::ExecJs(extensions::AppWindowRegistry::Get(browser()->GetProfile())
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
      extensions::ProcessManager::Get(browser()->GetProfile())
          ->GetBackgroundHostForExtension(bad_app->id())
          ->render_process_host(),
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  base::DictValue fake_embed_request_param;
  fake_embed_request_param.Set(appview::kGuestInstanceID, guest_instance_id);
  fake_embed_request_param.Set(appview::kEmbedderID, host_app->id());
  extensions::AppRuntimeEventRouter::DispatchOnEmbedRequestedEvent(
      browser()->GetProfile(), std::move(fake_embed_request_param), bad_app);
  bad_app_obs.Wait();
  EXPECT_FALSE(bad_app_obs.did_exit_normally());
  // Now ask the guest to continue embedding.
  ContinueEmbedding(guest_app, true);
}

IN_PROC_BROWSER_TEST_P(AppViewTest, NoCrossProfilePendingRequestCollision) {
  // Existing test utilities don't handle opening apps in other profiles. For
  // ease of testing, we simulate an app making an embed request in another
  // profile and leaving it pending.
  const base::UnguessableToken other_profile_token =
      base::UnguessableToken::Create();
  const int colliding_id = 1;
  extensions::AppViewGuest::AddFakePendingRequestForTesting(other_profile_token,
                                                            colliding_id);

  // Now in the original profile, embed an app normally.
  const extensions::Extension* host_app =
      LoadAndLaunchPlatformApp("app_view/host_app", "AppViewTest.LAUNCHED");
  const extensions::Extension* guest_app =
      InstallPlatformApp("app_view/guest_app");

  ExtensionTestMessageListener on_embed_requested_listener(
      "AppViewTest.EmbedRequested");
  ASSERT_TRUE(content::ExecJs(
      extensions::AppWindowRegistry::Get(browser()->GetProfile())
          ->GetCurrentAppWindowForApp(host_app->id())
          ->web_contents(),
      content::JsReplace("onAppCommand($1, $2);", "EMBED", guest_app->id())));
  ASSERT_TRUE(on_embed_requested_listener.WaitUntilSatisfied());
  EXPECT_EQ(
      2u,
      extensions::AppViewGuest::GetAllRegisteredInstanceIdsForTesting().size());

  ContinueEmbedding(guest_app, true);

  guest_view::GuestViewBase* guest =
      test_guest_view_manager()->WaitForSingleGuestViewCreated();
  EXPECT_EQ(browser()->GetProfile(), guest->browser_context());
  EXPECT_TRUE(test_guest_view_manager()->WaitUntilAttachedAndLoaded(guest));

  EXPECT_EQ(
      1u,
      extensions::AppViewGuest::GetAllRegisteredInstanceIdsForTesting().size());
}

// Load an AppView which loads a WebView with a text field. The embedding app
// calls `focus()` on the AppView. The AppView calls `focus()` on the WebView.
// This should be enough to focus the content of the WebView without further
// user input like needing to click on the WebView.
IN_PROC_BROWSER_TEST_P(AppViewTest, FocusWebViewInAppView) {
  // TODO(crbug.com/40202416): Fix focus under MPArch.
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    GTEST_SKIP() << "MPArch implementation skipped. https://crbug.com/40202416";
  }

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

  content::SimulateCharTyped(embedder_web_contents, 'F');
  EXPECT_TRUE(
      content::ExecJs(webview_guest->GetGuestMainFrame(), "waitForInput();"));
}

IN_PROC_BROWSER_TEST_P(AppViewTest, TestAppViewCannotOpenNewWindow) {
  // TODO(crbug.com/40202416): Fix new window blocking under MPArch.
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    GTEST_SKIP() << "MPArch implementation skipped. https://crbug.com/40202416";
  }

  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testBasicConnect", "app_view/shim", skeleton_app->id(),
             NO_TEST_SERVER);

  auto* guest = test_guest_view_manager()->WaitForSingleGuestViewCreated();
  test_guest_view_manager()->WaitUntilAttached(guest);
  content::WaitForLoadStop(guest->web_contents());
  auto* guest_rfh = guest->GetGuestMainFrame();

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

namespace {

// Fails the test if any navigation starts in the observed FrameTreeNode.
class NoFrameNavigationObserver : public content::WebContentsObserver {
 public:
  explicit NoFrameNavigationObserver(content::RenderFrameHost* rfh)
      : content::WebContentsObserver(
            content::WebContents::FromRenderFrameHost(rfh)),
        frame_tree_node_id_(rfh->GetFrameTreeNodeId()) {}

  void DidStartNavigation(content::NavigationHandle* handle) override {
    if (handle->GetFrameTreeNodeId() == frame_tree_node_id_) {
      ADD_FAILURE() << "Unexpected navigation started to " << handle->GetURL();
    }
  }

 private:
  const content::FrameTreeNodeId frame_tree_node_id_;
};

}  // namespace

IN_PROC_BROWSER_TEST_P(AppViewTest, TestAppViewCannotNavigateToWebPage) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testBasicConnect", "app_view/shim", skeleton_app->id(),
             NEEDS_TEST_SERVER);

  auto* guest = test_guest_view_manager()->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(test_guest_view_manager()->WaitUntilAttachedAndLoaded(guest));

  const GURL initial_url = skeleton_app->GetResourceURL("main.html");
  ASSERT_EQ(initial_url, guest->GetGuestMainFrame()->GetLastCommittedURL());

  const GURL web_url = embedded_test_server()->GetURL("/title1.html");

  const struct {
    const char* name;
    const char* script_template;
  } kNavigationAttempts[] = {
      {
          "location.href",
          "window.location.href = $1;",
      },
      {
          "location.assign",
          "window.location.assign($1);",
      },
      {
          "location.replace",
          "window.location.replace($1);",
      },
      {
          "anchor click",
          R"(
            const a = document.createElement('a');
            a.href = $1;
            document.body.appendChild(a);
            a.click();
          )",
      },
      {
          "form GET submit",
          R"(
            const form = document.createElement('form');
            form.method = 'GET';
            form.action = $1;
            document.body.appendChild(form);
            form.submit();
          )",
      },
      {
          "form POST submit",
          R"(
            const form = document.createElement('form');
            form.method = 'POST';
            form.action = $1;
            document.body.appendChild(form);
            form.submit();
          )",
      },
      {
          "meta refresh",
          R"(
            const meta = document.createElement('meta');
            meta.httpEquiv = 'refresh';
            meta.content = '0; url=' + $1;
            document.head.appendChild(meta);
          )",
      },
      {
          "window.open _self",
          "window.open($1, '_self');",
      },
      {
          "iframe top.location.href",
          R"(
            const iframe = document.createElement('iframe');
            document.body.appendChild(iframe);
            iframe.contentWindow.top.location.href = $1;
          )",
      },
  };

  {
    NoFrameNavigationObserver no_navigation_observer(
        guest->GetGuestMainFrame());
    for (const auto& attempt : kNavigationAttempts) {
      SCOPED_TRACE(attempt.name);

      ASSERT_TRUE(content::ExecJs(
          guest->GetGuestMainFrame(),
          content::JsReplace(attempt.script_template, web_url)));

      // Flush scheduled renderer tasks (such as form submissions or meta
      // refreshes) and any resulting Mojo messages to the browser.
      EXPECT_TRUE(content::ExecJs(guest->GetGuestMainFrame(),
                                  "new Promise(resolve => setTimeout(() => "
                                  "setTimeout(resolve, 0), 0));"));
      EXPECT_EQ(initial_url, guest->GetGuestMainFrame()->GetLastCommittedURL());
    }
  }
}

// Verifies browser-side enforcement when a navigation to a web URL reaches the
// browser directly (bypassing `browser_handles_all_top_level_requests`).
IN_PROC_BROWSER_TEST_P(AppViewTest, TestAppViewDirectLoadURLToWebPageBlocked) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testBasicConnect", "app_view/shim", skeleton_app->id(),
             NEEDS_TEST_SERVER);

  auto* guest = test_guest_view_manager()->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(test_guest_view_manager()->WaitUntilAttachedAndLoaded(guest));

  const GURL initial_url = skeleton_app->GetResourceURL("main.html");
  ASSERT_EQ(initial_url, guest->GetGuestMainFrame()->GetLastCommittedURL());

  const GURL web_url = embedded_test_server()->GetURL("/title1.html");
  content::TestNavigationManager nav_manager(guest->web_contents(), web_url);
  guest->GetController().LoadURL(web_url, content::Referrer(),
                                 ui::PAGE_TRANSITION_LINK, std::string());
  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  EXPECT_FALSE(nav_manager.was_committed());
  EXPECT_FALSE(nav_manager.was_successful());
  EXPECT_EQ(initial_url, guest->GetGuestMainFrame()->GetLastCommittedURL());
}

// Verifies that an appview cannot be navigated to another platform app's
// resources (such as the embedder app).
IN_PROC_BROWSER_TEST_P(AppViewTest,
                       TestAppViewDirectLoadURLToOtherPlatformAppBlocked) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testBasicConnect", "app_view/shim", skeleton_app->id(),
             NO_TEST_SERVER);

  auto* guest = test_guest_view_manager()->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(test_guest_view_manager()->WaitUntilAttachedAndLoaded(guest));

  const GURL initial_url = skeleton_app->GetResourceURL("main.html");
  ASSERT_EQ(initial_url, guest->GetGuestMainFrame()->GetLastCommittedURL());

  const GURL embedder_app_url =
      GetFirstAppWindowWebContents()->GetLastCommittedURL();
  ASSERT_NE(embedder_app_url.GetHost(), skeleton_app->id());

  content::TestNavigationManager nav_manager(guest->web_contents(),
                                             embedder_app_url);
  guest->GetController().LoadURL(embedder_app_url, content::Referrer(),
                                 ui::PAGE_TRANSITION_LINK, std::string());
  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  EXPECT_FALSE(nav_manager.was_successful());
}

// Simulates a compromised renderer ignoring
// `browser_handles_all_top_level_requests` and verifies that the browser still
// blocks navigating the appview to a web page.
IN_PROC_BROWSER_TEST_P(
    AppViewTest,
    TestAppViewRendererInitiatedNavigationBlockedWithoutPref) {
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    // Under MPArch, `guest->web_contents()` is the embedder's WebContents and
    // `GuestPageHolderImpl::GetRendererPrefs()` always re-applies
    // `AppViewGuest::GuestOverrideRendererPreferences()`, so we cannot clear
    // `browser_handles_all_top_level_requests` via `GetMutableRendererPrefs()`.
    GTEST_SKIP() << "Cannot override guest renderer prefs via WebContents "
                    "under MPArch.";
  }

  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testBasicConnect", "app_view/shim", skeleton_app->id(),
             NEEDS_TEST_SERVER);

  auto* guest = test_guest_view_manager()->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(test_guest_view_manager()->WaitUntilAttachedAndLoaded(guest));

  const GURL initial_url = skeleton_app->GetResourceURL("main.html");
  ASSERT_EQ(initial_url, guest->GetGuestMainFrame()->GetLastCommittedURL());

  guest->web_contents()
      ->GetMutableRendererPrefs()
      ->browser_handles_all_top_level_requests = false;
  guest->web_contents()->SyncRendererPrefs();

  const GURL web_url = embedded_test_server()->GetURL("/title1.html");
  content::TestNavigationManager nav_manager(guest->web_contents(), web_url);
  ASSERT_TRUE(content::ExecJs(
      guest->GetGuestMainFrame(),
      content::JsReplace("window.location.href = $1;", web_url)));
  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  EXPECT_FALSE(nav_manager.was_committed());
  EXPECT_FALSE(nav_manager.was_successful());
  EXPECT_EQ(initial_url, guest->GetGuestMainFrame()->GetLastCommittedURL());
}
