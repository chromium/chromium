// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/get_catalog_task.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/android/explore_sites/blacklist_site_task.h"
#include "chrome/browser/android/explore_sites/explore_sites_schema.h"
#include "components/offline_pages/task/task.h"
#include "components/offline_pages/task/task_test_base.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using offline_pages::TaskTestBase;

namespace explore_sites {
namespace {
using InitializationStatus = ExploreSitesStore::InitializationStatus;

void ValidateTestingCatalog(GetCatalogTask::CategoryList* catalog) {
  EXPECT_FALSE(catalog == nullptr);

  EXPECT_EQ(2U, catalog->size());
  ExploreSitesCategory* cat = &catalog->at(0);
  EXPECT_EQ(4, cat->category_id);
  EXPECT_EQ("5678", cat->version_token);
  EXPECT_EQ(1, cat->category_type);
  EXPECT_EQ("label_1", cat->label);
  EXPECT_EQ(4, cat->ntp_shown_count);
  EXPECT_EQ(2, cat->interaction_count);

  EXPECT_EQ(1U, cat->sites.size());
  ExploreSitesSite* site = &cat->sites[0];
  EXPECT_EQ("https://www.example.com/1", site->url.spec());
  EXPECT_EQ(4, site->category_id);
  EXPECT_EQ("example_1", site->title);
  EXPECT_FALSE(site->is_blacklisted);

  cat = &catalog->at(1);
  EXPECT_EQ(5, cat->category_id);
  EXPECT_EQ("5678", cat->version_token);
  EXPECT_EQ(2, cat->category_type);
  EXPECT_EQ("label_2", cat->label);
  EXPECT_EQ(6, cat->ntp_shown_count);
  EXPECT_EQ(0, cat->interaction_count);

  EXPECT_EQ(1U, cat->sites.size());
  site = &cat->sites[0];
  EXPECT_EQ("https://www.example.com/2", site->url.spec());
  EXPECT_EQ(5, site->category_id);
  EXPECT_EQ("example_2", site->title);
  EXPECT_FALSE(site->is_blacklisted);
}

// Same as above, sites blacklisted are clearly marked.
void ValidateBlacklistTestingCatalog(GetCatalogTask::CategoryList* catalog) {
  EXPECT_FALSE(catalog == nullptr);

  EXPECT_EQ(2U, catalog->size());
  ExploreSitesCategory* cat = &catalog->at(0);
  EXPECT_EQ(4, cat->category_id);
  EXPECT_EQ("5678", cat->version_token);
  EXPECT_EQ(1, cat->category_type);
  EXPECT_EQ("label_1", cat->label);
  EXPECT_EQ(4, cat->ntp_shown_count);
  EXPECT_EQ(2, cat->interaction_count);

  EXPECT_EQ(1U, cat->sites.size());
  ExploreSitesSite* site = &cat->sites[0];
  EXPECT_EQ("https://www.example.com/1", site->url.spec());
  EXPECT_EQ(4, site->category_id);
  EXPECT_EQ("example_1", site->title);
  EXPECT_FALSE(site->is_blacklisted);

  cat = &catalog->at(1);
  EXPECT_EQ(5, cat->category_id);
  EXPECT_EQ("5678", cat->version_token);
  EXPECT_EQ(2, cat->category_type);
  EXPECT_EQ("label_2", cat->label);
  EXPECT_EQ(6, cat->ntp_shown_count);
  EXPECT_EQ(0, cat->interaction_count);

  EXPECT_EQ(1U, cat->sites.size());
  site = &cat->sites[0];
  EXPECT_EQ("https://www.example.com/2", site->url.spec());
  EXPECT_EQ(5, site->category_id);
  EXPECT_EQ("example_2", site->title);
  EXPECT_TRUE(site->is_blacklisted);
}

void ExpectSuccessGetCatalogResult(
    GetCatalogStatus status,
    std::unique_ptr<GetCatalogTask::CategoryList> catalog) {
  EXPECT_EQ(GetCatalogStatus::kSuccess, status);
  ValidateTestingCatalog(catalog.get());
}

void ExpectBlacklistGetCatalogResult(
    GetCatalogStatus status,
    std::unique_ptr<GetCatalogTask::CategoryList> catalog) {
  EXPECT_EQ(GetCatalogStatus::kSuccess, status);
  ValidateBlacklistTestingCatalog(catalog.get());
}

void ExpectEmptyGetCatalogResult(
    GetCatalogStatus status,
    std::unique_ptr<GetCatalogTask::CategoryList> catalog) {
  EXPECT_EQ(GetCatalogStatus::kNoCatalog, status);
  EXPECT_TRUE(catalog == nullptr);
}

void ExpectFailedGetCatalogResult(
    GetCatalogStatus status,
    std::unique_ptr<GetCatalogTask::CategoryList> catalog) {
  EXPECT_EQ(GetCatalogStatus::kFailed, status);
  EXPECT_TRUE(catalog == nullptr);
}

}  // namespace

class ExploreSitesGetCatalogTaskTest : public TaskTestBase {
 public:
  ExploreSitesGetCatalogTaskTest() = default;
  ~ExploreSitesGetCatalogTaskTest() override = default;

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

  void PopulateTestingCatalog();
  void ValidateTestingCatalog(GetCatalogTask::CategoryList* catalog);
  void SetDownloadingAndCurrentVersion(std::string downloading_version_token,
                                       std::string current_version_token);
  std::pair<std::string, std::string> GetCurrentAndDownloadingVersion();
  int GetNumberOfCategoriesInDB();
  int GetNumberOfSitesInDB();
  void BlacklistSite(std::string url);

 private:
  std::unique_ptr<ExploreSitesStore> store_;

  DISALLOW_COPY_AND_ASSIGN(ExploreSitesGetCatalogTaskTest);
};

void ExploreSitesGetCatalogTaskTest::PopulateTestingCatalog() {
  // Populate a catalog with test data.  There are three out-dated categories
  // and three current categories.  Each category (of the 6) has a site except
  // two, but only the current categories should be returned.
  ExecuteSync(base::BindLambdaForTesting([](sql::Database* db) {
    sql::MetaTable meta_table;
    ExploreSitesSchema::InitMetaTable(db, &meta_table);
    meta_table.SetValue("current_catalog", "5678");
    meta_table.DeleteKey("downloading_catalog");
    sql::Statement insert(db->GetUniqueStatement(R"(
INSERT INTO categories
(category_id, version_token, type, label, ntp_shown_count)
VALUES
(1, "1234", 1, "label_1", 5), -- older catalog
(2, "1234", 2, "label_2", 2), -- older catalog
(3, "1234", 3, "label_3", 9), -- older catalog
(4, "5678", 1, "label_1", 4), -- current catalog
(5, "5678", 2, "label_2", 6), -- current catalog
(6, "5678", 3, "label_3", 7); -- current catalog)"));
    if (!insert.Run())
      return false;

    sql::Statement insert_sites(db->GetUniqueStatement(R"(
INSERT INTO sites
(site_id, url, category_id, title)
VALUES
(1, "https://www.example.com/1", 1, "example_old_1"),
(2, "https://www.example.com/2", 2, "example_old_2"),
(3, "https://www.example.com/1", 4, "example_1"),
(4, "https://www.example.com/2", 5, "example_2");
    )"));
    if (!insert_sites.Run())
      return false;

    sql::Statement insert_activity(db->GetUniqueStatement(R"(
INSERT INTO activity
(time, category_type, url)
VALUES
(12345, 1, "https://www.example.com/1"),
(23456, 1, "https://www.example.com/1");
    )"));
    return insert_activity.Run();
  }));
}


void ExploreSitesGetCatalogTaskTest::SetDownloadingAndCurrentVersion(
    std::string downloading_version_token,
    std::string current_version_token) {
  ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
    sql::MetaTable meta_table;
    ExploreSitesSchema::InitMetaTable(db, &meta_table);
    if (downloading_version_token.empty()) {
      meta_table.DeleteKey("downloading_catalog");
    } else {
      meta_table.SetValue("downloading_catalog", downloading_version_token);
    }
    if (current_version_token.empty()) {
      meta_table.DeleteKey("current_catalog");
    } else {
      meta_table.SetValue("current_catalog", current_version_token);
    }
    return true;
  }));
}

std::pair<std::string, std::string>
ExploreSitesGetCatalogTaskTest::GetCurrentAndDownloadingVersion() {
  std::string current_catalog = "";
  std::string downloading_catalog = "";
  ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
    sql::MetaTable meta_table;
    ExploreSitesSchema::InitMetaTable(db, &meta_table);
    meta_table.GetValue("current_catalog", &current_catalog);
    meta_table.GetValue("downloading_catalog", &downloading_catalog);
    return true;
  }));
  return std::make_pair(current_catalog, downloading_catalog);
}

int ExploreSitesGetCatalogTaskTest::GetNumberOfCategoriesInDB() {
  int result = -1;
  ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
    sql::Statement cat_count(
        db->GetUniqueStatement("SELECT COUNT(*) FROM categories"));
    cat_count.Step();
    result = cat_count.ColumnInt(0);
    return true;
  }));
  return result;
}

int ExploreSitesGetCatalogTaskTest::GetNumberOfSitesInDB() {
  int result = -1;
  ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
    sql::Statement site_count(
        db->GetUniqueStatement("SELECT COUNT(*) FROM sites"));
    site_count.Step();
    result = site_count.ColumnInt(0);
    return true;
  }));
  return result;
}

void ExploreSitesGetCatalogTaskTest::BlacklistSite(std::string url) {
  BlacklistSiteTask task(store(), url);
  RunTask(&task);
  // We don't actively wait for completion, so we rely on the blacklist request
  // clearing the task queue before the task in the test proper runs.
}

TEST_F(ExploreSitesGetCatalogTaskTest, StoreFailure) {
  store()->SetInitializationStatusForTesting(InitializationStatus::kFailure,
                                             false);

  GetCatalogTask task(store(), false,
                      base::BindOnce(&ExpectFailedGetCatalogResult));
  RunTask(&task);
}

TEST_F(ExploreSitesGetCatalogTaskTest, EmptyTask) {
  GetCatalogTask task(store(), false,
                      base::BindOnce(&ExpectEmptyGetCatalogResult));
  RunTask(&task);
}

// This tests the behavior of the catalog task when there is already a catalog
// with the current timestamp in the database. This tests both the case where it
// is the "current" catalog, and where it is the "downloading" catalog.
TEST_F(ExploreSitesGetCatalogTaskTest, SimpleCatalog) {
  PopulateTestingCatalog();
  GetCatalogTask task(store(), false,
                      base::BindOnce(&ExpectSuccessGetCatalogResult));
  RunTask(&task);
  // Since |update_current| is false, we should not have changed any rows in the
  // DB.
  EXPECT_EQ(6, GetNumberOfCategoriesInDB());
  EXPECT_EQ(4, GetNumberOfSitesInDB());
}

// This tests that sites on the blacklist do not show up when we do a get
// catalog task.
TEST_F(ExploreSitesGetCatalogTaskTest, BlasklistedSitesMarkedBlacklisted) {
  BlacklistSite("https://www.example.com/2");
  PopulateTestingCatalog();
  GetCatalogTask task(store(), false,
                      base::BindOnce(&ExpectBlacklistGetCatalogResult));
  RunTask(&task);
}

TEST_F(ExploreSitesGetCatalogTaskTest, CatalogWithVersionUpdate) {
  PopulateTestingCatalog();
  // Update the testing catalog so that the older catalog is current and the
  // downloading catalog is ready to upgrade.
  SetDownloadingAndCurrentVersion("5678", "1234");
  GetCatalogTask task(store(), true /* update_current */,
                      base::BindOnce(&ExpectSuccessGetCatalogResult));
  RunTask(&task);

  EXPECT_EQ(std::make_pair(std::string("5678"), std::string()),
            GetCurrentAndDownloadingVersion());
  // The task should have pruned the database.
  EXPECT_EQ(3, GetNumberOfCategoriesInDB());
  EXPECT_EQ(2, GetNumberOfSitesInDB());
}

TEST_F(ExploreSitesGetCatalogTaskTest, CatalogWithoutVersionUpdate) {
  PopulateTestingCatalog();
  // Make "1234" the downloading version, we should not see any changes in the
  // DB if the |update_current| flag is false.
  SetDownloadingAndCurrentVersion("1234", "5678");
  GetCatalogTask task(store(), false /* update_current */,
                      base::BindOnce(&ExpectSuccessGetCatalogResult));
  RunTask(&task);

  EXPECT_EQ(std::make_pair(std::string("5678"), std::string("1234")),
            GetCurrentAndDownloadingVersion());
  EXPECT_EQ(6, GetNumberOfCategoriesInDB());
  EXPECT_EQ(4, GetNumberOfSitesInDB());
}

TEST_F(ExploreSitesGetCatalogTaskTest, InvalidCatalogVersions) {
  PopulateTestingCatalog();
  SetDownloadingAndCurrentVersion("", "");
  GetCatalogTask task(store(), false /* update_current */,
                      base::BindOnce(&ExpectEmptyGetCatalogResult));
  RunTask(&task);
}

TEST_F(ExploreSitesGetCatalogTaskTest,
       GetCatalogWhenOnlyDownloadingCatalogExists) {
  PopulateTestingCatalog();
  SetDownloadingAndCurrentVersion("1234", "");
  ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
    sql::Statement cat_count(db->GetUniqueStatement(
        "DELETE FROM categories where version_token <> \"1234\";"));
    return cat_count.Run();
  }));
  auto callback = base::BindLambdaForTesting(
      [&](GetCatalogStatus status,
          std::unique_ptr<GetCatalogTask::CategoryList> catalog) {
        EXPECT_NE(0U, catalog->size());
      });
  GetCatalogTask task(store(), false /* update_current */, callback);
}

}  // namespace explore_sites
