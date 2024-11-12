// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/permanent_folder_ordering_tracker.h"

#include "base/test/scoped_feature_list.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/sync/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
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

}  // namespace
