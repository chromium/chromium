// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_parent_folder_children.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_test_helpers.h"
#include "chrome/browser/bookmarks/permanent_folder_ordering_tracker.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/sync/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

class BookmarkParentFolderChildrenTest : public testing::Test {
 public:
  BookmarkParentFolderChildrenTest() {
    model_ = std::make_unique<BookmarkModel>(
        std::make_unique<bookmarks::TestBookmarkClient>());
    model_->LoadEmptyForTest();
    merged_service_ = std::make_unique<BookmarkMergedSurfaceService>(
        model_.get(), /*managed_service*/ nullptr);
    merged_service_->LoadForTesting({});
  }

  BookmarkModel& bookmark_model() { return *model_.get(); }

  void AddUrlBookmarks(const BookmarkNode* parent,
                       size_t index,
                       size_t number_of_bookmarks) {
    for (size_t i = 0; i < number_of_bookmarks; i++) {
      bookmark_model().AddURL(parent, index + i, u"Title",
                              GURL("https://example.com"));
    }
  }

 private:
  std::unique_ptr<BookmarkModel> model_;
  std::unique_ptr<BookmarkMergedSurfaceService> merged_service_;
};

TEST_F(BookmarkParentFolderChildrenTest, FromNode) {
  const BookmarkNode* folder_node = bookmark_model().AddFolder(
      bookmark_model().bookmark_bar_node(), 0, u"folder");

  BookmarkParentFolderChildren folder_node_children(folder_node);
  EXPECT_EQ(folder_node_children.size(), 0u);

  size_t kChildrenSize = 5u;
  AddUrlBookmarks(folder_node, 0, /*number_of_bookmarks=*/kChildrenSize);
  EXPECT_EQ(folder_node_children.size(), kChildrenSize);
  ASSERT_EQ(folder_node->children().size(), folder_node_children.size());

  for (size_t i = 0; i < folder_node_children.size(); i++) {
    EXPECT_EQ(folder_node_children[i], folder_node->children()[i].get());
  }
}

TEST_F(BookmarkParentFolderChildrenTest, FromPermanentFolderOrderingTracker) {
  testing::NiceMock<MockPermanentFolderOrderingTrackerDelegate> delegate;
  PermanentFolderOrderingTracker tracker(
      &bookmark_model(), BookmarkNode::Type::BOOKMARK_BAR, &delegate);
  EXPECT_CALL(delegate, TrackedOrderingChanged);
  tracker.Init(/*in_order_node_ids=*/{});

  BookmarkParentFolderChildren bookmark_bar_children(&tracker);
  EXPECT_EQ(bookmark_bar_children.size(), 0u);

  size_t kChildrenSize = 5u;
  const BookmarkNode* bookmark_bar_node = bookmark_model().bookmark_bar_node();
  AddUrlBookmarks(bookmark_bar_node, 0,
                  /*number_of_bookmarks=*/kChildrenSize);
  EXPECT_EQ(bookmark_bar_children.size(), kChildrenSize);
  ASSERT_EQ(bookmark_bar_children.size(), bookmark_bar_node->children().size());

  for (size_t i = 0; i < bookmark_bar_children.size(); i++) {
    EXPECT_EQ(bookmark_bar_children[i], bookmark_bar_node->children()[i].get());
  }
}

TEST_F(BookmarkParentFolderChildrenTest, Iterator) {
  const BookmarkNode* folder_node_1 = bookmark_model().AddFolder(
      bookmark_model().bookmark_bar_node(), 0, u"folder");
  const BookmarkNode* folder_node_2 = bookmark_model().AddFolder(
      bookmark_model().bookmark_bar_node(), 1, u"folder");

  BookmarkParentFolderChildren folder_node_1_children(folder_node_1);
  EXPECT_EQ(folder_node_1_children.size(), 0u);
  BookmarkParentFolderChildren folder_node_2_children(folder_node_2);
  EXPECT_EQ(folder_node_2_children.size(), 0u);

  size_t kFolderNode1ChildrenSize = 5u;
  size_t kFolderNode2ChildrenSize = 3u;

  AddUrlBookmarks(folder_node_1, 0,
                  /*number_of_bookmarks=*/kFolderNode1ChildrenSize);
  AddUrlBookmarks(folder_node_2, 0,
                  /*number_of_bookmarks=*/kFolderNode2ChildrenSize);

  EXPECT_EQ(folder_node_1_children.size(), kFolderNode1ChildrenSize);
  ASSERT_EQ(folder_node_1->children().size(), folder_node_1_children.size());

  EXPECT_EQ(folder_node_2_children.size(), kFolderNode2ChildrenSize);
  ASSERT_EQ(folder_node_2->children().size(), folder_node_2_children.size());

  size_t index = 0;
  for (const BookmarkNode* node : folder_node_1_children) {
    EXPECT_EQ(node, folder_node_1->children()[index++].get());
  }

  index = 0;
  for (const BookmarkNode* node : folder_node_2_children) {
    EXPECT_EQ(node, folder_node_2->children()[index++].get());
  }

  EXPECT_NE(folder_node_1_children.begin(), folder_node_1_children.end());
  EXPECT_NE(folder_node_2_children.begin(), folder_node_2_children.end());

  EXPECT_NE(folder_node_1_children.begin(), folder_node_2_children.end());
}

}  // namespace
