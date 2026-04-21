// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents.h"

#include <map>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/browser/process_manager.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "ui/base/page_transition_types.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
namespace {

using ExtensionWebContentsBrowserTest = ExtensionBrowserTest;

// Saves ExtensionNavigationUIData for each render frame which completes
// navigation.
class ExtensionNavigationUIDataObserver : public content::WebContentsObserver {
 public:
  explicit ExtensionNavigationUIDataObserver(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  ExtensionNavigationUIDataObserver(const ExtensionNavigationUIDataObserver&) =
      delete;
  ExtensionNavigationUIDataObserver& operator=(
      const ExtensionNavigationUIDataObserver&) = delete;

  const ExtensionNavigationUIData* GetExtensionNavigationUIData(
      content::RenderFrameHost* render_frame_host) const {
    auto iter = navigation_ui_data_map_.find(render_frame_host);
    if (iter == navigation_ui_data_map_.end())
      return nullptr;
    return iter->second.get();
  }

 private:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    CHECK(navigation_handle);
    if (!navigation_handle->HasCommitted()) {
      return;
    }
    content::RenderFrameHost* render_frame_host =
        navigation_handle->GetRenderFrameHost();
    CHECK(render_frame_host);
    const auto* data = static_cast<const ChromeNavigationUIData*>(
        navigation_handle->GetNavigationUIData());
    CHECK(data);
    navigation_ui_data_map_[render_frame_host] =
        data->GetExtensionNavigationUIData()->DeepCopy();
  }

  std::map<content::RenderFrameHost*,
           std::unique_ptr<ExtensionNavigationUIData>>
      navigation_ui_data_map_;
};

// Tests that we can load extension pages into the tab area and they can call
// extension APIs.
IN_PROC_BROWSER_TEST_F(ExtensionWebContentsBrowserTest, WebContents) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(
      web_contents,
      GURL("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/page.html")));

  EXPECT_EQ(true, content::EvalJs(web_contents, "testTabsAPI()"));

  // There was a bug where we would crash if we navigated to a page in the same
  // extension because no new render view was getting created, so we would not
  // do some setup.
  ASSERT_TRUE(NavigateToURL(
      web_contents,
      GURL("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/page.html")));
  EXPECT_EQ(true, content::EvalJs(web_contents, "testTabsAPI()"));
}

// Ensure that platform app frames can't be loaded in a tab even on a redirect.
// Regression test for crbug.com/40052959.
IN_PROC_BROWSER_TEST_F(ExtensionWebContentsBrowserTest,
                       TabNavigationToPlatformApp) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("platform_apps").AppendASCII("minimal"));
  ASSERT_TRUE(extension);

  const GURL test_cases[] = {extension->GetResourceURL("main.html"),
                             BackgroundInfo::GetBackgroundURL(extension)};
  for (const GURL& app_url : test_cases) {
    GURL redirect_to_platform_app =
        embedded_test_server()->GetURL("/server-redirect?" + app_url.spec());
    content::WebContents* web_contents = GetActiveWebContents();
    content::TestNavigationObserver observer(web_contents,
                                             net::ERR_BLOCKED_BY_CLIENT);
    // Returns false because the navigation does not succeed.
    ASSERT_FALSE(NavigateToURL(web_contents, redirect_to_platform_app));
    observer.Wait();
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }
}

// Ensure that the extension's background page can't be navigated away.
// Regression test for crbug.com/40149659.
IN_PROC_BROWSER_TEST_F(ExtensionWebContentsBrowserTest,
                       BackgroundPageNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("common").AppendASCII("background_page"));
  ASSERT_TRUE(extension);

  ExtensionHost* host =
      ProcessManager::Get(profile())->GetBackgroundHostForExtension(
          extension->id());
  ASSERT_TRUE(host);

  content::WebContents* background_contents = host->web_contents();

  // Navigation to a different url should be disallowed.
  {
    GURL target_url = embedded_test_server()->GetURL("/body1.html");
    content::TestNavigationManager navigation_observer(background_contents,
                                                       target_url);
    constexpr char kScript[] = "window.location.href = '%s'";
    ASSERT_TRUE(ExecuteScriptInBackgroundPageNoWait(
        extension->id(),
        base::StringPrintf(kScript, target_url.spec().c_str())));
    ASSERT_TRUE(navigation_observer.WaitForNavigationFinished());
    EXPECT_FALSE(navigation_observer.was_committed());
    EXPECT_EQ(extension->GetResourceURL("background.html"),
              background_contents->GetLastCommittedURL());
  }

  // A same-document navigation is still permitted.
  {
    GURL target_url = extension->GetResourceURL("background.html#fragment");
    content::TestNavigationManager navigation_observer(background_contents,
                                                       target_url);
    constexpr char kScript[] = "window.location.href = '%s'";
    ASSERT_TRUE(ExecuteScriptInBackgroundPageNoWait(
        extension->id(),
        base::StringPrintf(kScript, target_url.spec().c_str())));
    ASSERT_TRUE(navigation_observer.WaitForNavigationFinished());
    EXPECT_TRUE(navigation_observer.was_committed());
    EXPECT_EQ(target_url, background_contents->GetLastCommittedURL());
  }

  // Another same-document navigation case.
  {
    GURL target_url = extension->GetResourceURL("bar.html");
    content::TestNavigationManager navigation_observer(background_contents,
                                                       target_url);
    constexpr char kScript[] = "history.pushState({}, '', '%s')";
    ASSERT_TRUE(ExecuteScriptInBackgroundPageNoWait(
        extension->id(),
        base::StringPrintf(kScript, target_url.spec().c_str())));
    ASSERT_TRUE(navigation_observer.WaitForNavigationFinished());
    EXPECT_TRUE(navigation_observer.was_committed());
    EXPECT_EQ(target_url, background_contents->GetLastCommittedURL());
  }
}

// Test that we correctly set up the ExtensionNavigationUIData for each
// navigation.
IN_PROC_BROWSER_TEST_F(ExtensionWebContentsBrowserTest,
                       ExtensionNavigationUIData) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Start on a WebUI page. This avoids navigation observation problems on
  // Android where the initial page (e.g. the NTP) may be native UI.
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, GURL("chrome://version")));

  // Observe for navigation changes to cache NavigationUIData for later.
  ExtensionNavigationUIDataObserver observer(web_contents);

  // Load a page with an iframe. Use the browser Navigate() method because on
  // Android the NavigateToURL() helper calls directly into the content layer
  // and bypasses ChromeNavigationUIData and ExtensionNavigationUIData creation.
  NavigateParams params(GetBrowserWindowInterface(),
                        embedded_test_server()->GetURL("/iframe.html"),
                        ui::PAGE_TRANSITION_TYPED);
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  Navigate(&params);

  // Ensure the load is complete so DidFinishNavigation will fire.
  ASSERT_EQ(web_contents, GetActiveWebContents());
  content::WaitForLoadStop(web_contents);

  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(web_contents);
  ASSERT_TRUE(session_tab_helper);
  int expected_tab_id = session_tab_helper->session_id().id();
  int expected_window_id = session_tab_helper->window_id().id();

  // Test ExtensionNavigationUIData for the main frame.
  {
    const auto* extension_navigation_ui_data =
        observer.GetExtensionNavigationUIData(
            web_contents->GetPrimaryMainFrame());
    ASSERT_TRUE(extension_navigation_ui_data);
    EXPECT_FALSE(extension_navigation_ui_data->is_web_view());

    ExtensionApiFrameIdMap::FrameData frame_data =
        extension_navigation_ui_data->frame_data();
    EXPECT_EQ(ExtensionApiFrameIdMap::kTopFrameId, frame_data.frame_id);
    EXPECT_EQ(ExtensionApiFrameIdMap::kInvalidFrameId,
              frame_data.parent_frame_id);
    EXPECT_EQ(expected_tab_id, frame_data.tab_id);
    EXPECT_EQ(expected_window_id, frame_data.window_id);
  }

  // Test ExtensionNavigationUIData for the sub-frame.
  {
    const auto* extension_navigation_ui_data =
        observer.GetExtensionNavigationUIData(
            content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0));
    ASSERT_TRUE(extension_navigation_ui_data);
    EXPECT_FALSE(extension_navigation_ui_data->is_web_view());

    ExtensionApiFrameIdMap::FrameData frame_data =
        extension_navigation_ui_data->frame_data();
    EXPECT_NE(ExtensionApiFrameIdMap::kInvalidFrameId, frame_data.frame_id);
    EXPECT_NE(ExtensionApiFrameIdMap::kTopFrameId, frame_data.frame_id);
    EXPECT_EQ(ExtensionApiFrameIdMap::kTopFrameId, frame_data.parent_frame_id);
    EXPECT_EQ(expected_tab_id, frame_data.tab_id);
    EXPECT_EQ(expected_window_id, frame_data.window_id);
  }
}

}  // namespace
}  // namespace extensions
