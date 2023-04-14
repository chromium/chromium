// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_database.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"

class LCPCriticalPathPredictorDatabaseTest : public testing::Test {
 public:
  LCPCriticalPathPredictorDatabaseTest() = default;
  void TearDown() override {
    // Wait until LCPCriticalPathPredictorDatabase finishes closing its database
    // asynchronously, so as not to leak after the test concludes.
    task_environment_.RunUntilIdle();
  }

 protected:
  std::unique_ptr<LCPCriticalPathPredictorDatabase> CreateDatabase(
      base::OnceCallback<bool(sql::Database*)> db_opener) {
    base::test::TestFuture<std::unique_ptr<LCPCriticalPathPredictorDatabase>>
        database;
    LCPCriticalPathPredictorDatabase::Create(
        std::move(db_opener), base::SingleThreadTaskRunner::GetCurrentDefault(),
        /*flush_delay_for_writes=*/base::TimeDelta(), database.GetCallback());
    return database.Take();
  }

  base::test::TaskEnvironment task_environment_;
};

TEST_F(LCPCriticalPathPredictorDatabaseTest, Initializes) {
  std::unique_ptr<LCPCriticalPathPredictorDatabase> database =
      CreateDatabase(base::BindOnce([](sql::Database* db) {
        CHECK(db->OpenInMemory());
        return true;
      }));
  ASSERT_TRUE(database);
  // LCPElement KeyValueData should be available.
  EXPECT_TRUE(database->LCPElementData());
}

TEST_F(LCPCriticalPathPredictorDatabaseTest, StillInitializesOnDbOpenFailure) {
  // The database opener callback returning failure should still lead to a
  // usable state (albeit not one that writes through to a database).
  std::unique_ptr<LCPCriticalPathPredictorDatabase> database = CreateDatabase(
      base::BindOnce([](sql::Database* unused) { return false; }));
  ASSERT_TRUE(database);
  // LCPElement KeyValueData should be available.
  EXPECT_TRUE(database->LCPElementData());
}
