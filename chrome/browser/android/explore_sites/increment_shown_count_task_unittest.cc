// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/increment_shown_count_task.h"

#include "base/bind.h"
#include "base/test/bind_test_util.h"
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
class ExploreSitesIncrementShownCountTaskTest : public TaskTestBase {
 public:
  ExploreSitesIncrementShownCountTaskTest() = default;
  ~ExploreSitesIncrementShownCountTaskTest() override = default;

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

  void OnIncrementDone(bool success) {
    success_ = success;
    callback_called_ = true;
  }

  bool success() { return success_; }

  bool callback_called() { return callback_called_; }

  void PopulateCategories();

 private:
  std::unique_ptr<ExploreSitesStore> store_;
  bool success_;
  bool callback_called_;

  DISALLOW_COPY_AND_ASSIGN(ExploreSitesIncrementShownCountTaskTest);
};

void ExploreSitesIncrementShownCountTaskTest::PopulateCategories() {
  ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
    sql::Statement insert_categories(db->GetUniqueStatement(R"(
INSERT INTO categories
(category_id, version_token, type, label, ntp_shown_count)
VALUES
(1, "1234", 1, "label_1", 5),
(2, "1234", 2, "label_2", 2)
    )"));
    return insert_categories.Run();
  }));
}

TEST_F(ExploreSitesIncrementShownCountTaskTest, StoreFailure) {
  store()->SetInitializationStatusForTesting(InitializationStatus::kFailure,
                                             false);
  IncrementShownCountTask task(store(), 1);
  RunTask(&task);

  // A database failure should be completed but return with an error.
  EXPECT_TRUE(task.complete());
  EXPECT_FALSE(task.result());
}

TEST_F(ExploreSitesIncrementShownCountTaskTest, IncrementCount) {
  PopulateCategories();
  IncrementShownCountTask task(store(), 1);
  RunTask(&task);

  // Task should complete successfully
  EXPECT_TRUE(task.complete());
  EXPECT_TRUE(task.result());

  // Check that DB's categories table incremented the ntp_shown_count column.
  ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
    sql::Statement cat_s(db->GetUniqueStatement(
        "SELECT category_id, ntp_shown_count FROM categories"));
    cat_s.Step();
    EXPECT_EQ(1, cat_s.ColumnInt(0));  // category_id 1 was incremented.
    EXPECT_EQ(6, cat_s.ColumnInt(1));
    cat_s.Step();
    EXPECT_EQ(2, cat_s.ColumnInt(0));  // category id 2 was not incremented.
    EXPECT_EQ(2, cat_s.ColumnInt(1));

    return true;
  }));
}

}  // namespace explore_sites
