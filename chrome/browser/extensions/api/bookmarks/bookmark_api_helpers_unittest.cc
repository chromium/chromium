// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmarks/bookmark_api_helpers.h"

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/extensions/bookmarks/bookmarks_error_constants.h"
#include "chrome/common/extensions/api/bookmarks.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace extensions {

using api::bookmarks::BookmarkTreeNode;

namespace bookmark_api_helpers {

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
  BookmarkTreeNode tree = GetBookmarkTreeNode(managed_, model_->other_node(),
                                              true,    // Recurse.
                                              false);  // Not only folders.
  ASSERT_EQ(3U, tree.children->size());
}

TEST_F(ExtensionBookmarksTest, GetFoldersOnlyFromRoot) {
  BookmarkTreeNode tree = GetBookmarkTreeNode(managed_, model_->other_node(),
                                              true,   // Recurse.
                                              true);  // Only folders.
  ASSERT_EQ(1U, tree.children->size());
}

TEST_F(ExtensionBookmarksTest, GetSubtree) {
  BookmarkTreeNode tree = GetBookmarkTreeNode(managed_, folder_,
                                              true,    // Recurse.
                                              false);  // Not only folders.
  ASSERT_EQ(4U, tree.children->size());
  const BookmarkTreeNode& digg = tree.children->at(1);
  ASSERT_EQ("Digg", digg.title);
}

TEST_F(ExtensionBookmarksTest, GetSubtreeFoldersOnly) {
  BookmarkTreeNode tree = GetBookmarkTreeNode(managed_, folder_,
                                              true,   // Recurse.
                                              true);  // Only folders.
  ASSERT_EQ(2U, tree.children->size());
  const BookmarkTreeNode& inner_folder = tree.children->at(1);
  ASSERT_EQ("inner folder 1", inner_folder.title);
}

TEST_F(ExtensionBookmarksTest, GetModifiableNode) {
  BookmarkTreeNode tree = GetBookmarkTreeNode(managed_, node_,
                                              false,   // Recurse.
                                              false);  // Only folders.
  EXPECT_EQ("Digg", tree.title);
  ASSERT_TRUE(tree.url);
  EXPECT_EQ("http://www.reddit.com/", *tree.url);
  EXPECT_EQ(api::bookmarks::BookmarkTreeNodeUnmodifiable::kNone,
            tree.unmodifiable);
}

TEST_F(ExtensionBookmarksTest, GetManagedNode) {
  const BookmarkNode* managed_bookmark =
      model_->AddURL(managed_->managed_node(), 0, u"Chromium",
                     GURL("http://www.chromium.org/"));
  BookmarkTreeNode tree = GetBookmarkTreeNode(managed_, managed_bookmark,
                                              false,   // Recurse.
                                              false);  // Only folders.
  EXPECT_EQ("Chromium", tree.title);
  EXPECT_EQ("http://www.chromium.org/", *tree.url);
  EXPECT_EQ(api::bookmarks::BookmarkTreeNodeUnmodifiable::kManaged,
            tree.unmodifiable);
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

}  // namespace bookmark_api_helpers
}  // namespace extensions
