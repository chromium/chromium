// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/android/bookmark_model_sql_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/test/test_history_database.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::UrlAndTitle;
using content::BrowserThread;

namespace history {

class BookmarkModelSQLHandlerTest : public testing::Test {
 public:
  BookmarkModelSQLHandlerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        bookmark_model_(NULL) {}
  ~BookmarkModelSQLHandlerTest() override {}

 protected:
  void SetUp() override {
    // Setup the testing profile, so the bookmark_model_sql_handler could
    // get the bookmark model from it.
    ASSERT_TRUE(profile_manager_.SetUp());
    // It seems that the name has to be chrome::kInitialProfile, so it
    // could be found by ProfileManager::GetLastUsedProfile().
    TestingProfile* testing_profile = profile_manager_.CreateTestingProfile(
        chrome::kInitialProfile);
    // Create the BookmarkModel that doesn't need to invoke load().
    testing_profile->CreateBookmarkModel(true);
    bookmark_model_ =
        BookmarkModelFactory::GetForBrowserContext(testing_profile);
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);
    ASSERT_TRUE(bookmark_model_);
    // Get the BookmarkModel from LastUsedProfile, this is the same way that
    // how the BookmarkModelSQLHandler gets the BookmarkModel.
    Profile* profile = ProfileManager::GetLastUsedProfile();
    ASSERT_TRUE(profile);

    // Create the directory for history database.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath history_db_name =
        temp_dir_.GetPath().AppendASCII(kHistoryFilename);
    history_db_.Init(history_db_name);
  }

  // Runs the MessageLoopForUI, and return till all pending messages were
  // processed.
  void RunMessageLoopForUI() {
    content::RunAllPendingInMessageLoop();
  }

  content::BrowserTaskEnvironment task_environment_;

  TestingProfileManager profile_manager_;
  BookmarkModel* bookmark_model_;
  base::ScopedTempDir temp_dir_;
  TestHistoryDatabase history_db_;
};

TEST_F(BookmarkModelSQLHandlerTest, InsertIntoMobileFolder) {
  HistoryAndBookmarkRow row;
  row.set_raw_url("http://bookmark.com");
  row.set_url(GURL("http://bookmark.com"));
  row.set_title(base::UTF8ToUTF16("Bookmark Title"));
  row.set_is_bookmark(true);

  BookmarkModelSQLHandler handler(&history_db_);
  ASSERT_TRUE(handler.Insert(&row));
  RunMessageLoopForUI();
  std::vector<const BookmarkNode*> nodes;
  bookmark_model_->GetNodesByURL(row.url(), &nodes);
  ASSERT_EQ(1u, nodes.size());
  EXPECT_EQ(row.title(), nodes[0]->GetTitle());
  const BookmarkNode* parent = nodes[0]->parent();
  ASSERT_TRUE(parent);
  EXPECT_EQ(bookmark_model_->mobile_node()->id(), parent->id());
}

TEST_F(BookmarkModelSQLHandlerTest, InsertIntoSpecificFolder) {
  HistoryAndBookmarkRow row;
  row.set_raw_url("http://bookmark.com");
  row.set_url(GURL("http://bookmark.com"));
  row.set_title(base::UTF8ToUTF16("Bookmark Title"));
  row.set_is_bookmark(true);
  // Set other folder as parent.
  row.set_parent_id(bookmark_model_->other_node()->id());

  BookmarkModelSQLHandler handler(&history_db_);
  ASSERT_TRUE(handler.Insert(&row));
  RunMessageLoopForUI();
  std::vector<const BookmarkNode*> nodes;
  bookmark_model_->GetNodesByURL(row.url(), &nodes);
  ASSERT_EQ(1u, nodes.size());
  EXPECT_EQ(row.title(), nodes[0]->GetTitle());
  const BookmarkNode* parent = nodes[0]->parent();
  ASSERT_TRUE(parent);
  EXPECT_EQ(row.parent_id(), parent->id());
}

TEST_F(BookmarkModelSQLHandlerTest, UpdateHistoryToBookmark) {
  // Added a row in url database.
  URLRow url_row(GURL("http://www.google.com"));
  url_row.set_title(base::UTF8ToUTF16("Google"));
  URLID url_id = history_db_.AddURL(url_row);
  ASSERT_TRUE(url_id);

  // Update the added row as bookmark.
  HistoryAndBookmarkRow row;
  row.set_url(url_row.url());
  row.set_is_bookmark(true);

  TableIDRow id_row;
  id_row.url_id = url_id;
  id_row.url = url_row.url();
  TableIDRows id_rows;
  id_rows.push_back(id_row);

  BookmarkModelSQLHandler handler(&history_db_);
  ASSERT_TRUE(handler.Update(row, id_rows));
  RunMessageLoopForUI();
  // Get all bookmarks and verify there is only one.
  std::vector<UrlAndTitle> bookmarks;
  bookmark_model_->GetBookmarks(&bookmarks);
  ASSERT_EQ(1u, bookmarks.size());
  EXPECT_EQ(url_row.url(), bookmarks[0].url);
  EXPECT_EQ(url_row.title(), bookmarks[0].title);

  // Get the bookmark node.
  std::vector<const BookmarkNode*> nodes;
  bookmark_model_->GetNodesByURL(row.url(), &nodes);
  ASSERT_EQ(1u, nodes.size());
  EXPECT_EQ(url_row.title(), nodes[0]->GetTitle());
  const BookmarkNode* parent = nodes[0]->parent();
  ASSERT_TRUE(parent);
  EXPECT_EQ(bookmark_model_->mobile_node()->id(), parent->id());

  // Remove the bookmark
  row.set_is_bookmark(false);
  ASSERT_TRUE(handler.Update(row, id_rows));
  RunMessageLoopForUI();
  bookmarks.clear();
  bookmark_model_->GetBookmarks(&bookmarks);
  EXPECT_TRUE(bookmarks.empty());

  // Update with the parent id.
  row.set_parent_id(bookmark_model_->other_node()->id());
  row.set_is_bookmark(true);
  ASSERT_TRUE(handler.Update(row, id_rows));
  RunMessageLoopForUI();
  // Get all bookmarks and verify there is only one.
  bookmarks.clear();
  bookmark_model_->GetBookmarks(&bookmarks);
  ASSERT_EQ(1u, bookmarks.size());
  EXPECT_EQ(url_row.url(), bookmarks[0].url);
  EXPECT_EQ(url_row.title(), bookmarks[0].title);
  // Get the bookmark node.
  nodes.clear();
  bookmark_model_->GetNodesByURL(row.url(), &nodes);
  ASSERT_EQ(1u, nodes.size());
  EXPECT_EQ(url_row.title(), nodes[0]->GetTitle());
  const BookmarkNode* parent1 = nodes[0]->parent();
  ASSERT_TRUE(parent1);
  EXPECT_EQ(row.parent_id(), parent1->id());

  // Only update the title.
  url_row.set_title(base::UTF8ToUTF16("Google Inc."));
  history_db_.UpdateURLRow(url_id, url_row);
  HistoryAndBookmarkRow update_title;
  update_title.set_title(url_row.title());
  ASSERT_TRUE(handler.Update(update_title, id_rows));
  RunMessageLoopForUI();
  // Get all bookmarks and verify there is only one.
  bookmarks.clear();
  bookmark_model_->GetBookmarks(&bookmarks);
  ASSERT_EQ(1u, bookmarks.size());
  EXPECT_EQ(url_row.url(), bookmarks[0].url);
  EXPECT_EQ(url_row.title(), bookmarks[0].title);
  // Get the bookmark node.
  nodes.clear();
  bookmark_model_->GetNodesByURL(row.url(), &nodes);
  ASSERT_EQ(1u, nodes.size());
  EXPECT_EQ(url_row.title(), nodes[0]->GetTitle());
  const BookmarkNode* parent2 = nodes[0]->parent();
  ASSERT_TRUE(parent2);
  // The parent id shouldn't changed.
  EXPECT_EQ(row.parent_id(), parent2->id());
}

TEST_F(BookmarkModelSQLHandlerTest, Delete) {
  // Insert 3 bookmarks, 2 of them have the same URL, but one is in mobile
  // folder, another is in other folder, The 3rd one has different URL.
  HistoryAndBookmarkRow row;
  GURL url1 = GURL("http://bookmark.com");
  row.set_raw_url("http://bookmark.com");
  row.set_url(url1);
  row.set_title(base::UTF8ToUTF16("Bookmark Title"));
  row.set_is_bookmark(true);

  BookmarkModelSQLHandler handler(&history_db_);
  ASSERT_TRUE(handler.Insert(&row));

  // Set other folder as parent.
  row.set_parent_id(bookmark_model_->other_node()->id());
  ASSERT_TRUE(handler.Insert(&row));

  row.set_url(GURL("http://google.com"));
  ASSERT_TRUE(handler.Insert(&row));
  RunMessageLoopForUI();
  // Get all bookmarks and verify there are 3 bookmarks.
  EXPECT_EQ(1u, bookmark_model_->mobile_node()->children().size());
  EXPECT_EQ(2u, bookmark_model_->other_node()->children().size());

  // Remove the third one.
  TableIDRow id_row;
  id_row.url = row.url();
  TableIDRows id_rows;
  id_rows.push_back(id_row);

  ASSERT_TRUE(handler.Delete(id_rows));
  RunMessageLoopForUI();
  // Verify the first 2 bookmarks still exist.
  EXPECT_EQ(1u, bookmark_model_->mobile_node()->children().size());
  EXPECT_EQ(1u, bookmark_model_->other_node()->children().size());

  id_row.url = url1;
  id_rows.clear();
  id_rows.push_back(id_row);
  ASSERT_TRUE(handler.Delete(id_rows));
  RunMessageLoopForUI();
  // All bookmarks were deleted.
  EXPECT_FALSE(bookmark_model_->HasBookmarks());
}

}  // namespace history
