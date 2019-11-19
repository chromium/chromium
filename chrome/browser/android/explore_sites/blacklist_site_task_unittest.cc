// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/blacklist_site_task.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/android/explore_sites/explore_sites_schema.h"
#include "components/offline_pages/task/task.h"
#include "components/offline_pages/task/task_test_base.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

using offline_pages::TaskTestBase;

namespace explore_sites {
using InitializationStatus = ExploreSitesStore::InitializationStatus;

const char kGoogleUrl[] = "https://www.google.com";

class ExploreSitesBlacklistSiteTest : public TaskTestBase {
 public:
  ExploreSitesBlacklistSiteTest() = default;
  ~ExploreSitesBlacklistSiteTest() override = default;

  void SetUp() override {
    store_ = std::make_unique<ExploreSitesStore>(task_runner());
    success_ = false;
    callback_called_ = false;
  }

  ExploreSitesStore* store() { return store_.get(); }

  void ExecuteSync(base::RepeatingCallback<bool(sql::Database*)> query) {
    store()->Execute(base::OnceCallback<bool(sql::Database*)>(query),
                     base::BindOnce([](bool result) { ASSERT_TRUE(result); }),
                     false);
    RunUntilIdle();
  }

  void OnAddToBlacklisTaskDone(bool success) {
    success_ = success;
    callback_called_ = true;
  }

  bool success() { return success_; }

  bool callback_called() { return callback_called_; }

  void PopulateActivity();

 private:
  std::unique_ptr<ExploreSitesStore> store_;
  bool success_;
  bool callback_called_;

  DISALLOW_COPY_AND_ASSIGN(ExploreSitesBlacklistSiteTest);
};

void ExploreSitesBlacklistSiteTest::PopulateActivity() {
  ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
    sql::Statement insert_activity(db->GetUniqueStatement(R"(
INSERT INTO activity
(time, category_type, url)
VALUES
(12345, 1, "https://www.google.com"),
(23456, 1, "https://www.example.com/1");
    )"));
    return insert_activity.Run();
  }));
}

TEST_F(ExploreSitesBlacklistSiteTest, StoreFailure) {
  store()->SetInitializationStatusForTesting(InitializationStatus::kFailure,
                                             false);
  BlacklistSiteTask task(store(), kGoogleUrl);
  RunTask(&task);

  // A database failure should be completed but return with an error.
  EXPECT_TRUE(task.complete());
  EXPECT_FALSE(task.result());
}

TEST_F(ExploreSitesBlacklistSiteTest, EmptyUrlTask) {
  PopulateActivity();
  BlacklistSiteTask task(store(), "");
  RunTask(&task);

  // The task should be completed but return with an error.
  EXPECT_TRUE(task.complete());
  EXPECT_FALSE(task.result());

  // Check that DB's site_blacklist table is empty
  // and that activity table is not modified.
  ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
    sql::Statement cat_count_s(
        db->GetUniqueStatement("SELECT COUNT(*) FROM site_blacklist"));
    cat_count_s.Step();
    EXPECT_EQ(0, cat_count_s.ColumnInt(0));

    sql::Statement cat_activity_s(
        db->GetUniqueStatement("SELECT COUNT(*) FROM activity"));
    cat_activity_s.Step();
    EXPECT_EQ(2, cat_activity_s.ColumnInt(0));

    return true;
  }));
}

TEST_F(ExploreSitesBlacklistSiteTest, ValidUrlTask) {
  PopulateActivity();
  BlacklistSiteTask task(store(), kGoogleUrl);
  RunTask(&task);

  // Task should complete successfully
  EXPECT_TRUE(task.complete());
  EXPECT_TRUE(task.result());

  // Check that DB's site_blacklist table contains kGoogleUrl and that
  // kGoogleUrl-related activity is removed from the activity table.
  ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
    sql::Statement cat_count_s(
        db->GetUniqueStatement("SELECT COUNT(*) FROM site_blacklist"));
    cat_count_s.Step();
    EXPECT_EQ(1, cat_count_s.ColumnInt(0));

    sql::Statement cat_data_s(
        db->GetUniqueStatement("SELECT url FROM site_blacklist"));
    cat_data_s.Step();
    EXPECT_EQ(kGoogleUrl, cat_data_s.ColumnString(0));

    sql::Statement cat_activity_s(
        db->GetUniqueStatement("SELECT COUNT(*) FROM activity"));
    cat_activity_s.Step();
    EXPECT_EQ(1, cat_activity_s.ColumnInt(0));

    return true;
  }));
}

}  // namespace explore_sites
