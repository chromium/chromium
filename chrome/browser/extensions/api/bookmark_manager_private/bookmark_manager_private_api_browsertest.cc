// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
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
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());
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
                                          browser()->profile()));
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
                                          browser()->profile()));
}

}  // namespace extensions
