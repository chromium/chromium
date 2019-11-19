// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/android/android_history_provider_service.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/history/core/browser/android/android_history_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using base::Bind;
using base::Time;
using history::AndroidStatement;
using history::HistoryAndBookmarkRow;
using history::SearchRow;

// The test cases in this file don't intent to test the detail features of
// Android content provider which have been covered by
// android_provider_backend_unittest.cc, instead, they verify the code path to
// AndroidProviderBackend working fine.

class AndroidHistoryProviderServiceTest : public testing::Test {
 public:
  AndroidHistoryProviderServiceTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~AndroidHistoryProviderServiceTest() override {}

 protected:
  void SetUp() override {
    // Setup the testing profile, so the bookmark_model_sql_handler could
    // get the bookmark model from it.
    ASSERT_TRUE(profile_manager_.SetUp());
    // It seems that the name has to be chrome::kInitialProfile, so it
    // could be found by ProfileManager::GetLastUsedProfile().
    testing_profile_ = profile_manager_.CreateTestingProfile(
        chrome::kInitialProfile);

    testing_profile_->CreateBookmarkModel(true);
    bookmarks::test::WaitForBookmarkModelToLoad(
        BookmarkModelFactory::GetForBrowserContext(testing_profile_));
    ASSERT_TRUE(testing_profile_->CreateHistoryService(true, false));
    service_.reset(new AndroidHistoryProviderService(testing_profile_));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<AndroidHistoryProviderService> service_;
  base::CancelableTaskTracker cancelable_tracker_;
  TestingProfile* testing_profile_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AndroidHistoryProviderServiceTest);
};

class CallbackHelper : public base::RefCountedThreadSafe<CallbackHelper> {
 public:
  CallbackHelper()
      : success_(false), statement_(nullptr), cursor_position_(0), count_(0) {}

  bool success() const {
    return success_;
  }

  AndroidStatement* statement() const {
    return statement_;
  }

  int cursor_position() const {
    return cursor_position_;
  }

  int count() const {
    return count_;
  }

  void set_quit_when_idle_closure(const base::Closure& quit_when_idle_closure) {
    quit_when_idle_closure_ = quit_when_idle_closure;
  }

  void OnInserted(int64_t id) {
    success_ = id != 0;
    quit_when_idle_closure_.Run();
  }

  void OnQueryResult(AndroidStatement* statement) {
    success_ = statement != nullptr;
    statement_ = statement;
    quit_when_idle_closure_.Run();
  }

  void OnUpdated(int count) {
    success_ = count != 0;
    count_ = count;
    quit_when_idle_closure_.Run();
  }

  void OnDeleted(int count) {
    success_ = count != 0;
    count_ = count;
    quit_when_idle_closure_.Run();
  }

  void OnStatementMoved(int cursor_position) {
    cursor_position_ = cursor_position;
    quit_when_idle_closure_.Run();
  }

 private:
  friend class base::RefCountedThreadSafe<CallbackHelper>;
  ~CallbackHelper() {
  }

  bool success_;
  AndroidStatement* statement_;
  int cursor_position_;
  int count_;
  base::Closure quit_when_idle_closure_;

  DISALLOW_COPY_AND_ASSIGN(CallbackHelper);
};

void RunMessageLoop(CallbackHelper* callback_helper) {
  ASSERT_TRUE(callback_helper);
  base::RunLoop run_loop;
  callback_helper->set_quit_when_idle_closure(run_loop.QuitWhenIdleClosure());
  run_loop.Run();
  callback_helper->set_quit_when_idle_closure(base::Closure());
}

TEST_F(AndroidHistoryProviderServiceTest, TestHistoryAndBookmark) {
  HistoryAndBookmarkRow row;
  row.set_raw_url("http://www.google.com");
  row.set_url(GURL("http://www.google.com"));

  scoped_refptr<CallbackHelper> callback(new CallbackHelper());

  // Insert a row and verify it succeeded.
  service_->InsertHistoryAndBookmark(
      row,
      Bind(&CallbackHelper::OnInserted, callback.get()),
      &cancelable_tracker_);

  RunMessageLoop(callback.get());
  EXPECT_TRUE(callback->success());

  std::vector<HistoryAndBookmarkRow::ColumnID> projections;
  projections.push_back(HistoryAndBookmarkRow::ID);

  // Query the inserted row.
  service_->QueryHistoryAndBookmarks(
      projections,
      std::string(),
      std::vector<base::string16>(),
      std::string(),
      Bind(&CallbackHelper::OnQueryResult, callback.get()),
      &cancelable_tracker_);
  RunMessageLoop(callback.get());
  ASSERT_TRUE(callback->success());

  // Move the cursor to the begining and verify whether we could get
  // the same result.
  AndroidStatement* statement = callback->statement();
  service_->MoveStatement(
      statement,
      0,
      -1,
      Bind(&CallbackHelper::OnStatementMoved, callback.get()),
      &cancelable_tracker_);
  RunMessageLoop(callback.get());
  EXPECT_EQ(-1, callback->cursor_position());
  EXPECT_TRUE(callback->statement()->statement()->Step());
  EXPECT_FALSE(callback->statement()->statement()->Step());
  service_->CloseStatement(statement);

  // Update the row.
  HistoryAndBookmarkRow update_row;
  update_row.set_visit_count(3);
  service_->UpdateHistoryAndBookmarks(
      update_row,
      std::string(),
      std::vector<base::string16>(),
      Bind(&CallbackHelper::OnUpdated, callback.get()),
      &cancelable_tracker_);
  RunMessageLoop(callback.get());
  EXPECT_TRUE(callback->success());
  EXPECT_EQ(1, callback->count());

  // Delete the row.
  service_->DeleteHistoryAndBookmarks(
      std::string(),
      std::vector<base::string16>(),
      Bind(&CallbackHelper::OnDeleted, callback.get()),
      &cancelable_tracker_);
  RunMessageLoop(callback.get());
  EXPECT_TRUE(callback->success());
  EXPECT_EQ(1, callback->count());
}

TEST_F(AndroidHistoryProviderServiceTest, TestSearchTerm) {
  SearchRow search_row;
  search_row.set_search_term(base::UTF8ToUTF16("google"));
  search_row.set_url(GURL("http://google.com"));
  search_row.set_keyword_id(1);
  search_row.set_search_time(Time::Now());

  scoped_refptr<CallbackHelper> callback(new CallbackHelper());

  // Insert a row and verify it succeeded.
  service_->InsertSearchTerm(search_row,
                             Bind(&CallbackHelper::OnInserted, callback.get()),
                             &cancelable_tracker_);

  RunMessageLoop(callback.get());
  EXPECT_TRUE(callback->success());

  std::vector<SearchRow::ColumnID> projections;
  projections.push_back(SearchRow::ID);

  // Query the inserted row.
  service_->QuerySearchTerms(
      projections,
      std::string(),
      std::vector<base::string16>(),
      std::string(),
      Bind(&CallbackHelper::OnQueryResult, callback.get()),
      &cancelable_tracker_);
  RunMessageLoop(callback.get());
  ASSERT_TRUE(callback->success());

  // Move the cursor to the begining and verify whether we could get
  // the same result.
  AndroidStatement* statement = callback->statement();
  service_->MoveStatement(
      statement,
      0,
      -1,
      Bind(&CallbackHelper::OnStatementMoved, callback.get()),
      &cancelable_tracker_);
  RunMessageLoop(callback.get());
  EXPECT_EQ(-1, callback->cursor_position());
  EXPECT_TRUE(callback->statement()->statement()->Step());
  EXPECT_FALSE(callback->statement()->statement()->Step());
  service_->CloseStatement(statement);

  // Update the row.
  SearchRow update_row;
  update_row.set_search_time(Time::Now());
  service_->UpdateSearchTerms(update_row,
                              std::string(),
                              std::vector<base::string16>(),
                              Bind(&CallbackHelper::OnUpdated, callback.get()),
                              &cancelable_tracker_);
  RunMessageLoop(callback.get());
  EXPECT_TRUE(callback->success());
  EXPECT_EQ(1, callback->count());

  // Delete the row.
  service_->DeleteSearchTerms(std::string(),
                              std::vector<base::string16>(),
                              Bind(&CallbackHelper::OnDeleted, callback.get()),
                              &cancelable_tracker_);
  RunMessageLoop(callback.get());
  EXPECT_TRUE(callback->success());
  EXPECT_EQ(1, callback->count());
}

}  // namespace
