// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"

#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using PermanentFolderType = BookmarkParentFolder::PermanentFolderType;
using bookmarks::BookmarkNode;
using bookmarks::test::AddNodesFromModelString;
using bookmarks::test::ModelStringFromNode;

class BookmarkMergedSurfaceServiceTest : public testing::Test {
 public:
  BookmarkMergedSurfaceServiceTest()
      : model_(bookmarks::TestBookmarkClient::CreateModel()),
        service_(std::make_unique<BookmarkMergedSurfaceService>(model_.get())) {
  }

  ~BookmarkMergedSurfaceServiceTest() override = default;

  BookmarkMergedSurfaceService& service() { return *service_; }
  bookmarks::BookmarkModel& model() { return *model_; }

 private:
  std::unique_ptr<bookmarks::BookmarkModel> model_;
  std::unique_ptr<BookmarkMergedSurfaceService> service_;
};

TEST_F(BookmarkMergedSurfaceServiceTest, GetChildrenCount) {
  EXPECT_EQ(
      service().GetChildrenCount(BookmarkParentFolder::BookmarkBarFolder()),
      0u);
  EXPECT_EQ(service().GetChildrenCount(BookmarkParentFolder::OtherFolder()),
            0u);
  EXPECT_EQ(service().GetChildrenCount(BookmarkParentFolder::MobileFolder()),
            0u);

  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");
  EXPECT_EQ(
      service().GetChildrenCount(BookmarkParentFolder::BookmarkBarFolder()),
      4u);
  const BookmarkNode* folder_f1 =
      model().bookmark_bar_node()->children()[3].get();
  EXPECT_EQ(service().GetChildrenCount(
                BookmarkParentFolder::FromNonPermanentNode(folder_f1)),
            3u);
  const BookmarkNode* folder_f2 = folder_f1->children()[2].get();
  EXPECT_EQ(service().GetChildrenCount(
                BookmarkParentFolder::FromNonPermanentNode(folder_f2)),
            1u);

  AddNodesFromModelString(&model(), model().other_node(), "1 2 3 ");
  EXPECT_EQ(service().GetChildrenCount(BookmarkParentFolder::OtherFolder()),
            3u);

  AddNodesFromModelString(&model(), model().mobile_node(), "4 5 6 ");
  EXPECT_EQ(service().GetChildrenCount(BookmarkParentFolder::MobileFolder()),
            3u);
}

TEST_F(BookmarkMergedSurfaceServiceTest, IsPermanentNodeOfType) {
  EXPECT_TRUE(service().IsPermanentNodeOfType(model().other_node(),
                                              PermanentFolderType::kOtherNode));

  EXPECT_TRUE(BookmarkMergedSurfaceService::IsPermanentNodeOfType(
      model().mobile_node(), PermanentFolderType::kMobileNode));

  EXPECT_FALSE(BookmarkMergedSurfaceService::IsPermanentNodeOfType(
      model().other_node(), PermanentFolderType::kMobileNode));

  AddNodesFromModelString(&model(), model().other_node(), "1 2 3 ");
  EXPECT_EQ(model().other_node()->children().size(), 3u);
  for (const auto& node : model().other_node()->children()) {
    EXPECT_FALSE(BookmarkMergedSurfaceService::IsPermanentNodeOfType(
        node.get(), PermanentFolderType::kOtherNode));
  }
}

TEST_F(BookmarkMergedSurfaceServiceTest, MoveToPermanentFolder) {
  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "1 2 3 ");
  AddNodesFromModelString(&model(), model().other_node(), "4 5 6 ");

  // Move node "2" in bookmark bar to be after "4" in other node.
  const BookmarkNode* node = model().bookmark_bar_node()->children()[1].get();
  service().Move(node, BookmarkParentFolder::OtherFolder(), 1);
  EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()), "1 3 ");
  EXPECT_EQ(ModelStringFromNode(model().other_node()), "4 2 5 6 ");
}

TEST_F(BookmarkMergedSurfaceServiceTest, MoveToBookmarkNode) {
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ] ");
  AddNodesFromModelString(&model(), model().other_node(), "6 7 8 ");

  // Move node "8" from other node to node "f1" after "4".
  const BookmarkNode* node_to_move = model().other_node()->children()[2].get();
  const BookmarkNode* destination =
      model().bookmark_bar_node()->children()[3].get();
  service().Move(node_to_move,
                 BookmarkParentFolder::FromNonPermanentNode(destination), 1);

  EXPECT_EQ(ModelStringFromNode(model().bookmark_bar_node()),
            "1 2 3 f1:[ 4 8 5 ] ");
  EXPECT_EQ(ModelStringFromNode(model().other_node()), "6 7 ");
}

}  // namespace
