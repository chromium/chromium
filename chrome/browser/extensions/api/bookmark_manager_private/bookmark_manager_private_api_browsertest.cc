// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api_test_utils.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace extensions {

class BookmarkManagerPrivateApiBrowsertest : public InProcessBrowserTest {
 public:
  void SetUp() override { InProcessBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    model_ = WaitForBookmarkModel();
  }

  void TearDownOnMainThread() override {
    model_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  BookmarkModel* model() { return model_; }

 private:
  BookmarkModel* WaitForBookmarkModel() {
    BookmarkModel* model =
        BookmarkModelFactory::GetForBrowserContext(GetProfile());
    bookmarks::test::WaitForBookmarkModelToLoad(model);
    return model;
  }

  raw_ptr<BookmarkModel> model_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(BookmarkManagerPrivateApiBrowsertest,
                       OpenURLInNewWindow) {
  const BookmarkNode* node =
      model()->AddURL(model()->bookmark_bar_node(), 0, u"Settings",
                      GURL(chrome::kChromeUISettingsURL));
  std::string node_id = base::NumberToString(node->id());

  auto new_window_function =
      base::MakeRefCounted<BookmarkManagerPrivateOpenInNewWindowFunction>();
  std::string args = base::StringPrintf(R"([["%s"], false])", node_id.c_str());
  ASSERT_TRUE(api_test_utils::RunFunction(new_window_function.get(), args,
                                          GetProfile()));
}

IN_PROC_BROWSER_TEST_F(BookmarkManagerPrivateApiBrowsertest,
                       OpenURLInNewWindowIncognito) {
  const BookmarkNode* node =
      model()->AddURL(model()->bookmark_bar_node(), 0, u"Settings",
                      GURL(chrome::kChromeUIVersionURL));
  std::string node_id = base::NumberToString(node->id());

  auto new_window_function =
      base::MakeRefCounted<BookmarkManagerPrivateOpenInNewWindowFunction>();
  std::string args = base::StringPrintf(R"([["%s"], true])", node_id.c_str());
  ASSERT_TRUE(api_test_utils::RunFunction(new_window_function.get(), args,
                                          GetProfile()));
}

IN_PROC_BROWSER_TEST_F(BookmarkManagerPrivateApiBrowsertest,
                       OpenURLInNewTabGroup) {
  const BookmarkNode* node1 =
      model()->AddURL(model()->bookmark_bar_node(), 0, u"Settings",
                      GURL(chrome::kChromeUISettingsURL));
  std::string node_id1 = base::NumberToString(node1->id());

  const BookmarkNode* node2 =
      model()->AddURL(model()->bookmark_bar_node(), 1, u"Version",
                      GURL(chrome::kChromeUIVersionURL));
  std::string node_id2 = base::NumberToString(node2->id());

  auto new_tab_group_function =
      base::MakeRefCounted<BookmarkManagerPrivateOpenInNewTabGroupFunction>();
  std::string args = base::StringPrintf(R"([["%s","%s"]])", node_id1.c_str(),
                                        node_id2.c_str());
  ASSERT_TRUE(api_test_utils::RunFunction(new_tab_group_function.get(), args,
                                          GetProfile()));

  // Verify the tab group and the tabs are created.
  ASSERT_EQ(
      1u, browser()->tab_strip_model()->group_model()->ListTabGroups().size());
  ASSERT_EQ(3, browser()->tab_strip_model()->count());
  ASSERT_EQ(GURL(chrome::kChromeUISettingsURL),
            browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL());
  ASSERT_EQ(GURL(chrome::kChromeUIVersionURL),
            browser()->tab_strip_model()->GetWebContentsAt(2)->GetURL());
}

}  // namespace extensions
