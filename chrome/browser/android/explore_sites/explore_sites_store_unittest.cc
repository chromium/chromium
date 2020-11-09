// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_store.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace explore_sites {
using InitializationStatus = ExploreSitesStore::InitializationStatus;

class ExploreSitesStoreTest : public testing::Test {
 public:
  ExploreSitesStoreTest()
      : task_runner_(new base::TestMockTimeTaskRunner),
        task_runner_handle_(task_runner_) {
    EXPECT_TRUE(temp_directory_.CreateUniqueTempDir());
  }
  ~ExploreSitesStoreTest() override {}

 protected:
  void TearDown() override {
    // Wait for all the pieces of the store to delete itself properly.
    PumpLoop();
  }

  std::unique_ptr<ExploreSitesStore> BuildStore() {
    auto store = std::make_unique<ExploreSitesStore>(
        base::ThreadTaskRunnerHandle::Get(), TempPath());
    PumpLoop();
    return store;
  }

  void PumpLoop() { task_runner_->RunUntilIdle(); }
  void FastForwardBy(base::TimeDelta delta) {
    task_runner_->FastForwardBy(delta);
  }

  base::TestMockTimeTaskRunner* task_runner() const {
    return task_runner_.get();
  }
  base::FilePath TempPath() const { return temp_directory_.GetPath(); }

  template <typename T>
  T ExecuteSync(ExploreSitesStore* store,
                base::OnceCallback<T(sql::Database*)> run_callback,
                T default_value) {
    bool called = false;
    T result;
    auto result_callback = base::BindLambdaForTesting([&](T async_result) {
      result = std::move(async_result);
      called = true;
    });
    store->Execute<T>(std::move(run_callback), result_callback, default_value);
    PumpLoop();
    EXPECT_TRUE(called);
    return result;
  }

 protected:
  base::ScopedTempDir temp_directory_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
};

// Loads empty store and makes sure that there are no offline pages stored in
// it.
TEST_F(ExploreSitesStoreTest, LoadEmptyStore) {
  std::unique_ptr<ExploreSitesStore> store(BuildStore());
  // Executing something causes initialization.
  auto run_callback =
      base::BindLambdaForTesting([&](sql::Database* db) { return true; });
  bool result = ExecuteSync<bool>(store.get(), run_callback, false);
  EXPECT_TRUE(result);
  EXPECT_EQ(InitializationStatus::kSuccess,
            store->initialization_status_for_testing());
}

TEST_F(ExploreSitesStoreTest, StoreCloses) {
  std::unique_ptr<ExploreSitesStore> store(BuildStore());
  // Executing something causes initialization.
  auto run_callback =
      base::BindLambdaForTesting([&](sql::Database* db) { return true; });
  ExecuteSync<bool>(store.get(), run_callback, false);

  EXPECT_TRUE(task_runner()->HasPendingTask());
  EXPECT_LT(base::TimeDelta(), task_runner()->NextPendingTaskDelay());

  FastForwardBy(ExploreSitesStore::kClosingDelay);
  PumpLoop();
  EXPECT_EQ(InitializationStatus::kNotInitialized,
            store->initialization_status_for_testing());

  // Executing something causes initialization.
  ExecuteSync<bool>(store.get(), run_callback, false);

  EXPECT_EQ(InitializationStatus::kSuccess,
            store->initialization_status_for_testing());
}

}  // namespace explore_sites
