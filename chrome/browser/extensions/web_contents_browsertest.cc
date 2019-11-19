// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_navigation_ui_data.h"

namespace extensions {
namespace {

content::WebContents* GetActiveWebContents(const Browser* browser) {
  return browser->tab_strip_model()->GetActiveWebContents();
}

// Saves ExtensionNavigationUIData for each render frame which completes
// navigation.
class ExtensionNavigationUIDataObserver : public content::WebContentsObserver {
 public:
  explicit ExtensionNavigationUIDataObserver(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  const ExtensionNavigationUIData* GetExtensionNavigationUIData(
      content::RenderFrameHost* rfh) const {
    auto iter = navigation_ui_data_map_.find(rfh);
    if (iter == navigation_ui_data_map_.end())
      return nullptr;
    return iter->second.get();
  }

 private:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->HasCommitted())
      return;

    content::RenderFrameHost* rfh = navigation_handle->GetRenderFrameHost();
    const auto* data = static_cast<const ChromeNavigationUIData*>(
        navigation_handle->GetNavigationUIData());
    navigation_ui_data_map_[rfh] =
        data->GetExtensionNavigationUIData()->DeepCopy();
  }

  std::map<content::RenderFrameHost*,
           std::unique_ptr<ExtensionNavigationUIData>>
      navigation_ui_data_map_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionNavigationUIDataObserver);
};

}  // namespace

// Tests that we can load extension pages into the tab area and they can call
// extension APIs.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, WebContents) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/page.html"));

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetActiveWebContents(browser()), "testTabsAPI()", &result));
  EXPECT_TRUE(result);

  // There was a bug where we would crash if we navigated to a page in the same
  // extension because no new render view was getting created, so we would not
  // do some setup.
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/page.html"));
  result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetActiveWebContents(browser()), "testTabsAPI()", &result));
  EXPECT_TRUE(result);
}

// Test that we correctly set up the ExtensionNavigationUIData for each
// navigation.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, ExtensionNavigationUIData) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents = GetActiveWebContents(browser());
  GURL last_committed_main_frame_url = web_contents->GetLastCommittedURL();
  ExtensionNavigationUIDataObserver observer(web_contents);

  // Load a page with an iframe.
  const GURL url = embedded_test_server()->GetURL("/iframe.html");
  ui_test_utils::NavigateToURL(browser(), url);

  SessionTabHelper* session_tab_helper =
      SessionTabHelper::FromWebContents(web_contents);
  ASSERT_TRUE(session_tab_helper);
  int expected_tab_id = session_tab_helper->session_id().id();
  int expected_window_id = session_tab_helper->window_id().id();

  // Test ExtensionNavigationUIData for the main frame.
  {
    const auto* extension_navigation_ui_data =
        observer.GetExtensionNavigationUIData(web_contents->GetMainFrame());
    ASSERT_TRUE(extension_navigation_ui_data);
    EXPECT_FALSE(extension_navigation_ui_data->is_web_view());

    ExtensionApiFrameIdMap::FrameData frame_data =
        extension_navigation_ui_data->frame_data();
    EXPECT_EQ(ExtensionApiFrameIdMap::kTopFrameId, frame_data.frame_id);
    EXPECT_EQ(ExtensionApiFrameIdMap::kInvalidFrameId,
              frame_data.parent_frame_id);
    EXPECT_EQ(expected_tab_id, frame_data.tab_id);
    EXPECT_EQ(expected_window_id, frame_data.window_id);
    EXPECT_EQ(last_committed_main_frame_url,
              frame_data.last_committed_main_frame_url);
    EXPECT_FALSE(frame_data.pending_main_frame_url);
  }

  // Test ExtensionNavigationUIData for the sub-frame.
  {
    const auto* extension_navigation_ui_data =
        observer.GetExtensionNavigationUIData(
            content::ChildFrameAt(web_contents->GetMainFrame(), 0));
    ASSERT_TRUE(extension_navigation_ui_data);
    EXPECT_FALSE(extension_navigation_ui_data->is_web_view());

    ExtensionApiFrameIdMap::FrameData frame_data =
        extension_navigation_ui_data->frame_data();
    EXPECT_NE(ExtensionApiFrameIdMap::kInvalidFrameId, frame_data.frame_id);
    EXPECT_NE(ExtensionApiFrameIdMap::kTopFrameId, frame_data.frame_id);
    EXPECT_EQ(ExtensionApiFrameIdMap::kTopFrameId, frame_data.parent_frame_id);
    EXPECT_EQ(expected_tab_id, frame_data.tab_id);
    EXPECT_EQ(expected_window_id, frame_data.window_id);
    EXPECT_EQ(url, frame_data.last_committed_main_frame_url);
    EXPECT_FALSE(frame_data.pending_main_frame_url);
  }
}

}  // namespace extensions
