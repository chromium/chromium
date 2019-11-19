// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/record_site_click_task.h"

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

const char kUrl[] = "https://www.example.com";
const int kType = 5;

class ExploreSitesRecordSiteClickTest : public TaskTestBase {
 public:
  ExploreSitesRecordSiteClickTest() = default;
  ~ExploreSitesRecordSiteClickTest() override = default;

  void SetUp() override {
    store_ = std::make_unique<ExploreSitesStore>(task_runner());
    success_ = false;
  }

  ExploreSitesStore* store() { return store_.get(); }

  void ExecuteSync(base::RepeatingCallback<bool(sql::Database*)> query) {
    store()->Execute(base::OnceCallback<bool(sql::Database*)>(query),
                     base::BindOnce([](bool result) { ASSERT_TRUE(result); }),
                     false);
    RunUntilIdle();
  }

  bool success() { return success_; }

  void PopulateActivity();

 private:
  std::unique_ptr<ExploreSitesStore> store_;
  bool success_;

  DISALLOW_COPY_AND_ASSIGN(ExploreSitesRecordSiteClickTest);
};

void ExploreSitesRecordSiteClickTest::PopulateActivity() {
  ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
    for (int i = 0; i < 200; i++) {
      sql::Statement insert_activity(db->GetUniqueStatement(R"(
INSERT INTO activity
(time, category_type, url)
VALUES
(?, 1, "https://www.google.com");
      )"));
      insert_activity.BindInt64(0, i + 1);
      insert_activity.Run();
    }
    return true;
  }));
}

TEST_F(ExploreSitesRecordSiteClickTest, StoreFailure) {
  store()->SetInitializationStatusForTesting(InitializationStatus::kFailure,
                                             false);
  RecordSiteClickTask task(store(), kUrl, kType);
  RunTask(&task);

  // A database failure should be completed but return with an error.
  EXPECT_TRUE(task.complete());
  EXPECT_FALSE(task.result());
}

TEST_F(ExploreSitesRecordSiteClickTest, EmptyUrlTask) {
  RecordSiteClickTask task(store(), "", kType);
  RunTask(&task);

  // The task should be completed but return with an error.
  EXPECT_TRUE(task.complete());
  EXPECT_FALSE(task.result());

  // Check that activity table is not modified.
  ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
    sql::Statement cat_activity_s(
        db->GetUniqueStatement("SELECT COUNT(*) FROM activity"));
    cat_activity_s.Step();
    EXPECT_EQ(0, cat_activity_s.ColumnInt(0));

    return true;
  }));
}

TEST_F(ExploreSitesRecordSiteClickTest, ValidUrlTask) {
  RecordSiteClickTask task(store(), kUrl, kType);
  RunTask(&task);

  // The task should be completed but return with an error.
  EXPECT_TRUE(task.complete());
  EXPECT_TRUE(task.result());

  // Check that activity table is not modified.
  ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
    sql::Statement cat_activity_s(db->GetUniqueStatement(
        "SELECT COUNT(*), url, category_type FROM activity"));
    cat_activity_s.Step();
    EXPECT_EQ(1, cat_activity_s.ColumnInt(0));
    EXPECT_EQ(kUrl, cat_activity_s.ColumnString(1));
    EXPECT_EQ(kType, cat_activity_s.ColumnInt(2));

    return true;
  }));
}

TEST_F(ExploreSitesRecordSiteClickTest, RemoveWhenOver200) {
  PopulateActivity();
  RecordSiteClickTask task(store(), kUrl, kType);
  RunTask(&task);

  // The task should be completed but return with an error.
  EXPECT_TRUE(task.complete());
  EXPECT_TRUE(task.result());

  // Check that activity table is not modified.
  ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
    sql::Statement cat_activity_s(
        db->GetUniqueStatement("SELECT COUNT(*) FROM activity"));
    cat_activity_s.Step();
    EXPECT_EQ(200, cat_activity_s.ColumnInt(0));

    return true;
  }));
}
}  // namespace explore_sites
