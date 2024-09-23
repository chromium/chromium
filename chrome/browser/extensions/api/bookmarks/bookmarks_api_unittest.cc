// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmarks/bookmarks_api.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "extensions/browser/api_test_utils.h"

namespace extensions {

class BookmarksApiUnittest : public ExtensionServiceTestBase {
 public:
  BookmarksApiUnittest() = default;
  BookmarksApiUnittest(const BookmarksApiUnittest&) = delete;
  BookmarksApiUnittest& operator=(const BookmarksApiUnittest&) = delete;

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();

    ExtensionServiceInitParams params;
    params.enable_bookmark_model = true;
    InitializeExtensionService(std::move(params));

    model_ = BookmarkModelFactory::GetForBrowserContext(profile());
    bookmarks::test::WaitForBookmarkModelToLoad(model_);

    const bookmarks::BookmarkNode* folder_node =
        model_->AddFolder(model_->other_node(), 0, u"Empty folder");
    const bookmarks::BookmarkNode* subfolder_node =
        model_->AddFolder(folder_node, 0, u"Empty subfolder");
    const bookmarks::BookmarkNode* url_node =
        model_->AddURL(model_->other_node(), 0, u"URL", url_);
    folder_node_id_ = base::NumberToString(folder_node->id());
    subfolder_node_id_ = base::NumberToString(subfolder_node->id());
    url_node_id_ = base::NumberToString(url_node->id());
  }

  raw_ptr<bookmarks::BookmarkModel> model() const { return model_; }
  std::string folder_node_id() const { return folder_node_id_; }
  std::string subfolder_node_id() const { return subfolder_node_id_; }
  std::string url_node_id() const { return url_node_id_; }
  const GURL url() const { return url_; }

 private:
  raw_ptr<bookmarks::BookmarkModel> model_ = nullptr;
  std::string folder_node_id_;
  std::string subfolder_node_id_;
  std::string url_node_id_;
  const GURL url_ = GURL("https://example.org");
};

// Tests that running updating a bookmark folder's url does not succeed.
// Regression test for https://crbug.com/818395.
TEST_F(BookmarksApiUnittest, Update) {
  auto update_function = base::MakeRefCounted<BookmarksUpdateFunction>();
  ASSERT_EQ(R"(Can't set URL of a bookmark folder.)",
            api_test_utils::RunFunctionAndReturnError(
                update_function.get(),
                base::StringPrintf(R"(["%s", {"url": "https://example.com"}])",
                                   folder_node_id().c_str()),
                profile()));
}

// Tests that attempting to creating a bookmark with a non-folder parent does
// not add the bookmark to that parent.
// Regression test for https://crbug.com/1441071.
TEST_F(BookmarksApiUnittest, Create) {
  auto create_function = base::MakeRefCounted<BookmarksCreateFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      create_function.get(),
      base::StringPrintf(R"([{"parentId": "%s"}])", url_node_id().c_str()),
      profile());
  ASSERT_EQ("Parameter 'parentId' does not specify a folder.", error);

  const bookmarks::BookmarkNode* url_node =
      model()->GetMostRecentlyAddedUserNodeForURL(url());
  ASSERT_TRUE(url_node->children().empty());
}

// Tests that attempting to move a bookmark to a non-folder parent does
// not add the bookmark to that parent.
// Regression test for https://crbug.com/1491227.
TEST_F(BookmarksApiUnittest, Move) {
  auto move_function = base::MakeRefCounted<BookmarksMoveFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      move_function.get(),
      base::StringPrintf(R"(["%s", {"parentId": "%s"}])",
                         folder_node_id().c_str(), url_node_id().c_str()),
      profile());
  ASSERT_EQ("Parameter 'parentId' does not specify a folder.", error);

  const bookmarks::BookmarkNode* url_node =
      model()->GetMostRecentlyAddedUserNodeForURL(url());
  ASSERT_TRUE(url_node->children().empty());
}

// Tests that attempting to move a bookmark to a non existent parent returns an
// error.
TEST_F(BookmarksApiUnittest, Move_NoParent) {
  auto move_function = base::MakeRefCounted<BookmarksMoveFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      move_function.get(),
      base::StringPrintf(R"(["%s", {"parentId": "1234"}])",
                         folder_node_id().c_str()),
      profile());
  ASSERT_EQ("Can't find parent bookmark for id.", error);

  const bookmarks::BookmarkNode* url_node =
      model()->GetMostRecentlyAddedUserNodeForURL(url());
  ASSERT_TRUE(url_node->children().empty());
}

// Tests that attempting to move a folder to itself returns an error.
TEST_F(BookmarksApiUnittest, Move_FolderToItself) {
  auto move_function = base::MakeRefCounted<BookmarksMoveFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      move_function.get(),
      base::StringPrintf(R"(["%s", {"parentId": "%s"}])",
                         folder_node_id().c_str(), folder_node_id().c_str()),
      profile());
  ASSERT_EQ("Can't move a folder to itself or its descendant.", error);
}

// Tests that attempting to move a folder to its descendant returns an error.
TEST_F(BookmarksApiUnittest, Move_FolderToDescendant) {
  auto move_function = base::MakeRefCounted<BookmarksMoveFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      move_function.get(),
      base::StringPrintf(R"(["%s", {"parentId": "%s"}])",
                         folder_node_id().c_str(), subfolder_node_id().c_str()),
      profile());
  ASSERT_EQ("Can't move a folder to itself or its descendant.", error);
}

}  // namespace extensions
