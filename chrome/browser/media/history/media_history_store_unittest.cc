// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_store.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/pooled_sequenced_task_runner.h"
#include "base/test/test_timeouts.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/media_player_watch_time.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_history {

class MediaHistoryStoreUnitTest : public testing::Test {
 public:
  MediaHistoryStoreUnitTest() = default;
  void SetUp() override {
    // Set up the profile.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(temp_dir_.GetPath());

    // Set up the media history store.
    scoped_refptr<base::UpdateableSequencedTaskRunner> task_runner =
        base::CreateUpdateableSequencedTaskRunner(
            {base::ThreadPool(), base::MayBlock(),
             base::WithBaseSyncPrimitives()});
    media_history_store_ = std::make_unique<MediaHistoryStore>(
        profile_builder.Build().get(), task_runner);

    // Sleep the thread to allow the media history store to asynchronously
    // create the database and tables before proceeding with the tests and
    // tearing down the temporary directory.
    content::RunAllTasksUntilIdle();

    // Set up the local DB connection used for assertions.
    base::FilePath db_file =
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Media History"));
    EXPECT_TRUE(db_.Open(db_file));
  }

  void TearDown() override { content::RunAllTasksUntilIdle(); }

  MediaHistoryStore* GetMediaHistoryStore() {
    return media_history_store_.get();
  }

 private:
  base::ScopedTempDir temp_dir_;

 protected:
  sql::Database& GetDB() { return db_; }
  content::BrowserTaskEnvironment task_environment_;

 private:
  sql::Database db_;
  std::unique_ptr<MediaHistoryStore> media_history_store_;
};

TEST_F(MediaHistoryStoreUnitTest, CreateDatabaseTables) {
  ASSERT_TRUE(GetDB().DoesTableExist("mediaEngagement"));
  ASSERT_TRUE(GetDB().DoesTableExist("origin"));
  ASSERT_TRUE(GetDB().DoesTableExist("playback"));
}

TEST_F(MediaHistoryStoreUnitTest, SavePlayback) {
  // Create a media player watch time and save it to the playbacks table.
  GURL url("http://google.com/test");
  content::MediaPlayerWatchTime watch_time(
      url, url.GetOrigin(), base::TimeDelta::FromMilliseconds(123),
      base::TimeDelta::FromMilliseconds(321), true, false);
  GetMediaHistoryStore()->SavePlayback(watch_time);
  int64_t now_in_seconds_before =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds();

  // Save the watch time a second time.
  GetMediaHistoryStore()->SavePlayback(watch_time);

  // Wait until the playbacks have finished saving.
  content::RunAllTasksUntilIdle();

  int64_t now_in_seconds_after =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds();

  // Verify that the playback table contains the expected number of items
  sql::Statement select_from_playback_statement(GetDB().GetUniqueStatement(
      "SELECT id, url, origin_id, watch_time_ms, timestamp_ms, has_video, "
      "has_audio, last_updated_time_s FROM playback"));
  ASSERT_TRUE(select_from_playback_statement.is_valid());
  int playback_row_count = 0;
  while (select_from_playback_statement.Step()) {
    ++playback_row_count;
    EXPECT_EQ(playback_row_count, select_from_playback_statement.ColumnInt(0));
    EXPECT_EQ("http://google.com/test",
              select_from_playback_statement.ColumnString(1));
    EXPECT_EQ(1, select_from_playback_statement.ColumnInt(2));
    EXPECT_EQ(123, select_from_playback_statement.ColumnInt(3));
    EXPECT_EQ(321, select_from_playback_statement.ColumnInt(4));
    EXPECT_EQ(1, select_from_playback_statement.ColumnInt(5));
    EXPECT_EQ(0, select_from_playback_statement.ColumnInt(6));
    EXPECT_LE(now_in_seconds_before,
              select_from_playback_statement.ColumnInt64(7));
    EXPECT_GE(now_in_seconds_after,
              select_from_playback_statement.ColumnInt64(7));
  }

  EXPECT_EQ(2, playback_row_count);

  // Verify that the origin table contains the expected number of items
  sql::Statement select_from_origin_statement(
      GetDB().GetUniqueStatement("SELECT id, origin FROM origin"));
  ASSERT_TRUE(select_from_origin_statement.is_valid());
  int origin_row_count = 0;
  while (select_from_origin_statement.Step()) {
    ++origin_row_count;
    EXPECT_EQ(1, select_from_origin_statement.ColumnInt(0));
    EXPECT_EQ("http://google.com/",
              select_from_origin_statement.ColumnString(1));
  }

  EXPECT_EQ(1, origin_row_count);
}

}  // namespace media_history
