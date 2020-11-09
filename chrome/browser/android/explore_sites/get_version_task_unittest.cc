// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/get_version_task.h"

#include <memory>

#include "base/bind.h"
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

class ExploreSitesGetVersionTaskTest : public TaskTestBase {
 public:
  ExploreSitesGetVersionTaskTest() = default;
  ~ExploreSitesGetVersionTaskTest() override = default;

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

  void SetCurrentVersion(std::string version) {
    SetVersion("current_catalog", version);
  }

  void SetDownloadingVersion(std::string version) {
    SetVersion("downloading_catalog", version);
  }

  void SetVersion(const char* key, std::string version) {
    ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
      sql::MetaTable meta_table;
      if (!ExploreSitesSchema::InitMetaTable(db, &meta_table))
        return false;

      return meta_table.SetValue(key, version);
    }));
  }

 private:
  std::unique_ptr<ExploreSitesStore> store_;
  bool success_;
  bool callback_called_;

  DISALLOW_COPY_AND_ASSIGN(ExploreSitesGetVersionTaskTest);
};

TEST_F(ExploreSitesGetVersionTaskTest, StoreFailure) {
  store()->SetInitializationStatusForTesting(InitializationStatus::kFailure,
                                             false);
  GetVersionTask task(store(),
                      base::BindLambdaForTesting(
                          [&](std::string result) { EXPECT_EQ("", result); }));
  RunTask(&task);
}

TEST_F(ExploreSitesGetVersionTaskTest, NoVersionsAvailable) {
  std::string version = "unexpected";
  GetVersionTask task(store(),
                      base::BindLambdaForTesting(
                          [&](std::string result) { version = result; }));
  RunTask(&task);
  EXPECT_EQ("", version);
}

TEST_F(ExploreSitesGetVersionTaskTest, CurrentCatalogNoDownloadingCatalog) {
  std::string current_version = "1234x";
  SetCurrentVersion(current_version);
  std::string version;
  GetVersionTask task(store(),
                      base::BindLambdaForTesting(
                          [&](std::string result) { version = result; }));
  RunTask(&task);

  EXPECT_EQ(current_version, version);
}

TEST_F(ExploreSitesGetVersionTaskTest, CurrentCatalogAndDownloadingCatalog) {
  std::string current_version = "1234x";
  SetCurrentVersion(current_version);
  std::string downloading_version = "5678x";
  SetDownloadingVersion(downloading_version);
  std::string version;
  GetVersionTask task(store(),
                      base::BindLambdaForTesting(
                          [&](std::string result) { version = result; }));
  RunTask(&task);

  EXPECT_EQ(downloading_version, version);
}

}  // namespace explore_sites
