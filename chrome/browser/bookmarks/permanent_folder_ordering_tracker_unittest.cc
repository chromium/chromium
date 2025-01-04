// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/permanent_folder_ordering_tracker.h"

#include "base/test/scoped_feature_list.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/sync/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::test::AddNodesFromModelString;
using ::testing::ElementsAre;

namespace {

class PermanentFolderOrderingTrackerTest : public testing::Test {
 public:
  PermanentFolderOrderingTrackerTest() { ResetModel(); }

  BookmarkModel& model() { return *model_; }

  void ResetModel() {
    model_ = std::make_unique<BookmarkModel>(
        std::make_unique<bookmarks::TestBookmarkClient>());
  }

 private:
  base::test::ScopedFeatureList features_{
      syncer::kSyncEnableBookmarksInTransportMode};
  std::unique_ptr<BookmarkModel> model_;
};

TEST_F(PermanentFolderOrderingTrackerTest,
       GetUnderlyingPermanentNodesModelLoadedNoAccountNodes) {
  model().LoadEmptyForTest();
  ASSERT_FALSE(model().account_bookmark_bar_node());
  {
    PermanentFolderOrderingTracker tracker(&model(),
                                           BookmarkNode::BOOKMARK_BAR);
    EXPECT_THAT(tracker.GetUnderlyingPermanentNodes(),
                ElementsAre(model().bookmark_bar_node()));
  }

  {
    PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::OTHER_NODE);
    EXPECT_THAT(tracker.GetUnderlyingPermanentNodes(),
                ElementsAre(model().other_node()));
  }

  {
    PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::MOBILE);
    EXPECT_THAT(tracker.GetUnderlyingPermanentNodes(),
                ElementsAre(model().mobile_node()));
  }
}

TEST_F(PermanentFolderOrderingTrackerTest,
       GetUnderlyingPermanentNodesModelNotLoadedNoAccountNodes) {
  PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::BOOKMARK_BAR);
  model().LoadEmptyForTest();
  ASSERT_FALSE(model().account_bookmark_bar_node());
  EXPECT_THAT(tracker.GetUnderlyingPermanentNodes(),
              ElementsAre(model().bookmark_bar_node()));
}

TEST_F(PermanentFolderOrderingTrackerTest,
       GetUnderlyingPermanentNodesModelLoadedAccountNodes) {
  model().LoadEmptyForTest();
  model().CreateAccountPermanentFolders();
  ASSERT_TRUE(model().account_bookmark_bar_node());

  {
    PermanentFolderOrderingTracker tracker(&model(),
                                           BookmarkNode::BOOKMARK_BAR);
    EXPECT_THAT(tracker.GetUnderlyingPermanentNodes(),
                ElementsAre(model().account_bookmark_bar_node(),
                            model().bookmark_bar_node()));
  }

  {
    PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::OTHER_NODE);
    EXPECT_THAT(
        tracker.GetUnderlyingPermanentNodes(),
        ElementsAre(model().account_other_node(), model().other_node()));
  }

  {
    PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::MOBILE);
    EXPECT_THAT(
        tracker.GetUnderlyingPermanentNodes(),
        ElementsAre(model().account_mobile_node(), model().mobile_node()));
  }
}

TEST_F(PermanentFolderOrderingTrackerTest,
       GetUnderlyingPermanentNodesAccountNodesCreatedLater) {
  {
    PermanentFolderOrderingTracker tracker(&model(),
                                           BookmarkNode::BOOKMARK_BAR);
    model().LoadEmptyForTest();
    ASSERT_FALSE(model().account_bookmark_bar_node());
    ASSERT_THAT(tracker.GetUnderlyingPermanentNodes(),
                ElementsAre(model().bookmark_bar_node()));
    model().CreateAccountPermanentFolders();
    ASSERT_TRUE(model().account_bookmark_bar_node());
    EXPECT_THAT(tracker.GetUnderlyingPermanentNodes(),
                ElementsAre(model().account_bookmark_bar_node(),
                            model().bookmark_bar_node()));
  }

  ResetModel();
  {
    PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::OTHER_NODE);
    model().LoadEmptyForTest();
    ASSERT_FALSE(model().account_other_node());
    ASSERT_THAT(tracker.GetUnderlyingPermanentNodes(),
                ElementsAre(model().other_node()));
    model().CreateAccountPermanentFolders();
    ASSERT_TRUE(model().account_other_node());
    EXPECT_THAT(
        tracker.GetUnderlyingPermanentNodes(),
        ElementsAre(model().account_other_node(), model().other_node()));
  }

  ResetModel();
  {
    PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::MOBILE);
    model().LoadEmptyForTest();
    ASSERT_FALSE(model().account_mobile_node());
    ASSERT_THAT(tracker.GetUnderlyingPermanentNodes(),
                ElementsAre(model().mobile_node()));
    model().CreateAccountPermanentFolders();
    ASSERT_TRUE(model().account_mobile_node());
    EXPECT_THAT(
        tracker.GetUnderlyingPermanentNodes(),
        ElementsAre(model().account_mobile_node(), model().mobile_node()));
  }
}

TEST_F(PermanentFolderOrderingTrackerTest,
       GetUnderlyingPermanentNodesAccountPermanentFoldersRemoved) {
  model().LoadEmptyForTest();
  model().CreateAccountPermanentFolders();
  PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::BOOKMARK_BAR);
  ASSERT_THAT(tracker.GetUnderlyingPermanentNodes(),
              ElementsAre(model().account_bookmark_bar_node(),
                          model().bookmark_bar_node()));

  // Remove account permanent folders.
  model().RemoveAccountPermanentFolders();
  EXPECT_THAT(tracker.GetUnderlyingPermanentNodes(),
              ElementsAre(model().bookmark_bar_node()));
}

TEST_F(PermanentFolderOrderingTrackerTest, GetIndexOfNoAccountFolder) {
  model().LoadEmptyForTest();
  PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::BOOKMARK_BAR);
  ASSERT_THAT(tracker.GetUnderlyingPermanentNodes(),
              ElementsAre(model().bookmark_bar_node()));
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 ]");
  EXPECT_EQ(tracker.GetChildrenCount(),
            model().bookmark_bar_node()->children().size());
  for (size_t i = 0; i < model().bookmark_bar_node()->children().size(); i++) {
    const BookmarkNode* node = model().bookmark_bar_node()->children()[i].get();
    EXPECT_EQ(tracker.GetIndexOf(node), i);
  }
}

TEST_F(PermanentFolderOrderingTrackerTest, OrderingDefault) {
  model().LoadEmptyForTest();
  model().CreateAccountPermanentFolders();
  PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::OTHER_NODE);
  ASSERT_THAT(tracker.GetUnderlyingPermanentNodes(),
              ElementsAre(model().account_other_node(), model().other_node()));
  AddNodesFromModelString(&model(), model().other_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");
  AddNodesFromModelString(&model(), model().account_other_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");
  EXPECT_EQ(tracker.GetChildrenCount(), 8u);
  for (size_t i = 0; i < model().account_other_node()->children().size(); i++) {
    const BookmarkNode* node =
        model().account_other_node()->children()[i].get();
    EXPECT_EQ(tracker.GetIndexOf(node), i);
    EXPECT_EQ(tracker.GetNodeAtIndex(i), node);
  }

  size_t account_nodes_size = model().account_other_node()->children().size();
  ASSERT_EQ(account_nodes_size, 4u);
  for (size_t i = 0; i < model().other_node()->children().size(); i++) {
    const BookmarkNode* node = model().other_node()->children()[i].get();
    EXPECT_EQ(tracker.GetIndexOf(node), i + 4);
    EXPECT_EQ(tracker.GetNodeAtIndex(i + 4), node);
  }
}

TEST_F(PermanentFolderOrderingTrackerTest, OrderingCustomOrder) {
  model().LoadEmptyForTest();
  model().CreateAccountPermanentFolders();
  PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::BOOKMARK_BAR);
  ASSERT_THAT(tracker.GetUnderlyingPermanentNodes(),
              ElementsAre(model().account_bookmark_bar_node(),
                          model().bookmark_bar_node()));
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");
  EXPECT_EQ(tracker.GetChildrenCount(), 8u);

  // {L0, A0, A1, A2, L1, L2, L3, A3}.
  std::vector<raw_ptr<const BookmarkNode>> custom_ordering{
      model().bookmark_bar_node()->children()[0].get(),
      model().account_bookmark_bar_node()->children()[0].get(),
      model().account_bookmark_bar_node()->children()[1].get(),
      model().account_bookmark_bar_node()->children()[2].get(),
      model().bookmark_bar_node()->children()[1].get(),
      model().bookmark_bar_node()->children()[2].get(),
      model().bookmark_bar_node()->children()[3].get(),
      model().account_bookmark_bar_node()->children()[3].get()};
  tracker.SetNodesOrderingForTesting(custom_ordering);

  for (size_t i = 0; i < custom_ordering.size(); i++) {
    EXPECT_EQ(tracker.GetIndexOf(custom_ordering[i]), i);
    EXPECT_EQ(tracker.GetNodeAtIndex(i), custom_ordering[i]);
  }

  // Insert local nodes.
  const BookmarkNode* node = model().AddURL(
      model().bookmark_bar_node(), 1, u"Title", GURL("https://example.com"));
  // {L0, L01, A0, A1, A2, L1, L2, L3, A3}.
  EXPECT_EQ(tracker.GetIndexOf(node), 1u);
  EXPECT_EQ(tracker.GetNodeAtIndex(1), node);

  node = model().AddURL(model().bookmark_bar_node(), 3, u"Title",
                        GURL("https://example.com"));
  // 2 local nodes, 3 account, 1 local then `node`.
  // {L0, L01, A0, A1, A2, L1, L11, L2, L3, A3}.
  EXPECT_EQ(tracker.GetIndexOf(node), 6u);
  EXPECT_EQ(tracker.GetNodeAtIndex(6), node);

  // Insert account nodes.
  node = model().AddURL(model().account_bookmark_bar_node(), 0, u"Title",
                        GURL("https://example.com"));
  // {L0, L01, A00, A0, A1, A2, L1, L11, L2, L3, A3}.
  EXPECT_EQ(tracker.GetIndexOf(node), 2u);
  EXPECT_EQ(tracker.GetNodeAtIndex(2), node);

  node = model().AddURL(model().account_bookmark_bar_node(), 4, u"Title",
                        GURL("https://example.com"));
  // {L0, L01, A00, A0, A1, A2, A21, L1, L11, L2, L3, A3}.
  EXPECT_EQ(tracker.GetIndexOf(node), 6u);
  EXPECT_EQ(tracker.GetNodeAtIndex(6), node);
}

TEST_F(PermanentFolderOrderingTrackerTest, OrderingLocalOnly) {
  model().LoadEmptyForTest();
  PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::BOOKMARK_BAR);
  ASSERT_THAT(tracker.GetUnderlyingPermanentNodes(),
              ElementsAre(model().bookmark_bar_node()));
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");

  EXPECT_EQ(tracker.GetChildrenCount(),
            model().bookmark_bar_node()->children().size());
  for (size_t i = 0; i < model().bookmark_bar_node()->children().size(); i++) {
    const BookmarkNode* node = model().bookmark_bar_node()->children()[i].get();
    EXPECT_EQ(tracker.GetIndexOf(node), i);
    EXPECT_EQ(tracker.GetNodeAtIndex(i), node);
  }

  // Remove node.
  model().Remove(model().bookmark_bar_node()->children()[1].get(),
                 bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);

  // Add node.
  model().AddURL(model().bookmark_bar_node(), 1, u"Title",
                 GURL("https://example.com"));
  EXPECT_EQ(tracker.GetChildrenCount(),
            model().bookmark_bar_node()->children().size());
  for (size_t i = 0; i < model().bookmark_bar_node()->children().size(); i++) {
    const BookmarkNode* node = model().bookmark_bar_node()->children()[i].get();
    EXPECT_EQ(tracker.GetIndexOf(node), i);
    EXPECT_EQ(tracker.GetNodeAtIndex(i), node);
  }
}

TEST_F(PermanentFolderOrderingTrackerTest, OrderingAccountOnly) {
  model().LoadEmptyForTest();
  PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::BOOKMARK_BAR);
  model().CreateAccountPermanentFolders();
  ASSERT_THAT(tracker.GetUnderlyingPermanentNodes(),
              ElementsAre(model().account_bookmark_bar_node(),
                          model().bookmark_bar_node()));
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");

  EXPECT_EQ(tracker.GetChildrenCount(),
            model().account_bookmark_bar_node()->children().size());
  for (size_t i = 0; i < model().account_bookmark_bar_node()->children().size();
       i++) {
    const BookmarkNode* node =
        model().account_bookmark_bar_node()->children()[i].get();
    EXPECT_EQ(tracker.GetIndexOf(node), i);
    EXPECT_EQ(tracker.GetNodeAtIndex(i), node);
  }

  // Remove node.
  model().Remove(model().account_bookmark_bar_node()->children()[1].get(),
                 bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);

  // Add node.
  model().AddURL(model().account_bookmark_bar_node(), 1, u"Title",
                 GURL("https://example.com"));
  EXPECT_EQ(tracker.GetChildrenCount(),
            model().account_bookmark_bar_node()->children().size());
  for (size_t i = 0; i < model().account_bookmark_bar_node()->children().size();
       i++) {
    const BookmarkNode* node =
        model().account_bookmark_bar_node()->children()[i].get();
    EXPECT_EQ(tracker.GetIndexOf(node), i);
    EXPECT_EQ(tracker.GetNodeAtIndex(i), node);
  }
}

TEST_F(PermanentFolderOrderingTrackerTest, OrderingExistingLocal) {
  model().LoadEmptyForTest();
  PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::BOOKMARK_BAR);
  model().CreateAccountPermanentFolders();
  ASSERT_THAT(tracker.GetUnderlyingPermanentNodes(),
              ElementsAre(model().account_bookmark_bar_node(),
                          model().bookmark_bar_node()));
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");
  EXPECT_EQ(tracker.GetChildrenCount(),
            model().bookmark_bar_node()->children().size());

  // Add first account bookmark.
  const BookmarkNode* node =
      model().AddURL(model().account_bookmark_bar_node(), 0, u"Title x",
                     GURL("https://example.com"));
  EXPECT_EQ(tracker.GetIndexOf(node), 0u);
  EXPECT_EQ(tracker.GetNodeAtIndex(0), node);

  node = model().AddURL(model().account_bookmark_bar_node(), 1, u"Title y",
                        GURL("https://example.com"));
  EXPECT_EQ(tracker.GetIndexOf(node), 1u);
  EXPECT_EQ(tracker.GetNodeAtIndex(1), node);

  EXPECT_EQ(
      tracker.GetIndexOf(model().bookmark_bar_node()->children()[0].get()), 2u);
  // Insert local nodes.
  node = model().AddURL(model().bookmark_bar_node(), 0, u"Title z",
                        GURL("https://example.com"));
  EXPECT_EQ(tracker.GetIndexOf(node), 2u);
  EXPECT_EQ(tracker.GetNodeAtIndex(2), node);

  node = model().AddURL(model().bookmark_bar_node(), 4, u"Title w",
                        GURL("https://example.com"));
  EXPECT_EQ(tracker.GetIndexOf(node), 6u);
  EXPECT_EQ(tracker.GetNodeAtIndex(6), node);
  EXPECT_EQ(tracker.GetChildrenCount(),
            model().bookmark_bar_node()->children().size() +
                model().account_bookmark_bar_node()->children().size());
}

TEST_F(PermanentFolderOrderingTrackerTest, OrderingAccountThenLocal) {
  model().LoadEmptyForTest();
  PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::BOOKMARK_BAR);
  model().CreateAccountPermanentFolders();
  ASSERT_THAT(tracker.GetUnderlyingPermanentNodes(),
              ElementsAre(model().account_bookmark_bar_node(),
                          model().bookmark_bar_node()));
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");

  // Add local bookmarks.
  const BookmarkNode* node = model().AddURL(
      model().bookmark_bar_node(), 0, u"Title x", GURL("https://example.com"));
  EXPECT_EQ(tracker.GetIndexOf(node), 4u);
  EXPECT_EQ(tracker.GetNodeAtIndex(4), node);

  node = model().AddURL(model().bookmark_bar_node(), 1, u"Title y",
                        GURL("https://example.com"));
  EXPECT_EQ(tracker.GetIndexOf(node), 5u);
  EXPECT_EQ(tracker.GetNodeAtIndex(5), node);

  EXPECT_EQ(tracker.GetIndexOf(
                model().account_bookmark_bar_node()->children()[0].get()),
            0u);
  // Insert account nodes.
  node = model().AddURL(model().account_bookmark_bar_node(), 0, u"Title z",
                        GURL("https://example.com"));
  EXPECT_EQ(tracker.GetIndexOf(node), 0u);
  EXPECT_EQ(tracker.GetNodeAtIndex(0), node);
}

TEST_F(PermanentFolderOrderingTrackerTest, BookmarkAllUserNodesRemoved) {
  model().LoadEmptyForTest();
  model().CreateAccountPermanentFolders();
  PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::OTHER_NODE);
  ASSERT_THAT(tracker.GetUnderlyingPermanentNodes(),
              ElementsAre(model().account_other_node(), model().other_node()));
  AddNodesFromModelString(&model(), model().other_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");
  AddNodesFromModelString(&model(), model().account_other_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");

  model().RemoveAllUserBookmarks(FROM_HERE);
  const BookmarkNode* node = model().AddURL(model().other_node(), 0, u"Title x",
                                            GURL("https://example.com"));
  EXPECT_EQ(tracker.GetIndexOf(node), 0u);
  EXPECT_EQ(tracker.GetNodeAtIndex(0), node);
  EXPECT_EQ(tracker.GetChildrenCount(), 1u);
}

TEST_F(PermanentFolderOrderingTrackerTest, RemoveAccountPermanentFolders) {
  model().LoadEmptyForTest();
  model().CreateAccountPermanentFolders();
  PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::OTHER_NODE);
  ASSERT_THAT(tracker.GetUnderlyingPermanentNodes(),
              ElementsAre(model().account_other_node(), model().other_node()));
  AddNodesFromModelString(&model(), model().other_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");
  AddNodesFromModelString(&model(), model().account_other_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");

  const BookmarkNode* node = model().other_node()->children()[0].get();
  EXPECT_EQ(tracker.GetIndexOf(node),
            model().account_other_node()->children().size());
  EXPECT_EQ(
      tracker.GetNodeAtIndex(model().account_other_node()->children().size()),
      node);

  model().RemoveAccountPermanentFolders();
  EXPECT_THAT(tracker.GetUnderlyingPermanentNodes(),
              ElementsAre(model().other_node()));
  EXPECT_EQ(tracker.GetIndexOf(node), 0u);
  EXPECT_EQ(tracker.GetNodeAtIndex(0), node);
  EXPECT_EQ(tracker.GetChildrenCount(),
            model().other_node()->children().size());
}

TEST_F(PermanentFolderOrderingTrackerTest,
       BookmarkMovedOldParentNonTrackedNode) {
  model().LoadEmptyForTest();
  model().CreateAccountPermanentFolders();
  PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::BOOKMARK_BAR);
  AddNodesFromModelString(&model(), model().other_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");

  const BookmarkNode* node = model().bookmark_bar_node()->children()[0].get();
  EXPECT_EQ(tracker.GetIndexOf(node), 0u);

  // Move node from non-tracked `other_node` to `bookmark_bar_node()`.
  const BookmarkNode* node_to_be_moved =
      model().other_node()->children()[1].get();
  model().Move(node_to_be_moved, model().bookmark_bar_node(), 0u);
  EXPECT_EQ(tracker.GetIndexOf(node_to_be_moved), 0u);
  EXPECT_EQ(tracker.GetIndexOf(node), 1u);

  // Move node from non-tracked `other_node` to `account_bar_node()`.
  node_to_be_moved = model().bookmark_bar_node()->children()[1].get();
  model().Move(node_to_be_moved, model().account_bookmark_bar_node(), 0u);
  EXPECT_EQ(tracker.GetIndexOf(node_to_be_moved), 0u);
  EXPECT_EQ(
      tracker.GetIndexOf(model().bookmark_bar_node()->children()[0].get()),
      model().account_bookmark_bar_node()->children().size());

  // Move another node.
  node_to_be_moved = model().bookmark_bar_node()->children()[0].get();
  model().Move(node_to_be_moved, model().account_bookmark_bar_node(), 1u);
  EXPECT_EQ(tracker.GetIndexOf(node_to_be_moved), 1u);
  EXPECT_EQ(
      tracker.GetIndexOf(model().bookmark_bar_node()->children()[0].get()),
      model().account_bookmark_bar_node()->children().size());
}

TEST_F(PermanentFolderOrderingTrackerTest,
       BookmarkMovedNewParentNonTrackedNode) {
  model().LoadEmptyForTest();
  model().CreateAccountPermanentFolders();
  PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::BOOKMARK_BAR);
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "1 2 ");
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");
  EXPECT_EQ(
      tracker.GetIndexOf(model().bookmark_bar_node()->children()[0].get()),
      model().account_bookmark_bar_node()->children().size());

  model().Move(model().account_bookmark_bar_node()->children()[0].get(),
               model().other_node(), 0u);
  EXPECT_EQ(
      tracker.GetIndexOf(model().bookmark_bar_node()->children()[0].get()), 1u);

  model().Move(model().account_bookmark_bar_node()->children()[0].get(),
               model().other_node(), 0u);
  EXPECT_EQ(
      tracker.GetIndexOf(model().bookmark_bar_node()->children()[0].get()), 0u);

  const BookmarkNode* node = model().bookmark_bar_node()->children()[1].get();
  EXPECT_EQ(tracker.GetIndexOf(node), 1u);
  model().Move(model().bookmark_bar_node()->children()[0].get(),
               model().other_node(), 0u);
  EXPECT_EQ(tracker.GetIndexOf(node), 0u);
}

TEST_F(PermanentFolderOrderingTrackerTest,
       BookmarkMovedOldAndNewParentTrackedNodes) {
  model().LoadEmptyForTest();
  model().CreateAccountPermanentFolders();
  PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::BOOKMARK_BAR);
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "1 2 3 ");
  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "1 2 3 ");

  // Move node from tracked node to another tracked node.

  const BookmarkNode* node_to_be_moved =
      model().bookmark_bar_node()->children()[0].get();
  EXPECT_EQ(tracker.GetIndexOf(node_to_be_moved), 3u);

  model().Move(node_to_be_moved, model().account_bookmark_bar_node(), 1u);
  EXPECT_EQ(tracker.GetIndexOf(node_to_be_moved), 1u);

  model().Move(node_to_be_moved, model().bookmark_bar_node(), 0u);
  EXPECT_EQ(tracker.GetIndexOf(node_to_be_moved), 3u);
}

TEST_F(PermanentFolderOrderingTrackerTest, BookmarkMovedCustomOrder) {
  model().LoadEmptyForTest();
  model().CreateAccountPermanentFolders();
  PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::BOOKMARK_BAR);
  ASSERT_THAT(tracker.GetUnderlyingPermanentNodes(),
              ElementsAre(model().account_bookmark_bar_node(),
                          model().bookmark_bar_node()));
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");
  AddNodesFromModelString(&model(), model().other_node(), "X Y Z ");

  // {L0, A0, A1, A2, L1, L2, L3, A3}.
  std::vector<raw_ptr<const BookmarkNode>> custom_ordering{
      model().bookmark_bar_node()->children()[0].get(),
      model().account_bookmark_bar_node()->children()[0].get(),
      model().account_bookmark_bar_node()->children()[1].get(),
      model().account_bookmark_bar_node()->children()[2].get(),
      model().bookmark_bar_node()->children()[1].get(),
      model().bookmark_bar_node()->children()[2].get(),
      model().bookmark_bar_node()->children()[3].get(),
      model().account_bookmark_bar_node()->children()[3].get()};
  tracker.SetNodesOrderingForTesting(custom_ordering);

  // Move to a tracked node.
  const BookmarkNode* node_to_be_moved =
      model().other_node()->children()[0].get();
  model().Move(node_to_be_moved, model().bookmark_bar_node(), 1u);
  // Inserted at the end of an existing block.
  // {L0, X, A0, A1, A2, L1, L2, L3, A3}.
  EXPECT_EQ(tracker.GetIndexOf(node_to_be_moved), 1u);

  node_to_be_moved = model().other_node()->children()[0].get();
  model().Move(node_to_be_moved, model().account_bookmark_bar_node(), 0u);
  // Inserted at the beginning of an existing block.
  // {L0, X(L), Y(A), A0, A1, A2, L1, L2, L3, A3}.
  EXPECT_EQ(tracker.GetIndexOf(node_to_be_moved), 2u);

  node_to_be_moved = model().other_node()->children()[0].get();
  model().Move(node_to_be_moved, model().bookmark_bar_node(), 3u);
  // Inserted at the middle of an existing block.
  // {L0, X(L), Y(A), A0, A1, A2, L1, Z(L), L2, L3, A3}.
  EXPECT_EQ(tracker.GetIndexOf(node_to_be_moved), 7u);

  // Move from `bookmark_bar_node` to `account_bookmark_bar_node`.
  model().Move(node_to_be_moved, model().account_bookmark_bar_node(),
               model().account_bookmark_bar_node()->children().size());
  // {L0, X(L), Y(A), A0, A1, A2, L1, L2, L3, A3, Z(A)}.
  EXPECT_EQ(tracker.GetIndexOf(node_to_be_moved), 10u);

  // Move within `account_bookmark_bar_node`.
  model().Move(node_to_be_moved, model().account_bookmark_bar_node(), 0u);
  // {L0, X(L), Z(A), Y(A), A0, A1, A2, L1, L2, L3, A3}.
  EXPECT_EQ(tracker.GetIndexOf(node_to_be_moved), 2u);
}

TEST_F(PermanentFolderOrderingTrackerTest, MoveToReorderTrackedNodes) {
  model().LoadEmptyForTest();
  model().CreateAccountPermanentFolders();
  PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::BOOKMARK_BAR);
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "A1 A2 ");
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");
  {
    // Move node `3`
    const BookmarkNode* node = model().bookmark_bar_node()->children()[2].get();
    // {A1 A2 1 2 3 f1 }
    EXPECT_EQ(tracker.GetIndexOf(node), 4u);
    // Move to the end of the list.
    tracker.MoveToIndex(node, 6u);
    // {A1 A2 1 2 f1 3 }
    EXPECT_EQ(tracker.GetIndexOf(node), 5u);
    EXPECT_EQ(node->parent(), model().bookmark_bar_node());
    EXPECT_EQ(node->parent()->GetIndexOf(node), 3u);
  }

  {
    // Move `A1` to the end.
    const BookmarkNode* node =
        model().account_bookmark_bar_node()->children()[0].get();
    // {A1 A2 1 2 f1 3 }
    EXPECT_EQ(tracker.GetIndexOf(node), 0u);
    // Move to the end of the list.
    tracker.MoveToIndex(node, 6u);
    // {A2 1 2 f1 3 A1 }
    EXPECT_EQ(tracker.GetIndexOf(node), 5u);
    EXPECT_EQ(node->parent(), model().account_bookmark_bar_node());
    EXPECT_EQ(node->parent()->GetIndexOf(node), 1u);
  }

  {
    // Move `f1` to the beginning of the list.
    const BookmarkNode* node = model().bookmark_bar_node()->children()[2].get();
    // {A2 1 2 f1 3 A1 }
    EXPECT_EQ(tracker.GetIndexOf(node), 3u);
    tracker.MoveToIndex(node, 0u);
    EXPECT_EQ(tracker.GetIndexOf(node), 0u);
    // {f1 A2 1 2 3 A1 }
    EXPECT_EQ(node->parent()->GetIndexOf(node), 0u);
  }

  {
    // Move `2` in the middle to the right (only in storage move is required).
    const BookmarkNode* node = model().bookmark_bar_node()->children()[2].get();
    // {f1 A2 1 2 3 A1 }
    EXPECT_EQ(tracker.GetIndexOf(node), 3u);
    tracker.MoveToIndex(node, 5u);
    // {f1 A2 1 3 2 A1 }
    EXPECT_EQ(tracker.GetIndexOf(node), 4u);
    EXPECT_EQ(node->parent()->GetIndexOf(node), 3u);

    // Move `2` to the right (in storage move not needed).
    tracker.MoveToIndex(node, 6u);
    // {f1 A2 1 3 A1 2}
    EXPECT_EQ(tracker.GetIndexOf(node), 5u);
    EXPECT_EQ(node->parent()->GetIndexOf(node), 3u);
  }

  {
    // Move `A1` to the left
    const BookmarkNode* node =
        model().account_bookmark_bar_node()->children()[1].get();
    // {f1 A2 1 3 A1 2}
    EXPECT_EQ(tracker.GetIndexOf(node), 4u);
    tracker.MoveToIndex(node, 1u);
    // {f1 A1 A2 1 3 2}
    EXPECT_EQ(tracker.GetIndexOf(node), 1u);
    EXPECT_EQ(node->parent()->GetIndexOf(node), 0u);
  }

  {
    // No Reorder is needed.
    const BookmarkNode* node =
        model().account_bookmark_bar_node()->children()[1].get();
    // {f1 A1 A2 1 3 2}
    EXPECT_EQ(tracker.GetIndexOf(node), 2u);
    tracker.MoveToIndex(node, 3u);
    EXPECT_EQ(tracker.GetIndexOf(node), 2u);
    tracker.MoveToIndex(node, 2u);
    EXPECT_EQ(tracker.GetIndexOf(node), 2u);
  }
}

TEST_F(PermanentFolderOrderingTrackerTest, MoveAddsNewAccountTrackedNodes) {
  model().LoadEmptyForTest();
  model().CreateAccountPermanentFolders();
  PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::BOOKMARK_BAR);
  AddNodesFromModelString(&model(), model().account_other_node(), "X Y Z W ");
  AddNodesFromModelString(&model(), model().bookmark_bar_node(), "1 2 ");
  // AddNodesFromModelString(&model(), model().bookmark_bar_node(),
  //                         "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");
  EXPECT_EQ(tracker.GetChildrenCount(), 2u);
  {
    const BookmarkNode* node =
        model().account_other_node()->children()[0].get();
    // {1 2 }
    tracker.MoveToIndex(node, 1u);
    // {1 X 2 }
    EXPECT_EQ(tracker.GetIndexOf(node), 1u);
    EXPECT_EQ(node->parent(), model().account_bookmark_bar_node());
    EXPECT_EQ(node->parent()->GetIndexOf(node), 0u);
  }

  {
    // Move `Y` to the beginning of the list.
    const BookmarkNode* node =
        model().account_other_node()->children()[0].get();
    // {1 X 2 }
    tracker.MoveToIndex(node, 0u);
    // {Y 1 X 2 }
    EXPECT_EQ(tracker.GetIndexOf(node), 0u);
    EXPECT_EQ(node->parent(), model().account_bookmark_bar_node());
    EXPECT_EQ(node->parent()->GetIndexOf(node), 0u);
  }

  {
    // Move `Z` to the end of the list.
    const BookmarkNode* node =
        model().account_other_node()->children()[0].get();
    // {Y 1 X 2 }
    tracker.MoveToIndex(node, 4u);
    // {Y 1 X 2 Z }
    EXPECT_EQ(tracker.GetIndexOf(node), 4u);
    EXPECT_EQ(node->parent(), model().account_bookmark_bar_node());
    EXPECT_EQ(node->parent()->GetIndexOf(node), 2u);
  }

  {
    // Move `W` to be before `Z`.
    const BookmarkNode* node =
        model().account_other_node()->children()[0].get();
    // {Y 1 X 2 Z }
    tracker.MoveToIndex(node, 4u);
    // {Y 1 X 2 W Z }
    EXPECT_EQ(tracker.GetIndexOf(node), 4u);
    EXPECT_EQ(node->parent(), model().account_bookmark_bar_node());
    EXPECT_EQ(node->parent()->GetIndexOf(node), 2u);
  }
}

TEST_F(PermanentFolderOrderingTrackerTest, MoveAddsNewLocalTrackedNodes) {
  model().LoadEmptyForTest();
  model().CreateAccountPermanentFolders();
  PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::BOOKMARK_BAR);
  AddNodesFromModelString(&model(), model().other_node(), "X Y Z ");
  AddNodesFromModelString(&model(), model().account_bookmark_bar_node(),
                          "A1 A2 ");
  EXPECT_EQ(tracker.GetChildrenCount(), 2u);

  const BookmarkNode* node = model().other_node()->children()[0].get();
  // {A1 A2 }
  tracker.MoveToIndex(node, 1u);
  // {A1 X A2 }
  EXPECT_EQ(tracker.GetIndexOf(node), 1u);
  EXPECT_EQ(node->parent(), model().bookmark_bar_node());
  EXPECT_EQ(node->parent()->GetIndexOf(node), 0u);

  node = model().other_node()->children()[1].get();
  // {1 X 2 }
  tracker.MoveToIndex(node, 3u);
  // {1 X 2 Z }
  EXPECT_EQ(tracker.GetIndexOf(node), 3u);
  EXPECT_EQ(node->parent(), model().bookmark_bar_node());
  EXPECT_EQ(node->parent()->GetIndexOf(node), 1u);
}

TEST_F(PermanentFolderOrderingTrackerTest, MoveToLocalOrderingNotTracked) {
  model().LoadEmptyForTest();
  model().CreateAccountPermanentFolders();
  PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::BOOKMARK_BAR);
  AddNodesFromModelString(&model(), model().other_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");

  EXPECT_EQ(tracker.GetChildrenCount(), 0u);
  const BookmarkNode* node = model().other_node()->children()[0].get();
  tracker.MoveToIndex(node, 0);
  // { 1 }
  EXPECT_EQ(tracker.GetChildrenCount(), 1u);
  EXPECT_EQ(node->parent(), model().bookmark_bar_node());
  EXPECT_EQ(tracker.GetNodeAtIndex(0), node);

  node = model().other_node()->children()[0].get();
  tracker.MoveToIndex(node, 0);
  // { 2 1 }
  EXPECT_EQ(tracker.GetChildrenCount(), 2u);
  EXPECT_EQ(node->parent(), model().bookmark_bar_node());
  EXPECT_EQ(tracker.GetNodeAtIndex(0), node);
}

TEST_F(PermanentFolderOrderingTrackerTest, MoveToAccountOrderingNotTracked) {
  model().LoadEmptyForTest();
  model().CreateAccountPermanentFolders();
  PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::BOOKMARK_BAR);
  AddNodesFromModelString(&model(), model().account_other_node(), "A1 A2 ");

  EXPECT_EQ(tracker.GetChildrenCount(), 0u);
  const BookmarkNode* node = model().account_other_node()->children()[0].get();
  tracker.MoveToIndex(node, 0);
  // { A1 }
  EXPECT_EQ(tracker.GetChildrenCount(), 1u);
  EXPECT_EQ(node->parent(), model().account_bookmark_bar_node());
  EXPECT_EQ(tracker.GetNodeAtIndex(0), node);

  node = model().account_other_node()->children()[0].get();
  tracker.MoveToIndex(node, 1);
  // { A1 A2 }
  EXPECT_EQ(tracker.GetChildrenCount(), 2u);
  EXPECT_EQ(node->parent(), model().account_bookmark_bar_node());
  EXPECT_EQ(tracker.GetNodeAtIndex(1), node);
}

}  // namespace
