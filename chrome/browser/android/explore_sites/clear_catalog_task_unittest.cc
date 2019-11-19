// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/clear_catalog_task.h"

#include <memory>
#include <utility>

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

class ExploreSitesClearCatalogTest : public TaskTestBase {
 public:
  ExploreSitesClearCatalogTest() = default;
  ~ExploreSitesClearCatalogTest() override = default;

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

  bool RunTaskWithResult() {
    bool rv;
    ClearCatalogTask task(
        store(), base::BindLambdaForTesting([&](bool result) { rv = result; }));
    RunTask(&task);
    return rv;
  }

  void AddRowsToCatalog() {
    // Populate a catalog with test data.
    ExecuteSync(base::BindLambdaForTesting([](sql::Database* db) {
      sql::MetaTable meta_table;
      ExploreSitesSchema::InitMetaTable(db, &meta_table);
      meta_table.SetValue("current_catalog", "5678");
      meta_table.SetValue("downloading_catalog", "9101112");
      sql::Statement insert(db->GetUniqueStatement(R"(
INSERT INTO categories
(category_id, version_token, type, label)
VALUES
(3, "5678", 1, "label_1"), -- current catalog
(4, "5678", 2, "label_2"); -- current catalog)"));
      if (!insert.Run())
        return false;

      sql::Statement insert_sites(db->GetUniqueStatement(R"(
INSERT INTO sites
(site_id, url, category_id, title)
VALUES
(3, "https://www.example.com/1", 3, "example_1"),
(4, "https://www.example.com/2", 4, "example_2");)"));
      return insert_sites.Run();
    }));
    ASSERT_NE(std::make_pair(std::string(), std::string()),
              GetCatalogVersions());
    ASSERT_NE(std::make_pair(0, 0), GetCatalogSizes());
  }

  // Returns the current and the downloading catalog versions.
  std::pair<std::string, std::string> GetCatalogVersions() {
    std::string current_version;
    std::string downloading_version;

    ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
      sql::MetaTable meta_table;
      if (!ExploreSitesSchema::InitMetaTable(db, &meta_table))
        return false;

      meta_table.GetValue("current_catalog", &current_version);
      meta_table.GetValue("downloading_catalog", &downloading_version);
      return true;
    }));
    return std::make_pair(current_version, downloading_version);
  }

  // Returns the number of sites and categories in the catalog.
  std::pair<int, int> GetCatalogSizes() {
    int site_count = -1;
    int category_count = -1;
    // Check that DB's site_blacklist table is empty.
    ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
      sql::Statement site_count_s(
          db->GetUniqueStatement("SELECT COUNT(*) FROM sites"));
      site_count_s.Step();
      site_count = site_count_s.ColumnInt(0);

      sql::Statement category_count_s(
          db->GetUniqueStatement("SELECT COUNT(*) FROM categories"));
      category_count_s.Step();
      category_count = category_count_s.ColumnInt(0);

      return true;
    }));

    return std::make_pair(site_count, category_count);
  }

 private:
  std::unique_ptr<ExploreSitesStore> store_;

  DISALLOW_COPY_AND_ASSIGN(ExploreSitesClearCatalogTest);
};

TEST_F(ExploreSitesClearCatalogTest, StoreFailure) {
  store()->SetInitializationStatusForTesting(
      ExploreSitesStore::InitializationStatus::kFailure, false);

  // A database failure should be completed but return with an error.
  EXPECT_FALSE(RunTaskWithResult());
}

TEST_F(ExploreSitesClearCatalogTest, EmptyUrlTask) {
  AddRowsToCatalog();
  RunTaskWithResult();

  EXPECT_EQ(std::make_pair(std::string(), std::string()), GetCatalogVersions());
  EXPECT_EQ(std::make_pair(0, 0), GetCatalogSizes());
}

}  // namespace explore_sites
