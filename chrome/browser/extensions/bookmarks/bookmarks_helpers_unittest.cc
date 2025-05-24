// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bookmarks/bookmarks_helpers.h"

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/extensions/bookmarks/bookmarks_error_constants.h"
#include "chrome/browser/sync/local_or_syncable_bookmark_sync_service_factory.h"
#include "chrome/common/extensions/api/bookmarks.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace extensions {

using api::bookmarks::BookmarkTreeNode;
using ::testing::Eq;
using ::testing::ExplainMatchResult;

namespace bookmarks_helpers {

// Matches a BookmarkTreeNode that represents a folder node with the given
// `expected_type` and `expected_unmodifiable`.
MATCHER_P2(MatchesFolder, expected_type, expected_unmodifiable, "") {
  return ExplainMatchResult(Eq(expected_type), arg.folder_type,
                            result_listener) &&
         ExplainMatchResult(Eq(expected_unmodifiable), arg.unmodifiable,
                            result_listener) &&
         ExplainMatchResult(Eq(std::nullopt), arg.url, result_listener);
}

class ExtensionBookmarksTest : public testing::Test {
 public:
  ExtensionBookmarksTest()
      : managed_(nullptr),
        model_(nullptr),
        node_(nullptr),
        node2_(nullptr),
        folder_(nullptr) {}

  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        ManagedBookmarkServiceFactory::GetInstance(),
        ManagedBookmarkServiceFactory::GetDefaultFactory());

    profile_ = profile_builder.Build();
    model_ = BookmarkModelFactory::GetForBrowserContext(profile_.get());
    managed_ = ManagedBookmarkServiceFactory::GetForProfile(profile_.get());
    bookmarks::test::WaitForBookmarkModelToLoad(model_);

    node_ = model_->AddURL(model_->other_node(), 0, u"Digg",
                           GURL("http://www.reddit.com"));
    model_->SetNodeMetaInfo(node_, "some_key1", "some_value1");
    model_->SetNodeMetaInfo(node_, "some_key2", "some_value2");
    model_->AddURL(model_->other_node(), 0, u"News",
                   GURL("http://www.foxnews.com"));
    folder_ = model_->AddFolder(model_->other_node(), 0, u"outer folder");
    model_->SetNodeMetaInfo(folder_, "some_key1", "some_value1");
    model_->AddFolder(folder_, 0, u"inner folder 1");
    model_->AddFolder(folder_, 0, u"inner folder 2");
    node2_ = model_->AddURL(folder_, 0, u"Digg", GURL("http://reddit.com"));
    model_->SetNodeMetaInfo(node2_, "some_key2", "some_value2");
    model_->AddURL(folder_, 0, u"CNet", GURL("http://cnet.com"));

    // Add a URL to the mobile node so that it is not hidden.
    model_->AddURL(model_->mobile_node(), 0, u"Mobile bookmark",
                   GURL("http://www.mobile.com"));

    // Add a URL to the managed node so that it is not hidden.
    model_->AddURL(managed_->managed_node(), 0, u"Managed bookmark",
                   GURL("http://www.managed.com"));
  }

  // A simple wrapper to get a single BookmarkTreeNode from a BookmarkNode (with
  // no recursion).
  BookmarkTreeNode GetSingleBookmarkTreeNode(const BookmarkNode* node) {
    CHECK(node);
    return GetBookmarkTreeNode(model_, managed_, node,
                               /*recurse=*/false,
                               /*only_folders=*/false);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<bookmarks::ManagedBookmarkService> managed_;
  raw_ptr<BookmarkModel> model_;
  raw_ptr<const BookmarkNode> node_;
  raw_ptr<const BookmarkNode> node2_;
  raw_ptr<const BookmarkNode> folder_;
};

TEST_F(ExtensionBookmarksTest, GetFullTreeFromRoot) {
  BookmarkTreeNode tree =
      GetBookmarkTreeNode(model_, managed_, model_->root_node(),
                          /*recurse=*/true,
                          /*only_folders=*/false);
  ASSERT_EQ(4U, tree.children->size());
}

TEST_F(ExtensionBookmarksTest, GetTreeFromOtherPermanentNode) {
  BookmarkTreeNode tree =
      GetBookmarkTreeNode(model_, managed_, model_->other_node(),
                          /*recurse=*/true,
                          /*only_folders=*/false);
  ASSERT_EQ(3U, tree.children->size());
}

TEST_F(ExtensionBookmarksTest, GetFoldersOnlyFromOtherPermanentNode) {
  BookmarkTreeNode tree =
      GetBookmarkTreeNode(model_, managed_, model_->other_node(),
                          /*recurse=*/true,
                          /*only_folders=*/true);
  ASSERT_EQ(1U, tree.children->size());
}

TEST_F(ExtensionBookmarksTest, GetSubtree) {
  BookmarkTreeNode tree = GetBookmarkTreeNode(model_, managed_, folder_,
                                              /*recurse=*/true,
                                              /*only_folders=*/false);
  ASSERT_EQ(4U, tree.children->size());
  const BookmarkTreeNode& digg = tree.children->at(1);
  ASSERT_EQ("Digg", digg.title);
}

TEST_F(ExtensionBookmarksTest, GetSubtreeFoldersOnly) {
  BookmarkTreeNode tree = GetBookmarkTreeNode(model_, managed_, folder_,
                                              /*recurse=*/true,
                                              /*only_folders=*/true);
  ASSERT_EQ(2U, tree.children->size());
  const BookmarkTreeNode& inner_folder = tree.children->at(1);
  ASSERT_EQ("inner folder 1", inner_folder.title);
}

TEST_F(ExtensionBookmarksTest, GetModifiableNode) {
  BookmarkTreeNode tree = GetBookmarkTreeNode(model_, managed_, node_,
                                              /*recurse=*/false,
                                              /*only_folders=*/false);
  EXPECT_EQ("Digg", tree.title);
  ASSERT_TRUE(tree.url);
  EXPECT_EQ("http://www.reddit.com/", *tree.url);
  EXPECT_EQ(api::bookmarks::BookmarkTreeNodeUnmodifiable::kNone,
            tree.unmodifiable);
  EXPECT_EQ(api::bookmarks::FolderType::kNone, tree.folder_type);
}

TEST_F(ExtensionBookmarksTest, GetManagedNode) {
  const BookmarkNode* managed_bookmark =
      model_->AddURL(managed_->managed_node(), 0, u"Chromium",
                     GURL("http://www.chromium.org/"));
  BookmarkTreeNode tree =
      GetBookmarkTreeNode(model_, managed_, managed_bookmark,
                          /*recurse=*/false,
                          /*only_folders=*/false);
  EXPECT_EQ("Chromium", tree.title);
  EXPECT_EQ("http://www.chromium.org/", *tree.url);
  EXPECT_EQ(api::bookmarks::BookmarkTreeNodeUnmodifiable::kManaged,
            tree.unmodifiable);
  EXPECT_EQ(api::bookmarks::FolderType::kNone, tree.folder_type);
}

TEST_F(ExtensionBookmarksTest, GetLocalPermanentAndManagedFolders) {
  EXPECT_THAT(
      GetSingleBookmarkTreeNode(model_->bookmark_bar_node()),
      MatchesFolder(api::bookmarks::FolderType::kBookmarksBar,
                    api::bookmarks::BookmarkTreeNodeUnmodifiable::kNone));
  EXPECT_THAT(
      GetSingleBookmarkTreeNode(model_->other_node()),
      MatchesFolder(api::bookmarks::FolderType::kOther,
                    api::bookmarks::BookmarkTreeNodeUnmodifiable::kNone));
  EXPECT_THAT(
      GetSingleBookmarkTreeNode(model_->mobile_node()),
      MatchesFolder(api::bookmarks::FolderType::kMobile,
                    api::bookmarks::BookmarkTreeNodeUnmodifiable::kNone));
  EXPECT_THAT(
      GetSingleBookmarkTreeNode(managed_->managed_node()),
      MatchesFolder(api::bookmarks::FolderType::kManaged,
                    api::bookmarks::BookmarkTreeNodeUnmodifiable::kManaged));
}

TEST_F(ExtensionBookmarksTest, GetAccountPermanentNodes) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};
  model_->CreateAccountPermanentFolders();
  // Add a URL to the mobile node so that it is not hidden.
  model_->AddURL(model_->account_mobile_node(), 0, u"Mobile bookmark",
                 GURL("http://www.mobile.com"));

  EXPECT_THAT(
      GetSingleBookmarkTreeNode(model_->account_bookmark_bar_node()),
      MatchesFolder(api::bookmarks::FolderType::kBookmarksBar,
                    api::bookmarks::BookmarkTreeNodeUnmodifiable::kNone));
  EXPECT_THAT(
      GetSingleBookmarkTreeNode(model_->account_other_node()),
      MatchesFolder(api::bookmarks::FolderType::kOther,
                    api::bookmarks::BookmarkTreeNodeUnmodifiable::kNone));
  EXPECT_THAT(
      GetSingleBookmarkTreeNode(model_->account_mobile_node()),
      MatchesFolder(api::bookmarks::FolderType::kMobile,
                    api::bookmarks::BookmarkTreeNodeUnmodifiable::kNone));
}

TEST_F(ExtensionBookmarksTest,
       GetTreePopulatesSyncingProperty_BookmarksInTransportModeDisabled) {
  // Check that local permanent system nodes are not syncing.
  EXPECT_FALSE(GetSingleBookmarkTreeNode(model_->bookmark_bar_node()).syncing);
  EXPECT_FALSE(GetSingleBookmarkTreeNode(model_->other_node()).syncing);
  EXPECT_FALSE(GetSingleBookmarkTreeNode(model_->mobile_node()).syncing);
  EXPECT_FALSE(GetSingleBookmarkTreeNode(managed_->managed_node()).syncing);

  // Check that non-permanent local nodes are not syncing.
  EXPECT_FALSE(GetSingleBookmarkTreeNode(node_).syncing);
  EXPECT_FALSE(GetSingleBookmarkTreeNode(folder_).syncing);
}

TEST_F(ExtensionBookmarksTest,
       GetTreePopulatesSyncingProperty_SyncFeatureEnabledIncludingBookmarks) {
  // Pretend sync-the-feature is on for bookmarks.
  LocalOrSyncableBookmarkSyncServiceFactory::GetForProfile(profile_.get())
      ->SetIsTrackingMetadataForTesting();

  // Check that local-or-syncable permanent nodes are syncing.
  EXPECT_TRUE(GetSingleBookmarkTreeNode(model_->bookmark_bar_node()).syncing);
  EXPECT_TRUE(GetSingleBookmarkTreeNode(model_->other_node()).syncing);
  EXPECT_TRUE(GetSingleBookmarkTreeNode(model_->mobile_node()).syncing);

  // Check that non-permanent local nodes are syncing.
  EXPECT_TRUE(GetSingleBookmarkTreeNode(node_).syncing);
  EXPECT_TRUE(GetSingleBookmarkTreeNode(folder_).syncing);

  // Managed nodes are never syncing.
  EXPECT_FALSE(GetSingleBookmarkTreeNode(managed_->managed_node()).syncing);
  const BookmarkNode* managed_bookmark =
      model_->AddURL(managed_->managed_node(), 0, u"Chromium",
                     GURL("http://www.chromium.org"));
  EXPECT_FALSE(GetSingleBookmarkTreeNode(managed_bookmark).syncing);
}

TEST_F(ExtensionBookmarksTest,
       GetTreePopulatesSyncingProperty_AccountNodesEnabled) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};
  model_->CreateAccountPermanentFolders();
  // Add a URL to the mobile node so that it is not hidden.
  model_->AddURL(model_->account_mobile_node(), 0, u"Mobile bookmark",
                 GURL("http://www.mobile.com"));

  // Check that local permanent nodes are not syncing.
  EXPECT_FALSE(GetSingleBookmarkTreeNode(model_->bookmark_bar_node()).syncing);
  EXPECT_FALSE(GetSingleBookmarkTreeNode(model_->other_node()).syncing);
  EXPECT_FALSE(GetSingleBookmarkTreeNode(model_->mobile_node()).syncing);
  EXPECT_FALSE(GetSingleBookmarkTreeNode(managed_->managed_node()).syncing);

  // Check that non-permanent local nodes are not syncing.
  EXPECT_FALSE(GetSingleBookmarkTreeNode(node_).syncing);
  EXPECT_FALSE(GetSingleBookmarkTreeNode(folder_).syncing);

  // Check that account permanent nodes are syncing.
  EXPECT_TRUE(
      GetSingleBookmarkTreeNode(model_->account_bookmark_bar_node()).syncing);
  EXPECT_TRUE(GetSingleBookmarkTreeNode(model_->account_other_node()).syncing);
  EXPECT_TRUE(GetSingleBookmarkTreeNode(model_->account_mobile_node()).syncing);

  // Check that non-permanent account nodes are syncing.
  const BookmarkNode* account_bookmark = model_->AddURL(
      model_->account_other_node(), 0, u"Digg", GURL("http://www.reddit.com"));
  const BookmarkNode* account_folder =
      model_->AddFolder(model_->account_other_node(), 0, u"outer folder");
  EXPECT_TRUE(GetSingleBookmarkTreeNode(account_bookmark).syncing);
  EXPECT_TRUE(GetSingleBookmarkTreeNode(account_folder).syncing);
}

TEST_F(ExtensionBookmarksTest, RemoveNodeInvalidId) {
  int64_t invalid_id = model_->next_node_id();
  std::string error;
  EXPECT_FALSE(RemoveNode(model_, managed_, invalid_id, true, &error));
  EXPECT_EQ(bookmarks_errors::kNoNodeError, error);
}

TEST_F(ExtensionBookmarksTest, RemoveNodePermanent) {
  std::string error;
  EXPECT_FALSE(
      RemoveNode(model_, managed_, model_->other_node()->id(), true, &error));
  EXPECT_EQ(bookmarks_errors::kModifySpecialError, error);
}

TEST_F(ExtensionBookmarksTest, RemoveNodeManaged) {
  const BookmarkNode* managed_bookmark =
      model_->AddURL(managed_->managed_node(), 0, u"Chromium",
                     GURL("http://www.chromium.org"));
  std::string error;
  EXPECT_FALSE(
      RemoveNode(model_, managed_, managed_bookmark->id(), true, &error));
  EXPECT_EQ(bookmarks_errors::kModifyManagedError, error);
}

TEST_F(ExtensionBookmarksTest, RemoveNodeNotRecursive) {
  std::string error;
  EXPECT_FALSE(RemoveNode(model_, managed_, folder_->id(), false, &error));
  EXPECT_EQ(bookmarks_errors::kFolderNotEmptyError, error);
}

TEST_F(ExtensionBookmarksTest, RemoveNodeRecursive) {
  EXPECT_EQ(3u, model_->other_node()->children().size());
  std::string error;
  EXPECT_TRUE(RemoveNode(model_, managed_, folder_->id(), true, &error));
  EXPECT_EQ(2u, model_->other_node()->children().size());
}

TEST_F(ExtensionBookmarksTest, GetMetaInfo) {
  base::Value::Dict id_to_meta_info_map;
  GetMetaInfo(*model_->other_node(), id_to_meta_info_map);
  EXPECT_EQ(8u, id_to_meta_info_map.size());

  // Verify top level node.
  {
    const base::Value* value = id_to_meta_info_map.Find(
        base::NumberToString(model_->other_node()->id()));
    ASSERT_NE(value, nullptr);
    ASSERT_TRUE(value->is_dict());
    const base::Value::Dict& dict = value->GetDict();
    EXPECT_EQ(0u, dict.size());
  }

  // Verify bookmark with two meta info key/value pairs.
  {
    const base::Value* value =
        id_to_meta_info_map.Find(base::NumberToString(node_->id()));
    ASSERT_NE(value, nullptr);
    ASSERT_TRUE(value->is_dict());
    const base::Value::Dict& dict = value->GetDict();
    EXPECT_EQ(2u, dict.size());
    ASSERT_TRUE(dict.FindString("some_key1"));
    EXPECT_EQ("some_value1", *(dict.FindString("some_key1")));
    ASSERT_TRUE(dict.FindString("some_key2"));
    EXPECT_EQ("some_value2", *(dict.FindString("some_key2")));
  }

  // Verify folder with one meta info key/value pair.
  {
    const base::Value* value =
        id_to_meta_info_map.Find(base::NumberToString(folder_->id()));
    ASSERT_NE(value, nullptr);
    ASSERT_TRUE(value->is_dict());
    const base::Value::Dict& dict = value->GetDict();
    EXPECT_EQ(1u, dict.size());
    ASSERT_TRUE(dict.FindString("some_key1"));
    EXPECT_EQ("some_value1", *(dict.FindString("some_key1")));
  }

  // Verify bookmark in a subfolder with one meta info key/value pairs.
  {
    const base::Value* value =
        id_to_meta_info_map.Find(base::NumberToString(node2_->id()));
    ASSERT_NE(value, nullptr);
    ASSERT_TRUE(value->is_dict());
    const base::Value::Dict& dict = value->GetDict();
    EXPECT_EQ(1u, dict.size());
    ASSERT_FALSE(dict.FindString("some_key1"));
    ASSERT_TRUE(dict.FindString("some_key2"));
    EXPECT_EQ("some_value2", *(dict.FindString("some_key2")));
  }
}

TEST_F(ExtensionBookmarksTest, PopulateBookmarkTreeNodeIndexConsistency) {
  const BookmarkNode* url1 = model_->AddURL(model_->bookmark_bar_node(), 0,
                                            u"URL1", GURL("http://url1.com"));
  const BookmarkNode* url2 = model_->AddURL(model_->bookmark_bar_node(), 1,
                                            u"URL2", GURL("http://url2.com"));
  const BookmarkNode* subfolder =
      model_->AddFolder(model_->bookmark_bar_node(), 2, u"Subfolder");
  const BookmarkNode* url3 =
      model_->AddURL(subfolder, 0, u"URL3", GURL("http://url3.com"));

  // Get the tree and verify indexes.
  BookmarkTreeNode tree =
      GetBookmarkTreeNode(model_, managed_, model_->bookmark_bar_node(),
                          /*recurse=*/true, /*only_folders=*/false);

  ASSERT_TRUE(tree.children.has_value());
  const std::vector<BookmarkTreeNode>& children = *tree.children;

  // Helper to find node by title.
  auto find_node = [](const std::vector<BookmarkTreeNode>& nodes,
                      const std::string& title) {
    return std::find_if(
        nodes.begin(), nodes.end(),
        [&title](const BookmarkTreeNode& node) { return node.title == title; });
  };

  // Verify each node's visible_index matches GetAPIIndexOf.
  auto url1_it = find_node(children, "URL1");
  ASSERT_NE(url1_it, children.end());
  EXPECT_EQ(GetAPIIndexOf(*url1), url1_it->index);

  auto url2_it = find_node(children, "URL2");
  ASSERT_NE(url2_it, children.end());
  EXPECT_EQ(GetAPIIndexOf(*url2), url2_it->index);

  auto subfolder_it = find_node(children, "Subfolder");
  ASSERT_NE(subfolder_it, children.end());
  EXPECT_EQ(GetAPIIndexOf(*subfolder), subfolder_it->index);

  ASSERT_TRUE(subfolder_it->children.has_value());
  auto url3_it = find_node(*subfolder_it->children, "URL3");
  ASSERT_NE(url3_it, subfolder_it->children->end());
  EXPECT_EQ(GetAPIIndexOf(*url3), url3_it->index);

  // Verify indexes reflect the node order.
  EXPECT_EQ(0U, url1_it->index);
  EXPECT_EQ(1U, url2_it->index);
  EXPECT_EQ(2U, subfolder_it->index);
  EXPECT_EQ(0U, url3_it->index);

  // Directly test PopulateBookmarkTreeNode.
  api::bookmarks::BookmarkTreeNode populated_node;
  PopulateBookmarkTreeNode(model_, managed_, url1,
                           /*recurse=*/false, /*only_folders=*/false,
                           std::nullopt, &populated_node);
  EXPECT_EQ(GetAPIIndexOf(*url1), populated_node.index);

  // Test recursive population.
  api::bookmarks::BookmarkTreeNode recursive_node;
  PopulateBookmarkTreeNode(model_, managed_, subfolder,
                           /*recurse=*/true, /*only_folders=*/false,
                           std::nullopt, &recursive_node);
  EXPECT_EQ(GetAPIIndexOf(*subfolder), recursive_node.index);
  ASSERT_TRUE(recursive_node.children.has_value());
  ASSERT_EQ(1U, recursive_node.children->size());
  EXPECT_EQ(GetAPIIndexOf(*url3), recursive_node.children->at(0).index);

  // Test only_folders with recursion.
  api::bookmarks::BookmarkTreeNode folders_node;
  PopulateBookmarkTreeNode(model_, managed_, model_->bookmark_bar_node(),
                           /*recurse=*/true, /*only_folders=*/true,
                           std::nullopt, &folders_node);
  ASSERT_TRUE(folders_node.children.has_value());
  ASSERT_EQ(1U, folders_node.children->size());  // Only the subfolder.
  EXPECT_EQ(GetAPIIndexOf(*subfolder), folders_node.children->at(0).index);
}

}  // namespace bookmarks_helpers
}  // namespace extensions
