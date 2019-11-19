// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/get_images_task.h"

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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using offline_pages::TaskTestBase;

namespace explore_sites {
using InitializationStatus = ExploreSitesStore::InitializationStatus;

class ExploreSitesGetImagesTaskTest : public TaskTestBase {
 public:
  ExploreSitesGetImagesTaskTest() = default;
  ~ExploreSitesGetImagesTaskTest() override = default;

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
  void UpdateCatalogVersions(std::string current_version,
                             std::string downloading_version);
  void PopulateInvalidRowsInCatalog();

  void ExpectEmptyImageList(EncodedImageList images) {
    EXPECT_EQ(0U, images.size());
  }

  EncodedImageListCallback StoreResult() {
    return base::BindLambdaForTesting(
        [&](EncodedImageList result) { last_result = std::move(result); });
  }

  void ExpectSummaryBitmapsInOrder() {
    std::vector<std::string> ordered_bitmaps = {"bytes1", "bytes3", "bytes2"};
    EXPECT_EQ(3U, last_result.size());
    for (int i = 0; i < 3; i++) {
      std::vector<uint8_t>& result = *last_result[i];
      EXPECT_EQ(ordered_bitmaps[i], std::string(result.begin(), result.end()));
    }
  }

  EncodedImageList last_result;

 private:
  std::unique_ptr<ExploreSitesStore> store_;

  DISALLOW_COPY_AND_ASSIGN(ExploreSitesGetImagesTaskTest);
};

void ExploreSitesGetImagesTaskTest::PopulateTestingCatalog() {
  ExecuteSync(base::BindLambdaForTesting([](sql::Database* db) {
    sql::MetaTable meta_table;
    ExploreSitesSchema::InitMetaTable(db, &meta_table);
    meta_table.SetValue("current_catalog", 5678);
    meta_table.DeleteKey("downloading_catalog");
    sql::Statement insert(db->GetUniqueStatement(R"(
INSERT INTO categories
(category_id, version_token, type, label)
VALUES
(3, "5678", 1, "label_1"),
(4, "5678", 2, "label_2");)"));
    if (!insert.Run())
      return false;

    sql::Statement insert_sites(db->GetUniqueStatement(R"(
INSERT INTO sites
(site_id, url, category_id, title, favicon)
VALUES
(1, "https://www.example.com/1", 3, "example_1", "bytes1"),
(2, "https://www.example.com/2", 4, "example_2", "bytes2"),
(3, "https://www.example.com/3", 3, "example_3", "bytes3"),
(4, "https://www.example.com/4", 4, "example_4", "bytes4");
    )"));
    return insert_sites.Run();
  }));
}

void ExploreSitesGetImagesTaskTest::UpdateCatalogVersions(
    std::string current_version,
    std::string downloading_version) {
  ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
    sql::MetaTable meta_table;
    ExploreSitesSchema::InitMetaTable(db, &meta_table);
    meta_table.SetValue(ExploreSitesSchema::kDownloadingCatalogKey,
                        downloading_version);
    meta_table.SetValue(ExploreSitesSchema::kCurrentCatalogKey,
                        current_version);
    return true;
  }));
}

void ExploreSitesGetImagesTaskTest::PopulateInvalidRowsInCatalog() {
  ExecuteSync(base::BindLambdaForTesting([](sql::Database* db) {
    sql::MetaTable meta_table;
    ExploreSitesSchema::InitMetaTable(db, &meta_table);
    meta_table.SetValue(ExploreSitesSchema::kDownloadingCatalogKey, "5678");
    meta_table.DeleteKey(ExploreSitesSchema::kCurrentCatalogKey);
    sql::Statement insert(db->GetUniqueStatement(R"(
INSERT INTO categories
(category_id, version_token, type, label)
VALUES
(1, "XXXX", 1, "bad_1"),
(2, "XXXX", 2, "bad_2");)"));
    if (!insert.Run())
      return false;

    sql::Statement insert_sites(db->GetUniqueStatement(R"(
INSERT INTO sites
(site_id, url, category_id, title, favicon)
VALUES
(5, "https://www.bad.com/1", 1, "bad", "bad - used unknown version"),
(6, "https://www.bad.com/2", 2, "bad", "bad - used unknown version");
    )"));
    return insert_sites.Run();
  }));
}

TEST_F(ExploreSitesGetImagesTaskTest, StoreFailure) {
  store()->SetInitializationStatusForTesting(InitializationStatus::kFailure,
                                             false);

  GetImagesTask task(store(), 1, StoreResult());
  RunTask(&task);
  EXPECT_EQ(0U, last_result.size());
}

TEST_F(ExploreSitesGetImagesTaskTest, SiteDoesNotExist) {
  GetImagesTask task(store(), 43, StoreResult());
  RunTask(&task);
  EXPECT_EQ(0U, last_result.size());
}

TEST_F(ExploreSitesGetImagesTaskTest, CategoryDoesNotExist) {
  GetImagesTask task(store(), 43 /* invalid id */, 4, StoreResult());
  RunTask(&task);
  EXPECT_EQ(0U, last_result.size());
}

TEST_F(ExploreSitesGetImagesTaskTest, SiteExistsAndHasFavicon) {
  PopulateTestingCatalog();
  GetImagesTask task(store(), 1, StoreResult());
  RunTask(&task);

  EXPECT_EQ(1U, last_result.size());
  std::vector<uint8_t>& result = *last_result[0];
  EXPECT_EQ("bytes1", std::string(result.begin(), result.end()));
  last_result.clear();

  GetImagesTask task2(store(), 3, StoreResult());
  RunTask(&task2);

  EXPECT_EQ(1U, last_result.size());
  std::vector<uint8_t>& result3 = *last_result[0];
  EXPECT_EQ("bytes3", std::string(result3.begin(), result3.end()));
}

TEST_F(ExploreSitesGetImagesTaskTest, SitesExistAndNotBlacklisted) {
  PopulateTestingCatalog();
  GetImagesTask task(store(), 3, 4, StoreResult());
  RunTask(&task);

  EXPECT_EQ(2U, last_result.size());
  std::vector<uint8_t>& result = *last_result[0];
  EXPECT_EQ("bytes1", std::string(result.begin(), result.end()));
  std::vector<uint8_t>& result2 = *last_result[1];
  EXPECT_EQ("bytes3", std::string(result2.begin(), result2.end()));
}

TEST_F(ExploreSitesGetImagesTaskTest, SitesExistAndBlacklisted) {
  PopulateTestingCatalog();
  ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
    sql::Statement insert(db->GetUniqueStatement(R"(
INSERT INTO site_blacklist
(url, date_removed)
VALUES
("https://www.example.com/1", 123);)"));
    return insert.Run();
  }));
  GetImagesTask task(store(), 3, 4, StoreResult());
  RunTask(&task);

  EXPECT_EQ(1U, last_result.size());
  std::vector<uint8_t>& result = *last_result[0];
  EXPECT_EQ("bytes3", std::string(result.begin(), result.end()));
  last_result.clear();
}

TEST_F(ExploreSitesGetImagesTaskTest, TooManySitesToReturn) {
  PopulateTestingCatalog();
  // Add 3 more sites to the interesting category, but limit the site max to 4.
  ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
    sql::Statement insert(db->GetUniqueStatement(R"(
INSERT INTO sites
(site_id, url, category_id, title, favicon)
VALUES
(5, "https://www.example.com/5", 3, "example_5", "bytes5"),
(6, "https://www.example.com/6", 3, "example_6", "bytes6"),
(7, "https://www.example.com/7", 3, "example_7", "bytes7");)"));
    return insert.Run();
  }));
  GetImagesTask task(store(), 3, 4, StoreResult());
  RunTask(&task);

  EXPECT_EQ(4U, last_result.size());
}

TEST_F(ExploreSitesGetImagesTaskTest, SkipMissingFavicons) {
  PopulateTestingCatalog();
  ExecuteSync(base::BindLambdaForTesting([&](sql::Database* db) {
    sql::Statement insert(db->GetUniqueStatement(R"(
INSERT INTO sites
(site_id, url, category_id, title, favicon)
VALUES
(5, "https://www.example.com/5", 3, "example_5", NULL),
(6, "https://www.example.com/6", 3, "example_6", ""),
(7, "https://www.example.com/7", 3, "example_7", "bytes7");
)"));
    return insert.Run();
  }));
  GetImagesTask task(store(), 3, 3, StoreResult());
  RunTask(&task);

  EXPECT_EQ(3U, last_result.size());
  std::vector<uint8_t>& result3 = *last_result[2];
  EXPECT_EQ("bytes7", std::string(result3.begin(), result3.end()));
}

TEST_F(ExploreSitesGetImagesTaskTest, SummaryImage) {
  PopulateTestingCatalog();
  PopulateInvalidRowsInCatalog();
  UpdateCatalogVersions("5678", "XXXX");
  GetImagesTask task(store(), GetImagesTask::DataType::kSummary, 3,
                     StoreResult());
  RunTask(&task);
  ExpectSummaryBitmapsInOrder();
}

TEST_F(ExploreSitesGetImagesTaskTest,
       SummaryImageUsesDownloadingCatalogIfNecessary) {
  PopulateTestingCatalog();
  PopulateInvalidRowsInCatalog();
  UpdateCatalogVersions("", "5678");
  GetImagesTask task(store(), GetImagesTask::DataType::kSummary, 3,
                     StoreResult());
  RunTask(&task);
  ExpectSummaryBitmapsInOrder();
}

TEST_F(ExploreSitesGetImagesTaskTest, SummaryImageNoResultsIfNoCatalogVersion) {
  PopulateTestingCatalog();
  PopulateInvalidRowsInCatalog();
  UpdateCatalogVersions("", "");
  GetImagesTask task(store(), GetImagesTask::DataType::kSummary, 3,
                     StoreResult());
  RunTask(&task);
  EXPECT_EQ(0U, last_result.size());
}

}  // namespace explore_sites
