// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_expanded_state_tracker.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class BookmarkExpandedStateTrackerTest : public testing::Test {
 public:
  BookmarkExpandedStateTrackerTest();

  BookmarkExpandedStateTrackerTest(const BookmarkExpandedStateTrackerTest&) =
      delete;
  BookmarkExpandedStateTrackerTest& operator=(
      const BookmarkExpandedStateTrackerTest&) = delete;

  ~BookmarkExpandedStateTrackerTest() override;

 protected:
  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  base::ScopedTempDir scoped_temp_dir_;
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<bookmarks::BookmarkModel> model_;
  std::unique_ptr<BookmarkExpandedStateTracker> tracker_;
};

BookmarkExpandedStateTrackerTest::BookmarkExpandedStateTrackerTest() = default;

BookmarkExpandedStateTrackerTest::~BookmarkExpandedStateTrackerTest() = default;

void BookmarkExpandedStateTrackerTest::SetUp() {
  ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  prefs_.registry()->RegisterListPref(
      bookmarks::prefs::kBookmarkEditorExpandedNodes);
  prefs_.registry()->RegisterListPref(bookmarks::prefs::kManagedBookmarks);
  model_ = std::make_unique<bookmarks::BookmarkModel>(
      std::make_unique<bookmarks::TestBookmarkClient>());
  tracker_ = std::make_unique<BookmarkExpandedStateTracker>(&prefs_);
  tracker_->Init(model_.get());
  model_->Load(scoped_temp_dir_.GetPath());
  bookmarks::test::WaitForBookmarkModelToLoad(model_.get());
}

void BookmarkExpandedStateTrackerTest::TearDown() {
  model_.reset();
  base::RunLoop().RunUntilIdle();
}

// Various assertions for SetExpandedNodes.
TEST_F(BookmarkExpandedStateTrackerTest, SetExpandedNodes) {
  // Should start out initially empty.
  EXPECT_TRUE(tracker_->GetExpandedNodes().empty());

  BookmarkExpandedStateTracker::Nodes nodes;
  nodes.insert(model_->bookmark_bar_node());
  tracker_->SetExpandedNodes(nodes);
  EXPECT_EQ(nodes, tracker_->GetExpandedNodes());

  // Add a folder and mark it expanded.
  const bookmarks::BookmarkNode* n1 =
      model_->AddFolder(model_->bookmark_bar_node(), 0, u"x");
  nodes.insert(n1);
  tracker_->SetExpandedNodes(nodes);
  EXPECT_EQ(nodes, tracker_->GetExpandedNodes());

  // Remove the folder, which should remove it from the list of expanded nodes.
  model_->Remove(model_->bookmark_bar_node()->children().front().get(),
                 bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  nodes.erase(n1);
  n1 = nullptr;
  EXPECT_EQ(nodes, tracker_->GetExpandedNodes());
}

TEST_F(BookmarkExpandedStateTrackerTest, RemoveAllUserBookmarks) {
  // Add a folder and mark it expanded.
  const bookmarks::BookmarkNode* n1 =
      model_->AddFolder(model_->bookmark_bar_node(), 0, u"x");
  BookmarkExpandedStateTracker::Nodes nodes;
  nodes.insert(n1);
  tracker_->SetExpandedNodes(nodes);
  // Verify that the node is present.
  EXPECT_EQ(nodes, tracker_->GetExpandedNodes());
  // Call remove all.
  model_->RemoveAllUserBookmarks(FROM_HERE);
  // Verify node is not present.
  EXPECT_TRUE(tracker_->GetExpandedNodes().empty());
}
