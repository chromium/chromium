// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/clear_activities_task.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/android/explore_sites/explore_sites_schema.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/task/task.h"
#include "components/offline_pages/task/task_test_base.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

using offline_pages::TaskTestBase;

namespace explore_sites {

namespace {
using InitializationStatus = ExploreSitesStore::InitializationStatus;

const char kInsertActivitySql[] =
    "INSERT INTO activity (time, category_type, url) VALUES (?, ?, ?);";
const char kGetAllActivitiesSql[] =
    "SELECT time, category_type, url FROM activity";
const base::Time kJanuary2017 = base::Time::FromDoubleT(1484505871);
const base::Time kBetweenJanuaryAndJune2017 =
    base::Time::FromDoubleT(1490000000);
const base::Time kJune2017 = base::Time::FromDoubleT(1497552271);
const char kUrl1[] = "https://www.google.com";
const char kUrl2[] = "https://www.example.com/1";
const char kUrl3[] = "https://www.example.com/here";

struct ActivityInfo {
  base::Time time;
  int category_type;
  std::string url;
};

}  // namespace

std::vector<ActivityInfo> GetAllActivitiesSync(sql::Database* db) {
  std::vector<ActivityInfo> result;
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kGetAllActivitiesSql));
  while (statement.Step()) {
    base::Time time =
        offline_pages::store_utils::FromDatabaseTime(statement.ColumnInt64(0));
    int category_type = statement.ColumnInt(1);
    std::string url = statement.ColumnString(2);
    result.push_back({time, category_type, url});
  }
  if (!statement.Succeeded())
    result.clear();
  return result;
}

class ClearActivitiesTaskTest : public TaskTestBase {
 public:
  ClearActivitiesTaskTest() = default;
  ~ClearActivitiesTaskTest() override = default;

  void SetUp() override {
    store_ = std::make_unique<ExploreSitesStore>(task_runner());
  }

  ExploreSitesStore* store() { return store_.get(); }

  void ExecuteSync(base::RepeatingCallback<bool(sql::Database*)> query) {
    store()->Execute(base::OnceCallback<bool(sql::Database*)>(query),
                     base::BindOnce([](bool result) { ASSERT_TRUE(result); }),
                     false);
    RunUntilIdle();
  }

  void PopulateActivities();
  void ClearActivities(base::Time begin, base::Time end);
  std::vector<ActivityInfo> GetAllActivities();

  bool callback_called() const { return callback_called_; }
  bool success() const { return success_; }

 private:
  void InsertActivity(base::Time time,
                      int category_type,
                      const std::string& url);
  void OnClearActivitiesDone(bool success);
  void GetAllActivitiesDone(std::vector<ActivityInfo> activities);

  std::unique_ptr<ExploreSitesStore> store_;
  bool callback_called_ = false;
  bool success_ = false;
  std::vector<ActivityInfo> activities_;

  DISALLOW_COPY_AND_ASSIGN(ClearActivitiesTaskTest);
};

void ClearActivitiesTaskTest::PopulateActivities() {
  InsertActivity(kJanuary2017, 1, kUrl1);
  InsertActivity(kBetweenJanuaryAndJune2017, 2, kUrl2);
  InsertActivity(kJune2017, 3, kUrl3);
}

void ClearActivitiesTaskTest::InsertActivity(base::Time time,
                                             int category_type,
                                             const std::string& url) {
  ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
    sql::Statement statement(
        db->GetCachedStatement(SQL_FROM_HERE, kInsertActivitySql));
    statement.BindInt64(0, offline_pages::store_utils::ToDatabaseTime(time));
    statement.BindInt(1, category_type);
    statement.BindString(2, url);
    return statement.Run();
  }));
}

void ClearActivitiesTaskTest::ClearActivities(base::Time begin,
                                              base::Time end) {
  ClearActivitiesTask task(
      store(), begin, end,
      base::BindOnce(&ClearActivitiesTaskTest::OnClearActivitiesDone,
                     base::Unretained(this)));
  RunTask(&task);
}

void ClearActivitiesTaskTest::OnClearActivitiesDone(bool success) {
  success_ = success;
  callback_called_ = true;
}

std::vector<ActivityInfo> ClearActivitiesTaskTest::GetAllActivities() {
  activities_.clear();
  store()->Execute<std::vector<ActivityInfo>>(
      base::BindOnce(&GetAllActivitiesSync),
      base::BindOnce(&ClearActivitiesTaskTest::GetAllActivitiesDone,
                     base::Unretained(this)),
      {});
  RunUntilIdle();
  return activities_;
}

void ClearActivitiesTaskTest::GetAllActivitiesDone(
    std::vector<ActivityInfo> activities) {
  activities_ = activities;
}

TEST_F(ClearActivitiesTaskTest, StoreFailure) {
  store()->SetInitializationStatusForTesting(InitializationStatus::kFailure,
                                             false);
  ClearActivities(kJanuary2017, kJune2017);

  // A database failure should be completed but return with an error.
  EXPECT_TRUE(callback_called());
  EXPECT_FALSE(success());
}

TEST_F(ClearActivitiesTaskTest, EmptyTime) {
  PopulateActivities();
  ClearActivities(base::Time(), base::Time());
  EXPECT_TRUE(callback_called());
  EXPECT_TRUE(success());

  // Nothing should be removed.
  std::vector<ActivityInfo> activities = GetAllActivities();
  ASSERT_EQ(3u, activities.size());
}

TEST_F(ClearActivitiesTaskTest, ClearSome) {
  PopulateActivities();
  ClearActivities(kJanuary2017, kJune2017);
  EXPECT_TRUE(callback_called());
  EXPECT_TRUE(success());

  // Two matching activities are removed and one is left.
  std::vector<ActivityInfo> activities = GetAllActivities();
  ASSERT_EQ(1u, activities.size());
  EXPECT_EQ(kJune2017, activities[0].time);
  EXPECT_EQ(3, activities[0].category_type);
  EXPECT_EQ(kUrl3, activities[0].url);
}

}  // namespace explore_sites
