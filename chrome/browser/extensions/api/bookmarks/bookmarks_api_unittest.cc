// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmarks/bookmarks_api.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/extensions/bookmarks/bookmarks_error_constants.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/common/extensions/api/bookmarks.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/sync/base/features.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/test_event_router_observer.h"

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
    subfolder_node_ = model_->AddFolder(folder_node_, 0, u"Empty subfolder");
    url_node_ = model_->AddURL(model_->other_node(), 0, u"URL", url_);
    folder_node_id_ = base::NumberToString(folder_node_->id());
  }

  raw_ptr<bookmarks::BookmarkModel> model() const { return model_; }
  const bookmarks::BookmarkNode* folder_node() const { return folder_node_; }
  std::string folder_node_id() const { return folder_node_id_; }
  const bookmarks::BookmarkNode* subfolder_node() const {
    return subfolder_node_;
  }
  const bookmarks::BookmarkNode* url_node() const { return url_node_; }
  const GURL url() const { return url_; }

 private:
  raw_ptr<bookmarks::BookmarkModel> model_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> folder_node_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> subfolder_node_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> url_node_ = nullptr;
  std::string folder_node_id_;
  const GURL url_ = GURL("https://example.org");
};

// Tests that running updating a bookmark folder's url does not succeed.
// Regression test for https://crbug.com/818395.
TEST_F(BookmarksApiUnittest, Update) {
  auto update_function = base::MakeRefCounted<BookmarksUpdateFunction>();
  ASSERT_EQ(R"(Can't set URL of a bookmark folder.)",
            api_test_utils::RunFunctionAndReturnError(
                update_function.get(),
                absl::StrFormat(R"(["%s", {"url": "https://example.com"}])",
                                folder_node_id().c_str()),
                profile()));
}

// Tests that attempting to create a bookmark with no parent folder specified
// succeeds when only local/syncable bookmarks are available.
TEST_F(BookmarksApiUnittest, Create_NoParentLocalOnly) {
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
  EXPECT_EQ(result_node.index, model()->other_node()->children().size() - 1);
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
          create_function.get(), R"([{"title": "New folder"}])", profile())
          .value();
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
          profile())
          .value();
  api::bookmarks::BookmarkTreeNode result_node =
      extensions::api::bookmarks::BookmarkTreeNode::FromValue(result).value();

  // The new folder should be added as the last child of the parent folder.
  EXPECT_EQ(result_node.parent_id, folder_node_id());
  EXPECT_EQ(result_node.index, folder_node()->children().size() - 1);
}

// Tests creating a bookmark with a valid parent specified.
TEST_F(BookmarksApiUnittest,
       Create_SucceedsInLocalParentWhenBothLocalAndAccountBookmarksExist) {
  base::test::ScopedFeatureList scoped_feature_list{
      syncer::kSyncEnableBookmarksInTransportMode};
  model()->CreateAccountPermanentFolders();

  auto create_function = base::MakeRefCounted<BookmarksCreateFunction>();
  base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          create_function.get(),
          absl::StrFormat(R"([{"parentId": "%lu", "title": "New folder"}])",
                          folder_node()->id()),
          profile())
          .value();
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
      absl::StrFormat(R"([{"parentId": "%d"}])", url_node()->id()), profile());
  ASSERT_EQ("Parameter 'parentId' does not specify a folder.", error);

  const bookmarks::BookmarkNode* url_node =
      model()->GetMostRecentlyAddedUserNodeForURL(url());
  ASSERT_TRUE(url_node->children().empty());
}

// Tests that attempting to create a bookmark with a parent that is not
// visible fails.
TEST_F(BookmarksApiUnittest, Create_NonExistentParent) {
  auto create_function = base::MakeRefCounted<BookmarksCreateFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      create_function.get(), R"([{"parentId": "1234"}])", profile());
  EXPECT_EQ("Can't find parent bookmark for id.", error);
}

// Tests that attempting to create a bookmark with a parent that is not
// visible fails.
// TODO(crbug.com/392614318): Enforce visibility on write operations.
TEST_F(BookmarksApiUnittest, DISABLED_Create_NonVisibleParent) {
  // The mobile node is not visible, because it is empty.
  ASSERT_FALSE(model()->mobile_node()->IsVisible());

  auto create_function = base::MakeRefCounted<BookmarksCreateFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      create_function.get(),
      absl::StrFormat(R"([{"parentId": "%d"}])", model()->mobile_node()->id()),
      profile());
  EXPECT_EQ("Parameter 'parentId' does not specify a folder.", error);

  EXPECT_TRUE(model()->mobile_node()->children().empty());
}

TEST_F(BookmarksApiUnittest,
       Get_SucceedsForLocalPermanentFolderWhenNoAccountFolders) {
  auto get_function = base::MakeRefCounted<BookmarksGetFunction>();
  base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          get_function.get(),
          absl::StrFormat(R"(["%lu"])", model()->other_node()->id()), profile())
          .value();

  std::vector<int64_t> result_bookmark_ids;
  std::ranges::transform(
      result.GetList(), std::back_inserter(result_bookmark_ids),
      [](const base::Value& value) {
        int64_t id;
        CHECK(base::StringToInt64(
            extensions::api::bookmarks::BookmarkTreeNode::FromValue(value)->id,
            &id));
        return id;
      });

  EXPECT_THAT(result_bookmark_ids,
              testing::ElementsAre(model()->other_node()->id()));
}

TEST_F(BookmarksApiUnittest,
       Get_SucceedsForNonEmptyLocalPermanentFolderWhenAccountFolders) {
  base::test::ScopedFeatureList scoped_feature_list{
      syncer::kSyncEnableBookmarksInTransportMode};
  model()->CreateAccountPermanentFolders();

  auto get_function = base::MakeRefCounted<BookmarksGetFunction>();
  base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          get_function.get(),
          absl::StrFormat(R"(["%lu"])", model()->other_node()->id()), profile())
          .value();

  std::vector<int64_t> result_bookmark_ids;
  std::ranges::transform(
      result.GetList(), std::back_inserter(result_bookmark_ids),
      [](const base::Value& value) {
        int64_t id;
        CHECK(base::StringToInt64(
            extensions::api::bookmarks::BookmarkTreeNode::FromValue(value)->id,
            &id));
        return id;
      });

  EXPECT_THAT(result_bookmark_ids,
              testing::ElementsAre(model()->other_node()->id()));
}

TEST_F(BookmarksApiUnittest,
       GetTree_SucceedsForLocalPermanentFolderWhenNoAccountFolders) {
  auto get_tree_function = base::MakeRefCounted<BookmarksGetTreeFunction>();
  const base::Value result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          get_tree_function.get(), R"([])", profile())
          .value();
  std::vector<int64_t> result_bookmark_ids;
  std::ranges::transform(
      result.GetList(), std::back_inserter(result_bookmark_ids),
      [](const base::Value& value) {
        int64_t id;
        CHECK(base::StringToInt64(
            extensions::api::bookmarks::BookmarkTreeNode::FromValue(value)->id,
            &id));
        return id;
      });

  // The result should contain a single root node. Check that its children
  // include the three permanent folders, plus the non-permanent folder/url.
  ASSERT_EQ(result.GetList().size(), 1u);
  auto root_node = extensions::api::bookmarks::BookmarkTreeNode::FromValue(
      result.GetList()[0]);
  EXPECT_EQ(root_node->id, "0");

  ASSERT_EQ(root_node->children.value().size(), 2u);
  EXPECT_EQ(root_node->children.value()[0].id,
            base::NumberToString(model()->bookmark_bar_node()->id()));
  EXPECT_EQ(root_node->children.value()[1].id,
            base::NumberToString(model()->other_node()->id()));

  auto& other_node = root_node->children.value()[1];
  ASSERT_EQ(other_node.children.value().size(), 2u);
  EXPECT_EQ(other_node.children.value()[0].id,
            base::NumberToString(url_node()->id()));
  EXPECT_EQ(other_node.children.value()[1].id,
            base::NumberToString(folder_node()->id()));
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
          profile())
          .value();
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
      absl::StrFormat(R"(["%d", {"parentId": "%d"}])", folder_node()->id(),
                      url_node()->id()),
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
      absl::StrFormat(R"(["%s", {"parentId": "1234"}])",
                      folder_node_id().c_str()),
      profile());
  ASSERT_EQ("Can't find parent bookmark for id.", error);

  const bookmarks::BookmarkNode* url_node =
      model()->GetMostRecentlyAddedUserNodeForURL(url());
  ASSERT_TRUE(url_node->children().empty());
}

// Tests that attempting to move a bookmark to a non-visible parent returns an
// error.
// TODO(crbug.com/392614318): Enforce visibility on write operations.
TEST_F(BookmarksApiUnittest, DISABLED_Move_NonVisibleParent) {
  // The mobile node is not visible, because it is empty.
  ASSERT_FALSE(model()->mobile_node()->IsVisible());

  auto move_function = base::MakeRefCounted<BookmarksMoveFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      move_function.get(),
      absl::StrFormat(R"(["%s", {"parentId": "%d"}])", folder_node_id().c_str(),
                      model()->mobile_node()->id()),
      profile());
  EXPECT_EQ("Can't find parent bookmark for id.", error);

  EXPECT_TRUE(model()->mobile_node()->children().empty());
}

// Tests that attempting to move a folder to itself returns an error.
TEST_F(BookmarksApiUnittest, Move_FolderToItself) {
  auto move_function = base::MakeRefCounted<BookmarksMoveFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      move_function.get(),
      absl::StrFormat(R"(["%s", {"parentId": "%s"}])", folder_node_id().c_str(),
                      folder_node_id().c_str()),
      profile());
  ASSERT_EQ("Can't move a folder to itself or its descendant.", error);
}

// Tests that attempting to move a folder to its descendant returns an error.
TEST_F(BookmarksApiUnittest, Move_FolderToDescendant) {
  auto move_function = base::MakeRefCounted<BookmarksMoveFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
      move_function.get(),
      absl::StrFormat(R"(["%d", {"parentId": "%d"}])", folder_node()->id(),
                      subfolder_node()->id()),
      profile());
  ASSERT_EQ("Can't move a folder to itself or its descendant.", error);
}

}  // namespace extensions
