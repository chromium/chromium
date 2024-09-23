// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/extensions/api/bookmarks/bookmarks_api.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "extensions/browser/api_test_utils.h"

namespace extensions {

class BookmarkManagerPrivateApiUnitTest : public ExtensionServiceTestBase {
 public:
  BookmarkManagerPrivateApiUnitTest() = default;
  BookmarkManagerPrivateApiUnitTest(const BookmarkManagerPrivateApiUnitTest&) =
      delete;
  BookmarkManagerPrivateApiUnitTest& operator=(
      const BookmarkManagerPrivateApiUnitTest&) = delete;

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();

    ExtensionServiceInitParams params;
    params.enable_bookmark_model = true;
    InitializeExtensionService(std::move(params));

    browser_window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams browser_params(profile(), true);
    browser_params.type = Browser::TYPE_NORMAL;
    browser_params.window = browser_window_.get();
    browser_ = std::unique_ptr<Browser>(Browser::Create(browser_params));

    model_ = BookmarkModelFactory::GetForBrowserContext(profile());
    bookmarks::test::WaitForBookmarkModelToLoad(model_);

    url_ = GURL("https://www.google.com");
    const bookmarks::BookmarkNode* node =
        model_->AddURL(model_->other_node(), 0, u"Goog", url_);
    // Store node->id() as we will delete |node| in RunOnDeletedNode().
    node_id_ = base::NumberToString(node->id());
  }

  void TearDown() override {
    browser_->tab_strip_model()->CloseAllTabs();
    browser_.reset();
    browser_window_.reset();
    ExtensionServiceTestBase::TearDown();
  }

  Browser* browser() { return browser_.get(); }
  bookmarks::BookmarkModel* model() { return model_; }
  std::string node_id() const { return node_id_; }
  GURL& url() { return url_; }

 private:
  GURL url_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestBrowserWindow> browser_window_;
  raw_ptr<bookmarks::BookmarkModel> model_ = nullptr;
  std::string node_id_;
};

// Tests that running ExtensionFunction-s on deleted bookmark node gracefully
// fails.
// Regression test for https://crbug.com/739260.
TEST_F(BookmarkManagerPrivateApiUnitTest, RunOnDeletedNode) {
  // Remove our only bookmark node.
  auto remove_function = base::MakeRefCounted<BookmarksRemoveFunction>();
  api_test_utils::RunFunction(remove_function.get(),
                              base::StringPrintf("[\"%s\"]", node_id().c_str()),
                              profile());

  // Call bookmarkManagerPrivate.copy() with the removed bookmark node's id.
  auto copy_function =
      base::MakeRefCounted<BookmarkManagerPrivateCopyFunction>();
  EXPECT_EQ(
      base::StringPrintf("Could not find bookmark nodes with given ids: [%s]",
                         node_id().c_str()),
      api_test_utils::RunFunctionAndReturnError(
          copy_function.get(),
          base::StringPrintf("[[\"%s\"]]", node_id().c_str()), profile()));
}

// Tests that calling bookmarkManagerPrivate.cut() to cut a permanent bookmark
// node into the clipboard gracefully fails.
// Regression test for https://crbug.com/1021829.
TEST_F(BookmarkManagerPrivateApiUnitTest, RunCutOnPermanentNode) {
  auto cut_function = base::MakeRefCounted<BookmarkManagerPrivateCutFunction>();
  std::string node_id =
      base::NumberToString(model()->bookmark_bar_node()->id());
  EXPECT_EQ("Can't modify the root bookmark folders.",
            api_test_utils::RunFunctionAndReturnError(
                cut_function.get(),
                base::StringPrintf("[[\"%s\"]]", node_id.c_str()), profile()));
}

TEST_F(BookmarkManagerPrivateApiUnitTest, RunOpenInNewTabFunction) {
  auto new_tab_function =
      base::MakeRefCounted<BookmarkManagerPrivateOpenInNewTabFunction>();
  std::string args = base::StringPrintf(R"(["%s", false])", node_id().c_str());
  ASSERT_TRUE(
      api_test_utils::RunFunction(new_tab_function.get(), args, profile()));

  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_EQ(url(), browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL());
}

TEST_F(BookmarkManagerPrivateApiUnitTest, RunOpenInNewTabFunctionFolder) {
  auto new_tab_function =
      base::MakeRefCounted<BookmarkManagerPrivateOpenInNewTabFunction>();
  std::string node_id =
      base::NumberToString(model()->bookmark_bar_node()->id());
  std::string args = base::StringPrintf(R"(["%s", false])", node_id.c_str());
  EXPECT_EQ("Cannot open a folder in a new tab.",
            api_test_utils::RunFunctionAndReturnError(new_tab_function.get(),
                                                      args, profile()));
}

TEST_F(BookmarkManagerPrivateApiUnitTest, RunOpenInNewWindowFunctionFolder) {
  auto new_window_function =
      base::MakeRefCounted<BookmarkManagerPrivateOpenInNewWindowFunction>();
  std::string node_id =
      base::NumberToString(model()->bookmark_bar_node()->id());
  std::string args = base::StringPrintf(R"([["%s"], false])", node_id.c_str());
  EXPECT_EQ("Cannot open a folder in a new window.",
            api_test_utils::RunFunctionAndReturnError(new_window_function.get(),
                                                      args, profile()));
}

TEST_F(BookmarkManagerPrivateApiUnitTest,
       RunOpenInNewWindowFunctionIncognitoDisabled) {
  // Incognito disabled.
  IncognitoModePrefs::SetAvailability(
      profile()->GetPrefs(), policy::IncognitoModeAvailability::kDisabled);

  auto new_window_function =
      base::MakeRefCounted<BookmarkManagerPrivateOpenInNewWindowFunction>();
  std::string args = base::StringPrintf(R"([["%s"], true])", node_id().c_str());
  EXPECT_EQ("Incognito mode is disabled.",
            api_test_utils::RunFunctionAndReturnError(new_window_function.get(),
                                                      args, profile()));
}

TEST_F(BookmarkManagerPrivateApiUnitTest,
       RunOpenInNewWindowFunctionIncognitoForced) {
  // Incognito forced.
  IncognitoModePrefs::SetAvailability(
      profile()->GetPrefs(), policy::IncognitoModeAvailability::kForced);

  auto new_window_function =
      base::MakeRefCounted<BookmarkManagerPrivateOpenInNewWindowFunction>();
  std::string args =
      base::StringPrintf(R"([["%s"], false])", node_id().c_str());
  EXPECT_EQ("Incognito mode is forced. Cannot open normal windows.",
            api_test_utils::RunFunctionAndReturnError(new_window_function.get(),
                                                      args, profile()));
}

TEST_F(BookmarkManagerPrivateApiUnitTest,
       RunOpenInNewWindowFunctionIncognitoIncompatibleNode) {
  const bookmarks::BookmarkNode* node = model()->AddURL(
      model()->other_node(), 0, u"history", GURL("chrome://history"));
  std::string node_id = base::NumberToString(node->id());

  auto new_window_function =
      base::MakeRefCounted<BookmarkManagerPrivateOpenInNewWindowFunction>();
  std::string args = base::StringPrintf(R"([["%s"], true])", node_id.c_str());
  EXPECT_EQ("Cannot open URL \"chrome://history/\" in an incognito window.",
            api_test_utils::RunFunctionAndReturnError(new_window_function.get(),
                                                      args, profile()));
}

}  // namespace extensions
