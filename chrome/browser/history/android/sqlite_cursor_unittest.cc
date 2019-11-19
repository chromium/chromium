// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/android/sqlite_cursor.h"

#include <jni.h>
#include <stdint.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/android/android_history_provider_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/history/core/browser/android/android_history_types.h"
#include "components/history/core/browser/android/android_time.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using base::Bind;
using base::Time;
using history::AndroidStatement;
using history::HistoryAndBookmarkRow;
using history::SearchRow;

// The test cases in this file don't test the JNI interface which will be
// covered in Java tests.
class SQLiteCursorTest : public testing::Test,
                         public SQLiteCursor::TestObserver {
 public:
  SQLiteCursorTest() : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~SQLiteCursorTest() override {}

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

    testing_profile_->CreateFaviconService();
    ASSERT_TRUE(testing_profile_->CreateHistoryService(true, false));
    service_.reset(new AndroidHistoryProviderService(testing_profile_));
    hs_ = HistoryServiceFactory::GetForProfile(
        testing_profile_, ServiceAccessType::EXPLICIT_ACCESS);
  }

  // Override SQLiteCursor::TestObserver.
  void OnPostMoveToTask() override {
    ASSERT_FALSE(run_loop_);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_ = nullptr;
  }

  void OnGetMoveToResult() override {
    ASSERT_TRUE(run_loop_);
    run_loop_->QuitWhenIdle();
  }

  void OnPostGetFaviconTask() override {
    ASSERT_FALSE(run_loop_);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_ = nullptr;
  }

  void OnGetFaviconResult() override {
    ASSERT_TRUE(run_loop_);
    run_loop_->QuitWhenIdle();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<AndroidHistoryProviderService> service_;
  base::CancelableTaskTracker cancelable_tracker_;
  TestingProfile* testing_profile_;
  history::HistoryService* hs_;
  std::unique_ptr<base::RunLoop> run_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SQLiteCursorTest);
};

class CallbackHelper : public base::RefCountedThreadSafe<CallbackHelper> {
 public:
  CallbackHelper()
      : success_(false),
        statement_(NULL) {
  }

  bool success() const {
    return success_;
  }

  AndroidStatement* statement() const {
    return statement_;
  }

  void OnInserted(int64_t id) {
    success_ = id != 0;
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

  void OnQueryResult(AndroidStatement* statement) {
    success_ = statement != NULL;
    statement_ = statement;
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

 private:
  friend class base::RefCountedThreadSafe<CallbackHelper>;
  ~CallbackHelper() {
  }

  bool success_;
  AndroidStatement* statement_;

  DISALLOW_COPY_AND_ASSIGN(CallbackHelper);
};

}  // namespace

TEST_F(SQLiteCursorTest, Run) {
  HistoryAndBookmarkRow row;
  row.set_raw_url("http://www.google.com/");
  row.set_url(GURL("http://www.google.com/"));
  std::vector<unsigned char> favicon_data;
  favicon_data.push_back(1);
  scoped_refptr<base::RefCountedBytes> data_bytes =
      base::RefCountedBytes::TakeVector(&favicon_data);
  row.set_favicon(data_bytes);
  row.set_last_visit_time(Time::Now());
  row.set_visit_count(2);
  row.set_title(base::UTF8ToUTF16("cnn"));
  scoped_refptr<CallbackHelper> callback(new CallbackHelper());

  // Insert a row and verify it succeeded.
  service_->InsertHistoryAndBookmark(
      row,
      Bind(&CallbackHelper::OnInserted, callback.get()),
      &cancelable_tracker_);

  base::RunLoop().Run();
  EXPECT_TRUE(callback->success());

  std::vector<HistoryAndBookmarkRow::ColumnID> projections;
  projections.push_back(HistoryAndBookmarkRow::URL);
  projections.push_back(HistoryAndBookmarkRow::LAST_VISIT_TIME);
  projections.push_back(HistoryAndBookmarkRow::VISIT_COUNT);
  projections.push_back(HistoryAndBookmarkRow::FAVICON);

  // Query the inserted row.
  service_->QueryHistoryAndBookmarks(
      projections,
      std::string(),
      std::vector<base::string16>(),
      std::string(),
      Bind(&CallbackHelper::OnQueryResult, callback.get()),
      &cancelable_tracker_);
  base::RunLoop().Run();
  ASSERT_TRUE(callback->success());

  AndroidStatement* statement = callback->statement();
  std::vector<std::string> column_names;
  column_names.push_back(
      HistoryAndBookmarkRow::GetAndroidName(HistoryAndBookmarkRow::URL));
  column_names.push_back(HistoryAndBookmarkRow::GetAndroidName(
      HistoryAndBookmarkRow::LAST_VISIT_TIME));
  column_names.push_back(HistoryAndBookmarkRow::GetAndroidName(
      HistoryAndBookmarkRow::VISIT_COUNT));
  column_names.push_back(HistoryAndBookmarkRow::GetAndroidName(
      HistoryAndBookmarkRow::FAVICON));

  SQLiteCursor* cursor = new SQLiteCursor(column_names, statement,
      service_.get());
  cursor->set_test_observer(this);
  JNIEnv* env = base::android::AttachCurrentThread();
  EXPECT_EQ(1, cursor->GetCount(env, NULL));
  EXPECT_EQ(0, cursor->MoveTo(env, NULL, 0));
  EXPECT_EQ(row.url().spec(), base::android::ConvertJavaStringToUTF8(
      cursor->GetString(env, NULL, 0)).c_str());
  EXPECT_EQ(history::ToDatabaseTime(row.last_visit_time()),
      cursor->GetLong(env, NULL, 1));
  EXPECT_EQ(row.visit_count(), cursor->GetInt(env, NULL, 2));
  base::android::ScopedJavaLocalRef<jbyteArray> data =
      cursor->GetBlob(env, NULL, 3);
  std::vector<uint8_t> out;
  base::android::JavaByteArrayToByteVector(env, data, &out);
  EXPECT_EQ(data_bytes->data().size(), out.size());
  EXPECT_EQ(data_bytes->data()[0], out[0]);
  cursor->Destroy(env, NULL);
  // Cursor::Destroy posts the task in UI thread, run Message loop to release
  // the statement, delete SQLiteCursor itself etc.
  content::RunAllPendingInMessageLoop();
}
