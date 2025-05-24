// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_parent_folder.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/signin/public/base/signin_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using PermanentFolderType = BookmarkParentFolder::PermanentFolderType;
using bookmarks::BookmarkNode;
using bookmarks::test::AddNodesFromModelString;

TEST(BookmarkParentFolderTest, FromPermanentFolderType) {
  {
    BookmarkParentFolder folder = BookmarkParentFolder::BookmarkBarFolder();
    EXPECT_FALSE(folder.HoldsNonPermanentFolder());
    EXPECT_FALSE(folder.as_non_permanent_folder());

    ASSERT_TRUE(folder.as_permanent_folder());
    EXPECT_EQ(*folder.as_permanent_folder(),
              PermanentFolderType::kBookmarkBarNode);
  }
  {
    BookmarkParentFolder folder = BookmarkParentFolder::OtherFolder();
    EXPECT_FALSE(folder.HoldsNonPermanentFolder());
    EXPECT_FALSE(folder.as_non_permanent_folder());

    ASSERT_TRUE(folder.as_permanent_folder());
    EXPECT_EQ(*folder.as_permanent_folder(), PermanentFolderType::kOtherNode);
  }
  {
    BookmarkParentFolder folder = BookmarkParentFolder::MobileFolder();
    EXPECT_FALSE(folder.HoldsNonPermanentFolder());
    EXPECT_FALSE(folder.as_non_permanent_folder());

    ASSERT_TRUE(folder.as_permanent_folder());
    EXPECT_EQ(*folder.as_permanent_folder(), PermanentFolderType::kMobileNode);
  }
  {
    BookmarkParentFolder folder = BookmarkParentFolder::ManagedFolder();
    EXPECT_FALSE(folder.HoldsNonPermanentFolder());
    EXPECT_FALSE(folder.as_non_permanent_folder());

    ASSERT_TRUE(folder.as_permanent_folder());
    EXPECT_EQ(*folder.as_permanent_folder(), PermanentFolderType::kManagedNode);
  }
}

TEST(BookmarkParentFolderTest, FromFolderNode) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};
  auto client = std::make_unique<bookmarks::TestBookmarkClient>();
  bookmarks::BookmarkNode* managed_node = client->EnableManagedNode();
  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModelWithClient(std::move(client));
  model->CreateAccountPermanentFolders();
  const BookmarkNode* folder_node =
      model->AddFolder(model->bookmark_bar_node(), 0, u"folder");

  EXPECT_EQ(BookmarkParentFolder::FromFolderNode(model->bookmark_bar_node()),
            BookmarkParentFolder::BookmarkBarFolder());
  EXPECT_EQ(BookmarkParentFolder::FromFolderNode(model->other_node()),
            BookmarkParentFolder::OtherFolder());
  EXPECT_EQ(BookmarkParentFolder::FromFolderNode(model->mobile_node()),
            BookmarkParentFolder::MobileFolder());
  EXPECT_EQ(BookmarkParentFolder::FromFolderNode(managed_node),
            BookmarkParentFolder::ManagedFolder());

  EXPECT_EQ(
      BookmarkParentFolder::FromFolderNode(model->account_bookmark_bar_node()),
      BookmarkParentFolder::BookmarkBarFolder());
  EXPECT_EQ(BookmarkParentFolder::FromFolderNode(model->account_other_node()),
            BookmarkParentFolder::OtherFolder());
  EXPECT_EQ(BookmarkParentFolder::FromFolderNode(model->account_mobile_node()),
            BookmarkParentFolder::MobileFolder());

  BookmarkParentFolder folder =
      BookmarkParentFolder::FromFolderNode(folder_node);
  EXPECT_TRUE(folder.HoldsNonPermanentFolder());
  EXPECT_EQ(folder.as_non_permanent_folder(), folder_node);
  EXPECT_FALSE(folder.as_permanent_folder());
}

TEST(BookmarkParentFolderTest, HasDirectChildNode) {
  std::unique_ptr<bookmarks::BookmarkModel> model =
      std::make_unique<bookmarks::BookmarkModel>(
          std::make_unique<bookmarks::TestBookmarkClient>());
  model->LoadEmptyForTest();
  AddNodesFromModelString(model.get(), model->bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  AddNodesFromModelString(model.get(), model->other_node(), "6 7 8 ");

  const BookmarkNode* f1 = model->bookmark_bar_node()->children()[3].get();
  {
    BookmarkParentFolder folder = BookmarkParentFolder::BookmarkBarFolder();
    for (const auto& node : model->bookmark_bar_node()->children()) {
      EXPECT_TRUE(folder.HasDirectChildNode(node.get()));
    }
    EXPECT_FALSE(folder.HasDirectChildNode(f1->children()[0].get()));
    EXPECT_FALSE(folder.HasDirectChildNode(model->other_node()));
    EXPECT_FALSE(
        folder.HasDirectChildNode(model->other_node()->children()[1].get()));
  }

  {
    BookmarkParentFolder folder = BookmarkParentFolder::FromFolderNode(f1);
    for (const auto& node : f1->children()) {
      EXPECT_TRUE(folder.HasDirectChildNode(node.get()));
    }
    EXPECT_FALSE(folder.HasDirectChildNode(model->bookmark_bar_node()));
    EXPECT_FALSE(folder.HasDirectChildNode(model->other_node()));
    EXPECT_FALSE(folder.HasDirectChildNode(f1));
  }
}

TEST(BookmarkParentFolderTest, HasAncestor) {
  std::unique_ptr<bookmarks::BookmarkModel> model =
      std::make_unique<bookmarks::BookmarkModel>(
          std::make_unique<bookmarks::TestBookmarkClient>());
  model->LoadEmptyForTest();
  AddNodesFromModelString(model.get(), model->bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 1 ] ] ");

  const BookmarkNode* f1 = model->bookmark_bar_node()->children()[3].get();
  const BookmarkParentFolder f1_folder(
      BookmarkParentFolder::FromFolderNode(f1));
  EXPECT_TRUE(f1_folder.HasAncestor(BookmarkParentFolder::FromFolderNode(f1)));
  EXPECT_TRUE(f1_folder.HasAncestor(BookmarkParentFolder::BookmarkBarFolder()));
  EXPECT_FALSE(f1_folder.HasAncestor(BookmarkParentFolder::OtherFolder()));

  const BookmarkParentFolder f2_folder(
      BookmarkParentFolder::FromFolderNode(f1->children()[2].get()));
  EXPECT_FALSE(f1_folder.HasAncestor(f2_folder));
  EXPECT_TRUE(f2_folder.HasAncestor(f2_folder));
  EXPECT_TRUE(f2_folder.HasAncestor(BookmarkParentFolder::BookmarkBarFolder()));
  EXPECT_FALSE(f2_folder.HasAncestor(BookmarkParentFolder::OtherFolder()));

  EXPECT_FALSE(
      BookmarkParentFolder::BookmarkBarFolder().HasAncestor(f1_folder));
  EXPECT_TRUE(BookmarkParentFolder::BookmarkBarFolder().HasAncestor(
      BookmarkParentFolder::BookmarkBarFolder()));
  EXPECT_FALSE(BookmarkParentFolder::BookmarkBarFolder().HasAncestor(
      BookmarkParentFolder::OtherFolder()));
}

}  // namespace
