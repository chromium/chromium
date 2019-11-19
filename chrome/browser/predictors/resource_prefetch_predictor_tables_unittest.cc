// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/browser/predictors/predictor_database.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/base/request_priority.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace predictors {

class ResourcePrefetchPredictorTablesTest : public testing::Test {
 public:
  ResourcePrefetchPredictorTablesTest();
  ~ResourcePrefetchPredictorTablesTest() override;

  using RedirectDataMap = std::map<std::string, RedirectData>;
  using OriginDataMap = std::map<std::string, OriginData>;

  void SetUp() override;
  void TearDown() override;

  void DeleteAllData();
  void GetAllData(RedirectDataMap* host_redirect_data,
                  OriginDataMap* origin_data) const;

 protected:
  void ReopenDatabase();
  void TestGetAllData();
  void TestUpdateData();
  void TestDeleteData();
  void TestDeleteAllData();

  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  TestingProfile profile_;
  std::unique_ptr<PredictorDatabase> db_;
  scoped_refptr<ResourcePrefetchPredictorTables> tables_;

 private:
  // Initializes the tables, |test_url_data_|, |test_host_data_|,
  // |test_url_redirect_data_|, |test_host_redirect_data_| and
  // |test_origin_data|.
  void InitializeSampleData();

  // Checks that the input RedirectData are the same, although the redirects
  // can be in different order.
  void TestRedirectDataAreEqual(const RedirectDataMap& lhs,
                                const RedirectDataMap& rhs) const;
  void TestRedirectsAreEqual(const std::vector<RedirectStat>& lhs,
                             const std::vector<RedirectStat>& rhs) const;

  void TestOriginDataAreEqual(const OriginDataMap& lhs,
                              const OriginDataMap& rhs) const;
  void TestOriginStatsAreEqual(const std::vector<OriginStat>& lhs,
                               const std::vector<OriginStat>& rhs) const;

  void AddKey(RedirectDataMap* m, const GURL& url) const;
  void AddKey(OriginDataMap* m, const std::string& key) const;

  std::string GetKeyForRedirectStat(const RedirectStat& stat) const;

  RedirectDataMap test_host_redirect_data_;
  OriginDataMap test_origin_data_;
};

class ResourcePrefetchPredictorTablesReopenTest
    : public ResourcePrefetchPredictorTablesTest {
 public:
  void SetUp() override {
    // Write data to the table, and then reopen the db.
    ResourcePrefetchPredictorTablesTest::SetUp();
    ResourcePrefetchPredictorTablesTest::TearDown();

    ReopenDatabase();
  }
};

ResourcePrefetchPredictorTablesTest::ResourcePrefetchPredictorTablesTest()
    : task_runner_(base::SequencedTaskRunnerHandle::Get()),
      db_(std::make_unique<PredictorDatabase>(&profile_, task_runner_)),
      tables_(db_->resource_prefetch_tables()) {
  content::RunAllTasksUntilIdle();
}

ResourcePrefetchPredictorTablesTest::~ResourcePrefetchPredictorTablesTest() {
}

void ResourcePrefetchPredictorTablesTest::SetUp() {
  DeleteAllData();
  InitializeSampleData();
  content::RunAllTasksUntilIdle();
}

void ResourcePrefetchPredictorTablesTest::TearDown() {
  tables_ = nullptr;
  db_ = nullptr;
  content::RunAllTasksUntilIdle();
}

void ResourcePrefetchPredictorTablesTest::TestGetAllData() {
  RedirectDataMap actual_host_redirect_data;
  OriginDataMap actual_origin_data;

  GetAllData(&actual_host_redirect_data, &actual_origin_data);

  TestRedirectDataAreEqual(test_host_redirect_data_, actual_host_redirect_data);
  TestOriginDataAreEqual(test_origin_data_, actual_origin_data);
}

void ResourcePrefetchPredictorTablesTest::TestDeleteData() {
  std::vector<std::string> urls_to_delete = {"http://fb.com/google",
                                             "http://google.com"};
  std::vector<std::string> hosts_to_delete = {"microsoft.com"};
  tables_->ExecuteDBTaskOnDBSequence(base::BindOnce(
      &LoadingPredictorKeyValueTable<RedirectData>::DeleteData,
      base::Unretained(tables_->host_redirect_table()), hosts_to_delete));

  hosts_to_delete = {"twitter.com"};
  tables_->ExecuteDBTaskOnDBSequence(base::BindOnce(
      &LoadingPredictorKeyValueTable<OriginData>::DeleteData,
      base::Unretained(tables_->origin_table()), hosts_to_delete));

  RedirectDataMap actual_host_redirect_data;
  OriginDataMap actual_origin_data;

  GetAllData(&actual_host_redirect_data, &actual_origin_data);

  RedirectDataMap expected_host_redirect_data;
  OriginDataMap expected_origin_data;
  AddKey(&expected_host_redirect_data, GURL("http://bbc.com"));
  AddKey(&expected_origin_data, "abc.xyz");

  TestRedirectDataAreEqual(expected_host_redirect_data,
                           actual_host_redirect_data);
  TestOriginDataAreEqual(expected_origin_data, actual_origin_data);
}

void ResourcePrefetchPredictorTablesTest::TestUpdateData() {
  RedirectData microsoft = CreateRedirectData("microsoft.com", 21);
  InitializeRedirectStat(microsoft.add_redirect_endpoints(),
                         GURL("https://m.microsoft.com"), 5, 7, 1);
  InitializeRedirectStat(microsoft.add_redirect_endpoints(),
                         GURL("https://microsoft.org"), 7, 2, 0);

  tables_->ExecuteDBTaskOnDBSequence(
      base::BindOnce(&LoadingPredictorKeyValueTable<RedirectData>::UpdateData,
                     base::Unretained(tables_->host_redirect_table()),
                     microsoft.primary_key(), microsoft));

  OriginData twitter = CreateOriginData("twitter.com");
  InitializeOriginStat(twitter.add_origins(), "https://dogs.twitter.com", 10, 1,
                       0, 12., false, true);
  tables_->ExecuteDBTaskOnDBSequence(base::BindOnce(
      &LoadingPredictorKeyValueTable<OriginData>::UpdateData,
      base::Unretained(tables_->origin_table()), twitter.host(), twitter));

  RedirectDataMap actual_host_redirect_data;
  OriginDataMap actual_origin_data;
  GetAllData(&actual_host_redirect_data, &actual_origin_data);

  RedirectDataMap expected_host_redirect_data;
  OriginDataMap expected_origin_data;

  AddKey(&expected_host_redirect_data, GURL("https://bbc.com"));
  expected_host_redirect_data.insert(
      std::make_pair("microsoft.com", microsoft));

  AddKey(&expected_origin_data, "abc.xyz");
  expected_origin_data.insert({"twitter.com", twitter});

  TestRedirectDataAreEqual(expected_host_redirect_data,
                           actual_host_redirect_data);
  TestOriginDataAreEqual(expected_origin_data, actual_origin_data);
}

void ResourcePrefetchPredictorTablesTest::TestDeleteAllData() {
  DeleteAllData();

  RedirectDataMap actual_host_redirect_data;
  OriginDataMap actual_origin_data;
  GetAllData(&actual_host_redirect_data, &actual_origin_data);
  EXPECT_TRUE(actual_host_redirect_data.empty());
  EXPECT_TRUE(actual_origin_data.empty());
}

void ResourcePrefetchPredictorTablesTest::TestRedirectDataAreEqual(
    const RedirectDataMap& lhs,
    const RedirectDataMap& rhs) const {
  EXPECT_EQ(lhs.size(), rhs.size());

  for (const auto& p : rhs) {
    const auto lhs_it = lhs.find(p.first);
    ASSERT_TRUE(lhs_it != lhs.end()) << p.first;
    EXPECT_EQ(lhs_it->second.primary_key(), p.second.primary_key());
    EXPECT_EQ(lhs_it->second.last_visit_time(), p.second.last_visit_time());

    std::vector<RedirectStat> lhs_redirects(
        lhs_it->second.redirect_endpoints().begin(),
        lhs_it->second.redirect_endpoints().end());
    std::vector<RedirectStat> rhs_redirects(
        p.second.redirect_endpoints().begin(),
        p.second.redirect_endpoints().end());

    TestRedirectsAreEqual(lhs_redirects, rhs_redirects);
  }
}

void ResourcePrefetchPredictorTablesTest::TestRedirectsAreEqual(
    const std::vector<RedirectStat>& lhs,
    const std::vector<RedirectStat>& rhs) const {
  EXPECT_EQ(lhs.size(), rhs.size());

  std::map<std::string, RedirectStat> lhs_index;
  // Repeated redirects are not allowed.
  for (const auto& r : lhs) {
    EXPECT_TRUE(
        lhs_index.insert(std::make_pair(GetKeyForRedirectStat(r), r)).second)
        << " r.url()=" << r.url();
  }

  for (const auto& r : rhs) {
    auto lhs_it = lhs_index.find(GetKeyForRedirectStat(r));
    if (lhs_it != lhs_index.end()) {
      EXPECT_EQ(r, lhs_it->second);
      lhs_index.erase(lhs_it);
    } else {
      ADD_FAILURE() << r.url() << " " << r.url_scheme();
    }
  }

  EXPECT_TRUE(lhs_index.empty());
}

void ResourcePrefetchPredictorTablesTest::TestOriginDataAreEqual(
    const OriginDataMap& lhs,
    const OriginDataMap& rhs) const {
  EXPECT_EQ(lhs.size(), rhs.size());

  for (const auto& o : rhs) {
    const auto lhs_it = lhs.find(o.first);
    ASSERT_TRUE(lhs_it != lhs.end()) << o.first;
    std::vector<OriginStat> lhs_origins(lhs_it->second.origins().begin(),
                                        lhs_it->second.origins().end());
    std::vector<OriginStat> rhs_origins(o.second.origins().begin(),
                                        o.second.origins().end());

    TestOriginStatsAreEqual(lhs_origins, rhs_origins);
  }
}

void ResourcePrefetchPredictorTablesTest::TestOriginStatsAreEqual(
    const std::vector<OriginStat>& lhs,
    const std::vector<OriginStat>& rhs) const {
  EXPECT_EQ(lhs.size(), rhs.size());

  std::map<std::string, OriginStat> lhs_index;
  // Repeated origins are not allowed.
  for (const auto& o : lhs)
    EXPECT_TRUE(lhs_index.insert({o.origin(), o}).second);

  for (const auto& o : rhs) {
    auto lhs_it = lhs_index.find(o.origin());
    if (lhs_it != lhs_index.end()) {
      EXPECT_EQ(o, lhs_it->second);
      lhs_index.erase(lhs_it);
    } else {
      ADD_FAILURE() << o.origin();
    }
  }

  EXPECT_TRUE(lhs_index.empty());
}

void ResourcePrefetchPredictorTablesTest::AddKey(RedirectDataMap* m,
                                                 const GURL& url) const {
  auto it = test_host_redirect_data_.find(url.host());
  EXPECT_TRUE(it != test_host_redirect_data_.end());
  m->insert(*it);
}

void ResourcePrefetchPredictorTablesTest::AddKey(OriginDataMap* m,
                                                 const std::string& key) const {
  auto it = test_origin_data_.find(key);
  EXPECT_TRUE(it != test_origin_data_.end());
  m->insert(*it);
}

std::string ResourcePrefetchPredictorTablesTest::GetKeyForRedirectStat(
    const RedirectStat& stat) const {
  return stat.url() + "," + stat.url_scheme() + "," +
         base::NumberToString(stat.url_port());
}

void ResourcePrefetchPredictorTablesTest::DeleteAllData() {
  tables_->ExecuteDBTaskOnDBSequence(base::BindOnce(
      &LoadingPredictorKeyValueTable<RedirectData>::DeleteAllData,
      base::Unretained(tables_->host_redirect_table())));
  tables_->ExecuteDBTaskOnDBSequence(
      base::BindOnce(&LoadingPredictorKeyValueTable<OriginData>::DeleteAllData,
                     base::Unretained(tables_->origin_table())));
}

void ResourcePrefetchPredictorTablesTest::GetAllData(
    RedirectDataMap* host_redirect_data,
    OriginDataMap* origin_data) const {
  tables_->ExecuteDBTaskOnDBSequence(base::BindOnce(
      &LoadingPredictorKeyValueTable<RedirectData>::GetAllData,
      base::Unretained(tables_->host_redirect_table()), host_redirect_data));
  tables_->ExecuteDBTaskOnDBSequence(
      base::BindOnce(&LoadingPredictorKeyValueTable<OriginData>::GetAllData,
                     base::Unretained(tables_->origin_table()), origin_data));
}

void ResourcePrefetchPredictorTablesTest::InitializeSampleData() {
  {  // Host redirect data.
    RedirectData bbc = CreateRedirectData("bbc.com", 9);
    InitializeRedirectStat(bbc.add_redirect_endpoints(),
                           GURL("https://www.bbc.com"), 8, 4, 1);
    InitializeRedirectStat(bbc.add_redirect_endpoints(),
                           GURL("https://m.bbc.com"), 5, 8, 0);
    InitializeRedirectStat(bbc.add_redirect_endpoints(),
                           GURL("https://bbc.co.uk"), 1, 3, 0);
    InitializeRedirectStat(bbc.add_redirect_endpoints(),
                           GURL("http://www.bbc.com"), 8, 4, 1);
    InitializeRedirectStat(bbc.add_redirect_endpoints(),
                           GURL("http://m.bbc.com"), 5, 8, 0);
    InitializeRedirectStat(bbc.add_redirect_endpoints(),
                           GURL("http://bbc.co.uk"), 1, 3, 0);

    RedirectData microsoft = CreateRedirectData("microsoft.com", 10);
    InitializeRedirectStat(microsoft.add_redirect_endpoints(),
                           GURL("https://www.microsoft.com"), 10, 0, 0);

    test_host_redirect_data_.clear();
    test_host_redirect_data_.insert(std::make_pair(bbc.primary_key(), bbc));
    test_host_redirect_data_.insert(
        std::make_pair(microsoft.primary_key(), microsoft));

    tables_->ExecuteDBTaskOnDBSequence(
        base::BindOnce(&LoadingPredictorKeyValueTable<RedirectData>::UpdateData,
                       base::Unretained(tables_->host_redirect_table()),
                       bbc.primary_key(), bbc));
    tables_->ExecuteDBTaskOnDBSequence(
        base::BindOnce(&LoadingPredictorKeyValueTable<RedirectData>::UpdateData,
                       base::Unretained(tables_->host_redirect_table()),
                       microsoft.primary_key(), microsoft));
  }

  {  // Origin data.
    OriginData twitter;
    twitter.set_host("twitter.com");
    InitializeOriginStat(twitter.add_origins(), "https://cdn.twitter.com", 10,
                         1, 0, 12., false, true);
    InitializeOriginStat(twitter.add_origins(), "https://cats.twitter.com", 10,
                         1, 0, 20., false, true);

    OriginData alphabet;
    alphabet.set_host("abc.xyz");
    InitializeOriginStat(alphabet.add_origins(), "https://abc.def.xyz", 10, 1,
                         0, 12., false, true);
    InitializeOriginStat(alphabet.add_origins(), "https://alpha.beta.com", 10,
                         1, 0, 20., false, true);

    test_origin_data_.clear();
    test_origin_data_.insert({"twitter.com", twitter});
    test_origin_data_.insert({"abc.xyz", alphabet});

    tables_->ExecuteDBTaskOnDBSequence(base::BindOnce(
        &LoadingPredictorKeyValueTable<OriginData>::UpdateData,
        base::Unretained(tables_->origin_table()), twitter.host(), twitter));
    tables_->ExecuteDBTaskOnDBSequence(base::BindOnce(
        &LoadingPredictorKeyValueTable<OriginData>::UpdateData,
        base::Unretained(tables_->origin_table()), alphabet.host(), alphabet));
  }
}

void ResourcePrefetchPredictorTablesTest::ReopenDatabase() {
  db_ = std::make_unique<PredictorDatabase>(&profile_, task_runner_);
  content::RunAllTasksUntilIdle();
  tables_ = db_->resource_prefetch_tables();
}

// Test cases.

TEST_F(ResourcePrefetchPredictorTablesTest, ComputeOriginScore) {
  auto compute_score = [](int hits, int misses, double average_position,
                          bool always_access_network, bool accessed_network) {
    OriginStat origin;
    InitializeOriginStat(&origin, "", hits, misses, 0, average_position,
                         always_access_network, accessed_network);
    return ResourcePrefetchPredictorTables::ComputeOriginScore(origin);
  };

  // High-confidence is more important than the rest.
  EXPECT_GT(compute_score(20, 2, 12., false, false),
            compute_score(2, 0, 12., false, false));
  EXPECT_GT(compute_score(20, 2, 12., false, false),
            compute_score(2, 0, 2., true, true));

  // Don't care about the confidence as long as it's high.
  EXPECT_NEAR(compute_score(20, 2, 12., false, false),
              compute_score(50, 6, 12., false, false), 1e-4);

  // Mandatory network access.
  EXPECT_GT(compute_score(1, 1, 12., true, false),
            compute_score(1, 1, 12., false, false));
  EXPECT_GT(compute_score(1, 1, 12., true, false),
            compute_score(1, 1, 12., false, true));
  EXPECT_GT(compute_score(1, 1, 12., true, false),
            compute_score(1, 1, 2., false, true));

  // Accessed network.
  EXPECT_GT(compute_score(1, 1, 12., false, true),
            compute_score(1, 1, 12., false, false));
  EXPECT_GT(compute_score(1, 1, 12., false, true),
            compute_score(1, 1, 2., false, false));

  // All else being equal, position matters.
  EXPECT_GT(compute_score(1, 1, 2., false, false),
            compute_score(1, 1, 12., false, false));
  EXPECT_GT(compute_score(1, 1, 2., true, true),
            compute_score(1, 1, 12., true, true));
}

TEST_F(ResourcePrefetchPredictorTablesTest, GetAllData) {
  TestGetAllData();
}

TEST_F(ResourcePrefetchPredictorTablesTest, UpdateData) {
  TestUpdateData();
}

TEST_F(ResourcePrefetchPredictorTablesTest, DeleteData) {
  TestDeleteData();
}

TEST_F(ResourcePrefetchPredictorTablesTest, DeleteAllData) {
  TestDeleteAllData();
}

TEST_F(ResourcePrefetchPredictorTablesTest, DatabaseVersionIsSet) {
  sql::Database* db = tables_->DB();
  const int version = ResourcePrefetchPredictorTables::kDatabaseVersion;
  EXPECT_EQ(version, ResourcePrefetchPredictorTables::GetDatabaseVersion(db));
}

TEST_F(ResourcePrefetchPredictorTablesTest, DatabaseIsResetWhenIncompatible) {
  const int version = ResourcePrefetchPredictorTables::kDatabaseVersion;
  sql::Database* db = tables_->DB();
  ASSERT_TRUE(
      ResourcePrefetchPredictorTables::SetDatabaseVersion(db, version + 1));
  EXPECT_EQ(version + 1,
            ResourcePrefetchPredictorTables::GetDatabaseVersion(db));

  ReopenDatabase();

  db = tables_->DB();
  ASSERT_EQ(version, ResourcePrefetchPredictorTables::GetDatabaseVersion(db));

  RedirectDataMap host_redirect_data;
  OriginDataMap origin_data;
  GetAllData(&host_redirect_data, &origin_data);
  EXPECT_TRUE(host_redirect_data.empty());
  EXPECT_TRUE(origin_data.empty());
}

TEST_F(ResourcePrefetchPredictorTablesReopenTest, GetAllData) {
  TestGetAllData();
}

TEST_F(ResourcePrefetchPredictorTablesReopenTest, UpdateData) {
  TestUpdateData();
}

TEST_F(ResourcePrefetchPredictorTablesReopenTest, DeleteData) {
  TestDeleteData();
}

TEST_F(ResourcePrefetchPredictorTablesReopenTest, DeleteAllData) {
  TestDeleteAllData();
}

}  // namespace predictors
