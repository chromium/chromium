// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class DurableStorageBrowserTest : public InProcessBrowserTest {
 public:
  DurableStorageBrowserTest() = default;

  DurableStorageBrowserTest(const DurableStorageBrowserTest&) = delete;
  DurableStorageBrowserTest& operator=(const DurableStorageBrowserTest&) =
      delete;

  ~DurableStorageBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine*) override;
  void SetUpOnMainThread() override;

 protected:
  content::RenderFrameHost* GetRenderFrameHost(Browser* browser) {
    return browser->tab_strip_model()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame();
  }

  content::RenderFrameHost* GetRenderFrameHost() {
    return GetRenderFrameHost(browser());
  }

  void Bookmark(Browser* browser) {
    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(browser->profile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
    bookmarks::AddIfNotBookmarked(bookmark_model, url_, u"");
  }

  void Bookmark() {
    Bookmark(browser());
  }

  bool CheckPermission(content::RenderFrameHost* render_frame_host = nullptr) {
    if (!render_frame_host)
      render_frame_host = GetRenderFrameHost();
    return content::EvalJs(render_frame_host, "checkPermission()")
        .ExtractBool();
  }

  std::string CheckPermissionUsingPermissionApi(
      content::RenderFrameHost* render_frame_host = nullptr) {
    if (!render_frame_host)
      render_frame_host = GetRenderFrameHost();
    return content::EvalJs(render_frame_host,
                           "checkPermissionUsingPermissionApi()")
        .ExtractString();
  }

  bool RequestPermission(
      content::RenderFrameHost* render_frame_host = nullptr) {
    if (!render_frame_host)
      render_frame_host = GetRenderFrameHost();
    return content::EvalJs(render_frame_host, "requestPermission()")
        .ExtractBool();
  }

  GURL url_;
};

void DurableStorageBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitch(
              switches::kEnableExperimentalWebPlatformFeatures);
}

void DurableStorageBrowserTest::SetUpOnMainThread() {
  if (embedded_test_server()->Started())
    return;
  ASSERT_TRUE(embedded_test_server()->Start());
  url_ = embedded_test_server()->GetURL("/durable/durability-permissions.html");
}

IN_PROC_BROWSER_TEST_F(DurableStorageBrowserTest, QueryNonBookmarkedPage) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_));
  EXPECT_FALSE(CheckPermission());
  EXPECT_EQ("prompt", CheckPermissionUsingPermissionApi());
}

IN_PROC_BROWSER_TEST_F(DurableStorageBrowserTest, RequestNonBookmarkedPage) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_));
  EXPECT_FALSE(RequestPermission());
}

IN_PROC_BROWSER_TEST_F(DurableStorageBrowserTest, QueryBookmarkedPage) {
  // Documents that the current behavior is to return "default" if script
  // hasn't requested the durable permission, even if it would be autogranted.
  Bookmark();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_));
  EXPECT_FALSE(CheckPermission());
  EXPECT_EQ("prompt", CheckPermissionUsingPermissionApi());
}

IN_PROC_BROWSER_TEST_F(DurableStorageBrowserTest, RequestBookmarkedPage) {
  Bookmark();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_));
  EXPECT_TRUE(RequestPermission());
}

IN_PROC_BROWSER_TEST_F(DurableStorageBrowserTest, BookmarkThenUnbookmark) {
  Bookmark();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_));

  EXPECT_TRUE(RequestPermission());
  EXPECT_TRUE(CheckPermission());
  EXPECT_EQ("granted", CheckPermissionUsingPermissionApi());

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::RemoveAllBookmarks(bookmark_model, url_, FROM_HERE);

  // Unbookmarking doesn't change the permission.
  EXPECT_TRUE(CheckPermission());
  EXPECT_EQ("granted", CheckPermissionUsingPermissionApi());
  // Requesting after unbookmarking doesn't change the default box.
  EXPECT_TRUE(RequestPermission());
  // Querying after requesting after unbookmarking still reports "granted".
  EXPECT_TRUE(CheckPermission());
  EXPECT_EQ("granted", CheckPermissionUsingPermissionApi());
}

IN_PROC_BROWSER_TEST_F(DurableStorageBrowserTest, FirstTabSeesResult) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_));

  EXPECT_FALSE(CheckPermission());
  EXPECT_EQ("prompt", CheckPermissionUsingPermissionApi());

  chrome::NewTab(browser());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_));
  Bookmark();

  EXPECT_TRUE(RequestPermission());

  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(CheckPermission());
  EXPECT_EQ("granted", CheckPermissionUsingPermissionApi());
}

IN_PROC_BROWSER_TEST_F(DurableStorageBrowserTest, Incognito) {
  Browser* browser = CreateIncognitoBrowser();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, url_));

  Bookmark(browser);
  EXPECT_TRUE(RequestPermission(GetRenderFrameHost(browser)));
  EXPECT_TRUE(CheckPermission(GetRenderFrameHost(browser)));
  EXPECT_EQ("granted",
            CheckPermissionUsingPermissionApi(GetRenderFrameHost(browser)));
}

IN_PROC_BROWSER_TEST_F(DurableStorageBrowserTest, SessionOnly) {
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                 CONTENT_SETTING_SESSION_ONLY);
  Bookmark();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_));

  EXPECT_FALSE(RequestPermission());
  EXPECT_FALSE(CheckPermission());
  EXPECT_EQ("prompt", CheckPermissionUsingPermissionApi());
}
