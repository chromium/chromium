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
#include "chrome/common/extensions/api/bookmarks.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/sync/base/features.h"
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

    folder_node_ = model_->AddFolder(model_->other_node(), 0, u"Empty folder");
    const bookmarks::BookmarkNode* subfolder_node =
        model_->AddFolder(folder_node_, 0, u"Empty subfolder");
    const bookmarks::BookmarkNode* url_node =
        model_->AddURL(model_->other_node(), 0, u"URL", url_);
    folder_node_id_ = base::NumberToString(folder_node_->id());
    subfolder_node_id_ = base::NumberToString(subfolder_node->id());
    url_node_id_ = base::NumberToString(url_node->id());
  }

  raw_ptr<bookmarks::BookmarkModel> model() const { return model_; }
  const bookmarks::BookmarkNode* folder_node() const { return folder_node_; }
  std::string folder_node_id() const { return folder_node_id_; }
  std::string subfolder_node_id() const { return subfolder_node_id_; }
  std::string url_node_id() const { return url_node_id_; }
  const GURL url() const { return url_; }

 private:
  raw_ptr<bookmarks::BookmarkModel> model_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> folder_node_ = nullptr;
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

// Tests that attempting to create a bookmark with no parent folder specified
// succeeds when only local/syncable bookmarks are available.
TEST_F(BookmarksApiUnittest, Create_NoParentLocal) {
  auto create_function = base::MakeRefCounted<BookmarksCreateFunction>();
  base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          create_function.get(), R"([{"title": "New folder"}])", profile())
          .value();
  api::bookmarks::BookmarkTreeNode result_node =
      extensions::api::bookmarks::BookmarkTreeNode::FromValue(result).value();

  // The new folder should be added as the last child of the local other node.
  EXPECT_EQ(result_node.parent_id,
            base::NumberToString(model()->other_node()->id()));
  EXPECT_EQ(result_node.index,
            model()->other_node()->children().size() - 1);
}

// Tests that attempting to create a bookmark with no parent folder specified
// succeeds and uses the account bookmarks folder when the user is signed in
// with bookmarks in transport mode.
TEST_F(BookmarksApiUnittest, Create_NoParentAccount) {
  base::test::ScopedFeatureList scoped_feature_list{
      syncer::kSyncEnableBookmarksInTransportMode};
  model()->CreateAccountPermanentFolders();

  auto create_function = base::MakeRefCounted<BookmarksCreateFunction>();
  base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
      create_function.get(), R"([{"title": "New folder"}])", profile()).value();
  api::bookmarks::BookmarkTreeNode result_node =
      extensions::api::bookmarks::BookmarkTreeNode::FromValue(result).value();

  // The new folder should be added as the last child of the account other node.
  EXPECT_EQ(result_node.parent_id,
            base::NumberToString(model()->account_other_node()->id()));
  EXPECT_EQ(result_node.index,
            model()->account_other_node()->children().size() - 1);
}

// Tests creating a bookmark with a valid parent specified.
TEST_F(BookmarksApiUnittest, Create_ValidParent) {
  auto create_function = base::MakeRefCounted<BookmarksCreateFunction>();
  base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
      create_function.get(),
      absl::StrFormat(R"([{"parentId": "%lu", "title": "New folder"}])",
                      folder_node()->id()),
      profile()).value();
  api::bookmarks::BookmarkTreeNode result_node =
      extensions::api::bookmarks::BookmarkTreeNode::FromValue(result).value();

  // The new folder should be added as the last child of the parent folder.
  EXPECT_EQ(result_node.parent_id, folder_node_id());
  EXPECT_EQ(result_node.index, folder_node()->children().size() - 1);
}

// Tests that attempting to creating a bookmark with a non-folder parent does
// not add the bookmark to that parent.
// Regression test for https://crbug.com/1441071.
TEST_F(BookmarksApiUnittest, Create_NonFolderParent) {
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

// Tests that moving from local to account storage is allowed.
TEST_F(BookmarksApiUnittest, Move_LocalToAccount) {
  base::test::ScopedFeatureList scoped_feature_list{
      syncer::kSyncEnableBookmarksInTransportMode};
  model()->CreateAccountPermanentFolders();

  ASSERT_TRUE(model()->IsLocalOnlyNode(*folder_node()));

  auto move_function = base::MakeRefCounted<BookmarksMoveFunction>();
  base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          move_function.get(),
          absl::StrFormat(R"(["%lu", {"parentId": "%lu"}])",
                          folder_node()->id(),
                          model()->account_other_node()->id()),
          profile()).value();
  api::bookmarks::BookmarkTreeNode result_node =
      extensions::api::bookmarks::BookmarkTreeNode::FromValue(result).value();

  EXPECT_EQ(result_node.parent_id,
            base::NumberToString(model()->account_other_node()->id()));
  EXPECT_EQ(result_node.index, 0);
  EXPECT_EQ(model()->account_other_node()->children()[0].get(), folder_node());
}

// Tests that attempting to move a bookmark to a non-folder parent does
// not add the bookmark to that parent.
// Regression test for https://crbug.com/1491227.
TEST_F(BookmarksApiUnittest, Move_NonFolderParent) {
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
TEST_F(BookmarksApiUnittest, Move_NonExistentParent) {
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
