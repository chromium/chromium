// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "components/history/core/browser/android/urls_sql_handler.h"
#include "components/history/core/browser/android/visit_sql_handler.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/test/test_history_database.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

namespace history {

class UrlsSQLHandlerTest : public testing::Test {
 public:
  UrlsSQLHandlerTest()
      : urls_sql_handler_(&history_db_),
        visit_sql_handler_(&history_db_, &history_db_) {}
  ~UrlsSQLHandlerTest() override {}

 protected:
  void SetUp() override {
    // Get a temporary directory for the test DB files.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath history_db_name =
        temp_dir_.GetPath().AppendASCII(kHistoryFilename);
    ASSERT_EQ(sql::INIT_OK, history_db_.Init(history_db_name));
  }

  void TearDown() override {}

  TestHistoryDatabase history_db_;
  base::ScopedTempDir temp_dir_;
  UrlsSQLHandler urls_sql_handler_;
  VisitSQLHandler visit_sql_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UrlsSQLHandlerTest);
};

// Insert a row only has URL to verify the visit count and last visit time
// are also set by UrlsSQLHandler.
TEST_F(UrlsSQLHandlerTest, InsertURL) {
  HistoryAndBookmarkRow row;
  row.set_raw_url("http://google.com");
  row.set_url(GURL("http://google.com"));

  ASSERT_TRUE(urls_sql_handler_.Insert(&row));
  URLRow url_row;
  ASSERT_TRUE(history_db_.GetURLRow(row.url_id(), &url_row));
  EXPECT_EQ(row.url(), url_row.url());
  // Both visit count and last visit time are default value.
  EXPECT_EQ(0, url_row.visit_count());
  EXPECT_EQ(Time(), url_row.last_visit());
  // The new row's id was set in url_row correctly.
  EXPECT_EQ(row.url_id(), url_row.id());
}

// Insert a row with last visit time to verify the visit count is set to 1 by
// the UrlsSQLHandler.
TEST_F(UrlsSQLHandlerTest, InsertURLWithLastVisitTime) {
  HistoryAndBookmarkRow row;
  row.set_raw_url("http://google.com");
  row.set_url(GURL("http://google.com"));
  row.set_last_visit_time(Time::Now());

  ASSERT_TRUE(urls_sql_handler_.Insert(&row));
  URLRow url_row;
  ASSERT_TRUE(history_db_.GetURLRow(row.url_id(), &url_row));
  EXPECT_EQ(row.url_id(), url_row.id());
  EXPECT_EQ(row.url(), url_row.url());
  // Visit count should be set to 1 automatically.
  EXPECT_EQ(1, url_row.visit_count());
  EXPECT_EQ(row.last_visit_time(), url_row.last_visit());
}

// Insert a row with different last visit time and created time to verify the
// visit count is set to 2 by the UrlsSQLHandler.
TEST_F(UrlsSQLHandlerTest, InsertURLWithBothTime) {
  HistoryAndBookmarkRow row;
  row.set_raw_url("http://google.com");
  row.set_url(GURL("http://google.com"));
  row.set_last_visit_time(Time::Now());
  row.set_created(Time::Now() - TimeDelta::FromDays(1));

  ASSERT_TRUE(urls_sql_handler_.Insert(&row));
  URLRow url_row;
  ASSERT_TRUE(history_db_.GetURLRow(row.url_id(), &url_row));
  EXPECT_EQ(row.url_id(), url_row.id());
  EXPECT_EQ(row.url(), url_row.url());
  // Visit count should be set to 2 automatically.
  EXPECT_EQ(2, url_row.visit_count());
  EXPECT_EQ(row.last_visit_time(), url_row.last_visit());
}

// Insert a row with created time to verify the visit count is also set to 1
// and last visit time is set to created time by the UrlsSQLHanlder.
TEST_F(UrlsSQLHandlerTest, InsertURLWithCreatedTime) {
  HistoryAndBookmarkRow row;
  row.set_raw_url("http://google.com");
  row.set_url(GURL("http://google.com"));
  row.set_title(base::UTF8ToUTF16("Google"));
  row.set_created(Time::Now());

  ASSERT_TRUE(urls_sql_handler_.Insert(&row));
  URLRow url_row;
  ASSERT_TRUE(history_db_.GetURLRow(row.url_id(), &url_row));
  EXPECT_EQ(row.url_id(), url_row.id());
  EXPECT_EQ(row.url(), url_row.url());
  // Visit count should be set to 1 automatically.
  EXPECT_EQ(1, url_row.visit_count());
  // Last visit time should be set as created time.
  EXPECT_EQ(row.created(), url_row.last_visit());
  EXPECT_EQ(row.title(), url_row.title());
}

// Insert a row with the visit count as 1 to verify the last visit
// time is set by the UrlsSQLHandler.
TEST_F(UrlsSQLHandlerTest, InsertURLWithVisitCount) {
  HistoryAndBookmarkRow row;
  row.set_raw_url("http://google.com");
  row.set_url(GURL("http://google.com"));
  row.set_visit_count(1);

  ASSERT_TRUE(urls_sql_handler_.Insert(&row));
  URLRow url_row;
  ASSERT_TRUE(history_db_.GetURLRow(row.url_id(), &url_row));
  EXPECT_EQ(row.url_id(), url_row.id());
  EXPECT_EQ(row.url(), url_row.url());
  EXPECT_EQ(1, url_row.visit_count());
  // Last visit time should be set to the time when it inserted.
  EXPECT_NE(Time(), url_row.last_visit());
}

// Insert a row with all columns set.
TEST_F(UrlsSQLHandlerTest, Insert) {
  HistoryAndBookmarkRow row;
  row.set_raw_url("http://google.com");
  row.set_url(GURL("http://google.com"));
  row.set_visit_count(10);
  row.set_last_visit_time(Time::Now());
  row.set_title(base::UTF8ToUTF16("Google"));

  ASSERT_TRUE(urls_sql_handler_.Insert(&row));
  URLRow url_row;
  ASSERT_TRUE(history_db_.GetURLRow(row.url_id(), &url_row));
  EXPECT_EQ(row.url_id(), url_row.id());
  EXPECT_EQ(row.url(), url_row.url());
  EXPECT_EQ(10, url_row.visit_count());
  EXPECT_EQ(row.last_visit_time(), url_row.last_visit());
  EXPECT_EQ(row.title(), url_row.title());
}

// Update all columns except URL which can not be updated.
TEST_F(UrlsSQLHandlerTest, Update) {
  HistoryAndBookmarkRow row;
  row.set_raw_url("http://google.com");
  row.set_url(GURL("http://google.com"));
  row.set_title(base::UTF8ToUTF16("Google"));
  row.set_visit_count(10);
  row.set_last_visit_time(Time::Now() - TimeDelta::FromDays(10));

  ASSERT_TRUE(urls_sql_handler_.Insert(&row));
  ASSERT_TRUE(visit_sql_handler_.Insert(&row));
  URLRow url_row;
  ASSERT_TRUE(history_db_.GetURLRow(row.url_id(), &url_row));
  EXPECT_EQ(row.url(), url_row.url());
  EXPECT_EQ(10, url_row.visit_count());
  EXPECT_EQ(row.last_visit_time(), url_row.last_visit());

  HistoryAndBookmarkRow update_row;
  update_row.set_last_visit_time(Time::Now());
  update_row.set_visit_count(1);
  update_row.set_title(base::UTF8ToUTF16("Google LLC"));
  TableIDRow id;
  id.url_id = url_row.id();
  TableIDRows ids;
  ids.push_back(id);
  ASSERT_TRUE(urls_sql_handler_.Update(update_row, ids));
  ASSERT_TRUE(history_db_.GetURLRow(row.url_id(), &url_row));
  EXPECT_EQ(1, url_row.visit_count());
  EXPECT_EQ(update_row.last_visit_time(), url_row.last_visit());
  EXPECT_EQ(update_row.title(), url_row.title());
}

// Update the both time to verify the created time is not impact to visit_count
// as the history will be rebuild.
TEST_F(UrlsSQLHandlerTest, UpdateLastBothTime) {
  HistoryAndBookmarkRow row;
  row.set_raw_url("http://google.com");
  row.set_url(GURL("http://google.com"));
  row.set_title(base::UTF8ToUTF16("Google"));
  row.set_visit_count(10);
  row.set_last_visit_time(Time::Now() - TimeDelta::FromDays(10));

  ASSERT_TRUE(urls_sql_handler_.Insert(&row));
  ASSERT_TRUE(visit_sql_handler_.Insert(&row));
  URLRow url_row;
  ASSERT_TRUE(history_db_.GetURLRow(row.url_id(), &url_row));
  EXPECT_EQ(row.url(), url_row.url());
  EXPECT_EQ(10, url_row.visit_count());
  EXPECT_EQ(row.last_visit_time(), url_row.last_visit());

  HistoryAndBookmarkRow update_row1;
  update_row1.set_created(url_row.last_visit());
  update_row1.set_last_visit_time(Time::Now());

  TableIDRow id;
  id.url_id = url_row.id();
  TableIDRows ids;
  ids.push_back(id);
  ASSERT_TRUE(urls_sql_handler_.Update(update_row1, ids));
  ASSERT_TRUE(history_db_.GetURLRow(row.url_id(), &url_row));
  EXPECT_EQ(11, url_row.visit_count());
  EXPECT_EQ(update_row1.last_visit_time(), url_row.last_visit());

  HistoryAndBookmarkRow update_row;
  update_row.set_created(Time::Now());
  ASSERT_TRUE(urls_sql_handler_.Update(update_row, ids));
  ASSERT_TRUE(history_db_.GetURLRow(row.url_id(), &url_row));
  // Visit count will not changed.
  EXPECT_EQ(11, url_row.visit_count());
  EXPECT_EQ(update_row1.last_visit_time(), url_row.last_visit());
}

// Update the visit count be zero to verify last visit time also set to zero.
TEST_F(UrlsSQLHandlerTest, UpdateVisitCountZero) {
  HistoryAndBookmarkRow row;
  row.set_raw_url("http://google.com");
  row.set_url(GURL("http://google.com"));
  row.set_visit_count(100);
  row.set_last_visit_time(Time::Now() - TimeDelta::FromDays(10));

  ASSERT_TRUE(urls_sql_handler_.Insert(&row));
  ASSERT_TRUE(visit_sql_handler_.Insert(&row));
  URLRow url_row;
  ASSERT_TRUE(history_db_.GetURLRow(row.url_id(), &url_row));
  EXPECT_EQ(row.url(), url_row.url());
  EXPECT_EQ(100, url_row.visit_count());
  EXPECT_EQ(row.last_visit_time(), url_row.last_visit());

  HistoryAndBookmarkRow update_row;
  update_row.set_visit_count(0);
  TableIDRow id;
  id.url_id = url_row.id();
  TableIDRows ids;
  ids.push_back(id);
  ASSERT_TRUE(urls_sql_handler_.Update(update_row, ids));
  ASSERT_TRUE(history_db_.GetURLRow(row.url_id(), &url_row));
  EXPECT_EQ(0, url_row.visit_count());
  // Last visit is reset.
  EXPECT_EQ(Time(), url_row.last_visit());
}

// Update the last visit time be a time earlier than current one to verify
// update failed.
TEST_F(UrlsSQLHandlerTest, UpdateEarlyLastVisit) {
  HistoryAndBookmarkRow row;
  row.set_raw_url("http://google.com");
  row.set_url(GURL("http://google.com"));
  row.set_visit_count(100);
  row.set_last_visit_time(Time::Now() - TimeDelta::FromDays(10));

  ASSERT_TRUE(urls_sql_handler_.Insert(&row));
  ASSERT_TRUE(visit_sql_handler_.Insert(&row));
  URLRow url_row;
  ASSERT_TRUE(history_db_.GetURLRow(row.url_id(), &url_row));
  EXPECT_EQ(row.url(), url_row.url());
  EXPECT_EQ(100, url_row.visit_count());
  EXPECT_EQ(row.last_visit_time(), url_row.last_visit());

  HistoryAndBookmarkRow update_row;
  update_row.set_last_visit_time(Time::Now() - TimeDelta::FromDays(11));
  TableIDRow id;
  id.url_id = url_row.id();
  TableIDRows ids;
  ids.push_back(id);
  EXPECT_FALSE(urls_sql_handler_.Update(update_row, ids));
}

// Increase the visit count to verify the last visit time is also update.
TEST_F(UrlsSQLHandlerTest, UpdateVisitCountIncreased) {
  HistoryAndBookmarkRow row;
  row.set_raw_url("http://google.com");
  row.set_url(GURL("http://google.com"));
  row.set_visit_count(10);
  row.set_last_visit_time(Time::Now() - TimeDelta::FromDays(10));

  ASSERT_TRUE(urls_sql_handler_.Insert(&row));
  ASSERT_TRUE(visit_sql_handler_.Insert(&row));
  URLRow url_row;
  ASSERT_TRUE(history_db_.GetURLRow(row.url_id(), &url_row));
  EXPECT_EQ(row.url(), url_row.url());
  EXPECT_EQ(10, url_row.visit_count());
  EXPECT_EQ(row.last_visit_time(), url_row.last_visit());

  HistoryAndBookmarkRow update_row;
  update_row.set_visit_count(11);
  TableIDRow id;
  id.url_id = url_row.id();
  TableIDRows ids;
  ids.push_back(id);
  ASSERT_TRUE(urls_sql_handler_.Update(update_row, ids));
  ASSERT_TRUE(history_db_.GetURLRow(row.url_id(), &url_row));
  EXPECT_EQ(row.url(), url_row.url());
  EXPECT_EQ(11, url_row.visit_count());
  EXPECT_LT(row.last_visit_time(), url_row.last_visit());
}

TEST_F(UrlsSQLHandlerTest, Delete) {
  HistoryAndBookmarkRow row;
  row.set_raw_url("http://google.com");
  row.set_url(GURL("http://google.com"));
  row.set_visit_count(10);
  row.set_last_visit_time(Time::Now() - TimeDelta::FromDays(10));

  ASSERT_TRUE(urls_sql_handler_.Insert(&row));
  URLRow url_row;
  ASSERT_TRUE(history_db_.GetURLRow(row.url_id(), &url_row));
  EXPECT_EQ(row.url(), url_row.url());
  EXPECT_EQ(10, url_row.visit_count());
  EXPECT_EQ(row.last_visit_time(), url_row.last_visit());

  TableIDRow id;
  id.url_id = url_row.id();
  TableIDRows ids;
  ids.push_back(id);
  ASSERT_TRUE(urls_sql_handler_.Delete(ids));
  EXPECT_FALSE(history_db_.GetURLRow(row.url_id(), &url_row));
}

}  // namespace history
