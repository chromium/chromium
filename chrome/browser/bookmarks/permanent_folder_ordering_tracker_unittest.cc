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

  for (size_t i = 0; i < model().account_other_node()->children().size(); i++) {
    const BookmarkNode* node =
        model().account_other_node()->children()[i].get();
    EXPECT_EQ(tracker.GetIndexOf(node), i);
  }

  size_t account_nodes_size = model().account_other_node()->children().size();
  ASSERT_EQ(account_nodes_size, 4u);
  for (size_t i = 0; i < model().other_node()->children().size(); i++) {
    const BookmarkNode* node = model().other_node()->children()[i].get();
    EXPECT_EQ(tracker.GetIndexOf(node), i + 4);
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
  }

  // Insert local nodes.
  const BookmarkNode* node = model().AddURL(
      model().bookmark_bar_node(), 1, u"Title", GURL("https://example.com"));
  // {L0, L01, A0, A1, A2, L1, L2, L3, A3}.
  EXPECT_EQ(tracker.GetIndexOf(node), 1u);
  node = model().AddURL(model().bookmark_bar_node(), 3, u"Title",
                        GURL("https://example.com"));
  // 2 local nodes, 3 account, 1 local then `node`.
  // {L0, L01, A0, A1, A2, L1, L11, L2, L3, A3}.
  EXPECT_EQ(tracker.GetIndexOf(node), 6u);

  // Insert account nodes.
  node = model().AddURL(model().account_bookmark_bar_node(), 0, u"Title",
                        GURL("https://example.com"));
  // {L0, L01, A00, A0, A1, A2, L1, L11, L2, L3, A3}.
  EXPECT_EQ(tracker.GetIndexOf(node), 2u);

  node = model().AddURL(model().account_bookmark_bar_node(), 4, u"Title",
                        GURL("https://example.com"));
  // {L0, L01, A00, A0, A1, A2, A21, L1, L11, L2, L3, A3}.
  EXPECT_EQ(tracker.GetIndexOf(node), 6u);
}

TEST_F(PermanentFolderOrderingTrackerTest, OrderingLocalOnly) {
  model().LoadEmptyForTest();
  PermanentFolderOrderingTracker tracker(&model(), BookmarkNode::BOOKMARK_BAR);
  ASSERT_THAT(tracker.GetUnderlyingPermanentNodes(),
              ElementsAre(model().bookmark_bar_node()));
  AddNodesFromModelString(&model(), model().bookmark_bar_node(),
                          "1 2 3 f1:[ 4 5 f2:[ 6 ] ]");

  for (size_t i = 0; i < model().bookmark_bar_node()->children().size(); i++) {
    const BookmarkNode* node = model().bookmark_bar_node()->children()[i].get();
    EXPECT_EQ(tracker.GetIndexOf(node), i);
  }

  // Remove node.
  model().Remove(model().bookmark_bar_node()->children()[1].get(),
                 bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);

  // Add node.
  model().AddURL(model().bookmark_bar_node(), 1, u"Title",
                 GURL("https://example.com"));
  for (size_t i = 0; i < model().bookmark_bar_node()->children().size(); i++) {
    const BookmarkNode* node = model().bookmark_bar_node()->children()[i].get();
    EXPECT_EQ(tracker.GetIndexOf(node), i);
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

  for (size_t i = 0; i < model().account_bookmark_bar_node()->children().size();
       i++) {
    const BookmarkNode* node =
        model().account_bookmark_bar_node()->children()[i].get();
    EXPECT_EQ(tracker.GetIndexOf(node), i);
  }

  // Remove node.
  model().Remove(model().account_bookmark_bar_node()->children()[1].get(),
                 bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);

  // Add node.
  model().AddURL(model().account_bookmark_bar_node(), 1, u"Title",
                 GURL("https://example.com"));
  for (size_t i = 0; i < model().account_bookmark_bar_node()->children().size();
       i++) {
    const BookmarkNode* node =
        model().account_bookmark_bar_node()->children()[i].get();
    EXPECT_EQ(tracker.GetIndexOf(node), i);
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

  // Add first account bookmark.
  const BookmarkNode* node =
      model().AddURL(model().account_bookmark_bar_node(), 0, u"Title x",
                     GURL("https://example.com"));
  EXPECT_EQ(tracker.GetIndexOf(node), 0u);

  node = model().AddURL(model().account_bookmark_bar_node(), 1, u"Title y",
                        GURL("https://example.com"));
  EXPECT_EQ(tracker.GetIndexOf(node), 1u);

  EXPECT_EQ(
      tracker.GetIndexOf(model().bookmark_bar_node()->children()[0].get()), 2u);
  // Insert local nodes.
  node = model().AddURL(model().bookmark_bar_node(), 0, u"Title z",
                        GURL("https://example.com"));
  EXPECT_EQ(tracker.GetIndexOf(node), 2u);

  node = model().AddURL(model().bookmark_bar_node(), 4, u"Title w",
                        GURL("https://example.com"));
  EXPECT_EQ(tracker.GetIndexOf(node), 6u);
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

  node = model().AddURL(model().bookmark_bar_node(), 1, u"Title y",
                        GURL("https://example.com"));
  EXPECT_EQ(tracker.GetIndexOf(node), 5u);

  EXPECT_EQ(tracker.GetIndexOf(
                model().account_bookmark_bar_node()->children()[0].get()),
            0u);
  // Insert account nodes.
  node = model().AddURL(model().account_bookmark_bar_node(), 0, u"Title z",
                        GURL("https://example.com"));
  EXPECT_EQ(tracker.GetIndexOf(node), 0u);
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

  model().RemoveAccountPermanentFolders();
  EXPECT_THAT(tracker.GetUnderlyingPermanentNodes(),
              ElementsAre(model().other_node()));
  EXPECT_EQ(tracker.GetIndexOf(node), 0u);
}

}  // namespace
