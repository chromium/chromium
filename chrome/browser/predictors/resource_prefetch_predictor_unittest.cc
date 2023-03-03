// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_predictor.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/sessions/core/session_id.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/base/network_isolation_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::StrictMock;
using testing::UnorderedElementsAre;

namespace predictors {
namespace {

using RedirectDataMap = std::map<std::string, RedirectData>;
using OriginDataMap = std::map<std::string, OriginData>;

template <typename T>
class FakeLoadingPredictorKeyValueTable
    : public sqlite_proto::KeyValueTable<T> {
 public:
  FakeLoadingPredictorKeyValueTable() : sqlite_proto::KeyValueTable<T>("") {}
  void GetAllData(std::map<std::string, T>* data_map,
                  sql::Database* db) const override {
    *data_map = data_;
  }
  void UpdateData(const std::string& key,
                  const T& data,
                  sql::Database* db) override {
    data_[key] = data;
  }
  void DeleteData(const std::vector<std::string>& keys,
                  sql::Database* db) override {
    for (const auto& key : keys)
      data_.erase(key);
  }
  void DeleteAllData(sql::Database* db) override { data_.clear(); }

  std::map<std::string, T> data_;
};

class MockResourcePrefetchPredictorTables
    : public ResourcePrefetchPredictorTables {
 public:
  using DBTask = base::OnceCallback<void(sql::Database*)>;

  explicit MockResourcePrefetchPredictorTables(
      scoped_refptr<base::SequencedTaskRunner> db_task_runner)
      : ResourcePrefetchPredictorTables(std::move(db_task_runner)) {}

  void ScheduleDBTask(const base::Location& from_here, DBTask task) override {
    ExecuteDBTaskOnDBSequence(std::move(task));
  }

  void ExecuteDBTaskOnDBSequence(DBTask task) override {
    std::move(task).Run(nullptr);
  }

  sqlite_proto::KeyValueTable<RedirectData>* host_redirect_table() override {
    return &host_redirect_table_;
  }

  sqlite_proto::KeyValueTable<OriginData>* origin_table() override {
    return &origin_table_;
  }

  FakeLoadingPredictorKeyValueTable<RedirectData> host_redirect_table_;
  FakeLoadingPredictorKeyValueTable<OriginData> origin_table_;

 protected:
  ~MockResourcePrefetchPredictorTables() override = default;
};

class MockResourcePrefetchPredictorObserver : public TestObserver {
 public:
  explicit MockResourcePrefetchPredictorObserver(
      ResourcePrefetchPredictor* predictor)
      : TestObserver(predictor) {}

  MOCK_METHOD1(OnNavigationLearned, void(const PageRequestSummary& summary));
};

}  // namespace

class ResourcePrefetchPredictorTest : public testing::Test {
 public:
  ResourcePrefetchPredictorTest();
  ~ResourcePrefetchPredictorTest() override;
  void SetUp() override;
  void TearDown() override;

 protected:
  void InitializePredictor() {
    loading_predictor_->StartInitialization();
    db_task_runner_->RunUntilIdle();
    profile_->BlockUntilHistoryProcessesPendingRequests();
  }

  void ResetPredictor(bool small_db = true) {
    if (loading_predictor_)
      loading_predictor_->Shutdown();

    LoadingPredictorConfig config;
    PopulateTestConfig(&config, small_db);
    loading_predictor_ =
        std::make_unique<LoadingPredictor>(config, profile_.get());
    predictor_ = loading_predictor_->resource_prefetch_predictor();
    predictor_->set_mock_tables(mock_tables_);
  }

  void InitializeSampleData();

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  scoped_refptr<base::TestSimpleTaskRunner> db_task_runner_;

  std::unique_ptr<LoadingPredictor> loading_predictor_;
  raw_ptr<ResourcePrefetchPredictor> predictor_;
  scoped_refptr<StrictMock<MockResourcePrefetchPredictorTables>> mock_tables_;

  RedirectDataMap test_host_redirect_data_;
  OriginDataMap test_origin_data_;

  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

ResourcePrefetchPredictorTest::ResourcePrefetchPredictorTest()
    : db_task_runner_(base::MakeRefCounted<base::TestSimpleTaskRunner>()),
      mock_tables_(
          base::MakeRefCounted<StrictMock<MockResourcePrefetchPredictorTables>>(
              db_task_runner_)) {}

ResourcePrefetchPredictorTest::~ResourcePrefetchPredictorTest() = default;

void ResourcePrefetchPredictorTest::SetUp() {
  InitializeSampleData();

  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(HistoryServiceFactory::GetInstance(),
                                    HistoryServiceFactory::GetDefaultFactory());
  profile_ = profile_builder.Build();

  profile_->BlockUntilHistoryProcessesPendingRequests();
  CHECK(HistoryServiceFactory::GetForProfile(
      profile_.get(), ServiceAccessType::EXPLICIT_ACCESS));
  // Initialize the predictor with empty data.
  ResetPredictor();
  // The first creation of the LoadingPredictor constructs the PredictorDatabase
  // for the |profile_|. The PredictorDatabase is initialized asynchronously and
  // we have to wait for the initialization completion even though the database
  // object is later replaced by a mock object.
  content::RunAllTasksUntilIdle();
  CHECK_EQ(predictor_->initialization_state_,
           ResourcePrefetchPredictor::NOT_INITIALIZED);
  InitializePredictor();
  CHECK_EQ(predictor_->initialization_state_,
           ResourcePrefetchPredictor::INITIALIZED);

  histogram_tester_ = std::make_unique<base::HistogramTester>();
}

void ResourcePrefetchPredictorTest::TearDown() {
  EXPECT_EQ(predictor_->host_redirect_data_->GetAllCached(),
            mock_tables_->host_redirect_table_.data_);
  EXPECT_EQ(predictor_->origin_data_->GetAllCached(),
            mock_tables_->origin_table_.data_);
  loading_predictor_->Shutdown();
}

void ResourcePrefetchPredictorTest::InitializeSampleData() {
  {  // Host redirect data.
    RedirectData bbc = CreateRedirectData("bbc.com", 9);
    InitializeRedirectStat(bbc.add_redirect_endpoints(),
                           GURL("https://www.bbc.com"), 8, 4, 1);
    InitializeRedirectStat(bbc.add_redirect_endpoints(),
                           GURL("https://m.bbc.com"), 5, 8, 0);
    InitializeRedirectStat(bbc.add_redirect_endpoints(),
                           GURL("http://bbc.co.uk"), 1, 3, 0);
    InitializeRedirectStat(bbc.add_redirect_endpoints(),
                           GURL("https://bbc.co.uk"), 1, 3, 0);

    RedirectData microsoft = CreateRedirectData("microsoft.com", 10);
    InitializeRedirectStat(microsoft.add_redirect_endpoints(),
                           GURL("https://www.microsoft.com"), 10, 0, 0);

    test_host_redirect_data_.clear();
    test_host_redirect_data_.insert(std::make_pair(bbc.primary_key(), bbc));
    test_host_redirect_data_.insert(
        std::make_pair(microsoft.primary_key(), microsoft));
  }

  {  // Origin data.
    OriginData google = CreateOriginData("google.com", 12);
    InitializeOriginStat(google.add_origins(), "https://static.google.com", 12,
                         0, 0, 3., false, true);
    InitializeOriginStat(google.add_origins(), "https://cats.google.com", 12, 0,
                         0, 5., true, true);
    test_origin_data_.insert({"google.com", google});

    OriginData twitter = CreateOriginData("twitter.com", 42);
    InitializeOriginStat(twitter.add_origins(), "https://static.twitter.com",
                         12, 0, 0, 3., false, true);
    InitializeOriginStat(twitter.add_origins(), "https://random.140chars.com",
                         12, 0, 0, 3., false, true);
    test_origin_data_.insert({"twitter.com", twitter});
  }
}

// Tests that the predictor initializes correctly without any data.
TEST_F(ResourcePrefetchPredictorTest, LazilyInitializeEmpty) {
  EXPECT_TRUE(mock_tables_->host_redirect_table_.data_.empty());
  EXPECT_TRUE(mock_tables_->origin_table_.data_.empty());
}

// Tests that the history and the db tables data are loaded correctly.
TEST_F(ResourcePrefetchPredictorTest, LazilyInitializeWithData) {
  mock_tables_->host_redirect_table_.data_ = test_host_redirect_data_;
  mock_tables_->origin_table_.data_ = test_origin_data_;

  ResetPredictor();
  InitializePredictor();

  // Test that the internal variables correctly initialized.
  EXPECT_EQ(predictor_->initialization_state_,
            ResourcePrefetchPredictor::INITIALIZED);

  // Integrity of the cache and the backend storage is checked on TearDown.
}

// Single navigation that will be recorded. Will check for duplicate
// resources and also for number of resources saved.
TEST_F(ResourcePrefetchPredictorTest, NavigationUrlNotInDB) {
  std::vector<blink::mojom::ResourceLoadInfoPtr> resources;
  resources.push_back(CreateResourceLoadInfo("http://www.google.com"));
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/style1.css",
                             network::mojom::RequestDestination::kStyle));
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/script1.js",
                             network::mojom::RequestDestination::kScript));
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/script2.js",
                             network::mojom::RequestDestination::kScript));
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/script1.js",
                             network::mojom::RequestDestination::kScript));
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/image1.png",
                             network::mojom::RequestDestination::kImage));
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/image2.png",
                             network::mojom::RequestDestination::kImage));
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/style2.css",
                             network::mojom::RequestDestination::kStyle));
  resources.push_back(
      CreateResourceLoadInfo("http://static.google.com/style2-no-store.css",
                             network::mojom::RequestDestination::kStyle,
                             /* always_access_network */ true));
  resources.push_back(CreateResourceLoadInfoWithRedirects(
      {"http://reader.google.com/style.css",
       "http://dev.null.google.com/style.css"},
      network::mojom::RequestDestination::kStyle));
  resources.back()->network_info->always_access_network = true;

  auto page_summary = CreatePageRequestSummary(
      "http://www.google.com", "http://www.google.com", resources);

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(predictor_);
  EXPECT_CALL(mock_observer, OnNavigationLearned(page_summary));

  predictor_->RecordPageRequestSummary(
      std::make_unique<PageRequestSummary>(page_summary));
  profile_->BlockUntilHistoryProcessesPendingRequests();

  OriginData origin_data = CreateOriginData("www.google.com");
  InitializeOriginStat(origin_data.add_origins(), "http://www.google.com/", 1,
                       0, 0, 1., false, true);
  InitializeOriginStat(origin_data.add_origins(), "http://static.google.com/",
                       1, 0, 0, 3., true, true);
  InitializeOriginStat(origin_data.add_origins(), "http://dev.null.google.com/",
                       1, 0, 0, 5., true, true);
  InitializeOriginStat(origin_data.add_origins(), "http://google.com/", 1, 0, 0,
                       2., false, true);
  InitializeOriginStat(origin_data.add_origins(), "http://reader.google.com/",
                       1, 0, 0, 4., false, true);
  EXPECT_EQ(mock_tables_->origin_table_.data_,
            OriginDataMap({{origin_data.host(), origin_data}}));

  RedirectData host_redirect_data = CreateRedirectData("www.google.com");
  InitializeRedirectStat(host_redirect_data.add_redirect_endpoints(),
                         GURL("http://www.google.com"), 1, 0, 0);
  EXPECT_EQ(mock_tables_->host_redirect_table_.data_,
            RedirectDataMap(
                {{host_redirect_data.primary_key(), host_redirect_data}}));
}

// Single navigation that will be recorded. Will check for duplicate
// resources and also for number of resources saved.
TEST_F(ResourcePrefetchPredictorTest,
       NavigationUrlNotInDB_LoadingPredictorDisregardAlwaysAccessesNetwork) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kLoadingPredictorDisregardAlwaysAccessesNetwork);

  std::vector<blink::mojom::ResourceLoadInfoPtr> resources;
  resources.push_back(CreateResourceLoadInfo("http://www.google.com"));
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/style1.css",
                             network::mojom::RequestDestination::kStyle));
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/script1.js",
                             network::mojom::RequestDestination::kScript));
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/script2.js",
                             network::mojom::RequestDestination::kScript));
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/script1.js",
                             network::mojom::RequestDestination::kScript));
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/image1.png",
                             network::mojom::RequestDestination::kImage));
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/image2.png",
                             network::mojom::RequestDestination::kImage));
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/style2.css",
                             network::mojom::RequestDestination::kStyle));
  resources.push_back(
      CreateResourceLoadInfo("http://static.google.com/style2-no-store.css",
                             network::mojom::RequestDestination::kStyle,
                             /* always_access_network */ true));
  resources.push_back(CreateResourceLoadInfoWithRedirects(
      {"http://reader.google.com/style.css",
       "http://dev.null.google.com/style.css"},
      network::mojom::RequestDestination::kStyle));
  resources.back()->network_info->always_access_network = true;

  auto page_summary = CreatePageRequestSummary(
      "http://www.google.com", "http://www.google.com", resources);

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(predictor_);
  EXPECT_CALL(mock_observer, OnNavigationLearned(page_summary));

  predictor_->RecordPageRequestSummary(
      std::make_unique<PageRequestSummary>(page_summary));
  profile_->BlockUntilHistoryProcessesPendingRequests();

  OriginData origin_data = CreateOriginData("www.google.com");
  InitializeOriginStat(origin_data.add_origins(), "http://www.google.com/", 1,
                       0, 0, 1., false, true);
  InitializeOriginStat(origin_data.add_origins(), "http://google.com/", 1, 0, 0,
                       2., false, true);
  InitializeOriginStat(origin_data.add_origins(), "http://static.google.com/",
                       1, 0, 0, 3., true, true);
  InitializeOriginStat(origin_data.add_origins(), "http://reader.google.com/",
                       1, 0, 0, 4., false, true);
  InitializeOriginStat(origin_data.add_origins(), "http://dev.null.google.com/",
                       1, 0, 0, 5., true, true);
  EXPECT_EQ(mock_tables_->origin_table_.data_,
            OriginDataMap({{origin_data.host(), origin_data}}));

  RedirectData host_redirect_data = CreateRedirectData("www.google.com");
  InitializeRedirectStat(host_redirect_data.add_redirect_endpoints(),
                         GURL("http://www.google.com"), 1, 0, 0);
  EXPECT_EQ(mock_tables_->host_redirect_table_.data_,
            RedirectDataMap(
                {{host_redirect_data.primary_key(), host_redirect_data}}));
}

// Tests that navigation is recorded correctly for URL already present in
// the database cache.
TEST_F(ResourcePrefetchPredictorTest, NavigationUrlInDB) {
  ResetPredictor();
  InitializePredictor();

  std::vector<blink::mojom::ResourceLoadInfoPtr> resources;
  resources.push_back(CreateResourceLoadInfo("http://www.google.com"));
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/style1.css",
                             network::mojom::RequestDestination::kStyle));
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/script1.js",
                             network::mojom::RequestDestination::kScript));
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/script2.js",
                             network::mojom::RequestDestination::kScript));
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/script1.js",
                             network::mojom::RequestDestination::kScript));
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/image1.png",
                             network::mojom::RequestDestination::kImage));
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/image2.png",
                             network::mojom::RequestDestination::kImage));
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/style2.css",
                             network::mojom::RequestDestination::kStyle));
  resources.push_back(
      CreateResourceLoadInfo("http://static.google.com/style2-no-store.css",
                             network::mojom::RequestDestination::kStyle,
                             /* always_access_network */ true));

  auto page_summary = CreatePageRequestSummary(
      "http://www.google.com", "http://www.google.com", resources);

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(predictor_);
  EXPECT_CALL(mock_observer, OnNavigationLearned(page_summary));

  predictor_->RecordPageRequestSummary(
      std::make_unique<PageRequestSummary>(page_summary));
  profile_->BlockUntilHistoryProcessesPendingRequests();

  RedirectData host_redirect_data = CreateRedirectData("www.google.com");
  InitializeRedirectStat(host_redirect_data.add_redirect_endpoints(),
                         GURL("http://www.google.com"), 1, 0, 0);
  EXPECT_EQ(mock_tables_->host_redirect_table_.data_,
            RedirectDataMap(
                {{host_redirect_data.primary_key(), host_redirect_data}}));

  OriginData origin_data = CreateOriginData("www.google.com");
  InitializeOriginStat(origin_data.add_origins(), "http://www.google.com/", 1.,
                       0, 0, 1., false, true);
  InitializeOriginStat(origin_data.add_origins(), "http://static.google.com/",
                       1, 0, 0, 3., true, true);
  InitializeOriginStat(origin_data.add_origins(), "http://google.com/", 1, 0, 0,
                       2., false, true);
  EXPECT_EQ(mock_tables_->origin_table_.data_,
            OriginDataMap({{origin_data.host(), origin_data}}));
}

// Tests that a URL is deleted before another is added if the cache is full.
TEST_F(ResourcePrefetchPredictorTest, NavigationUrlNotInDBAndDBFull) {
  mock_tables_->origin_table_.data_ = test_origin_data_;

  ResetPredictor();
  InitializePredictor();

  std::vector<blink::mojom::ResourceLoadInfoPtr> resources;
  resources.push_back(CreateResourceLoadInfo("http://www.nike.com"));
  resources.push_back(
      CreateResourceLoadInfo("http://nike.com/style1.css",
                             network::mojom::RequestDestination::kStyle));
  resources.push_back(
      CreateResourceLoadInfo("http://nike.com/image2.png",
                             network::mojom::RequestDestination::kImage));

  auto page_summary = CreatePageRequestSummary(
      "http://www.nike.com", "http://www.nike.com", resources);

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(predictor_);
  EXPECT_CALL(mock_observer, OnNavigationLearned(page_summary));

  predictor_->RecordPageRequestSummary(
      std::make_unique<PageRequestSummary>(page_summary));
  profile_->BlockUntilHistoryProcessesPendingRequests();

  RedirectData host_redirect_data = CreateRedirectData("www.nike.com");
  InitializeRedirectStat(host_redirect_data.add_redirect_endpoints(),
                         GURL("http://www.nike.com"), 1, 0, 0);
  EXPECT_EQ(mock_tables_->host_redirect_table_.data_,
            RedirectDataMap(
                {{host_redirect_data.primary_key(), host_redirect_data}}));

  OriginData origin_data = CreateOriginData("www.nike.com");
  InitializeOriginStat(origin_data.add_origins(), "http://www.nike.com/", 1, 0,
                       0, 1., false, true);
  InitializeOriginStat(origin_data.add_origins(), "http://nike.com/", 1, 0, 0,
                       2., false, true);
  OriginDataMap expected_origin_data = test_origin_data_;
  expected_origin_data.erase("google.com");
  expected_origin_data["www.nike.com"] = origin_data;
  EXPECT_EQ(mock_tables_->origin_table_.data_, expected_origin_data);
}

TEST_F(ResourcePrefetchPredictorTest,
       NavigationManyResourcesWithDifferentOrigins) {
  std::vector<blink::mojom::ResourceLoadInfoPtr> resources;
  resources.push_back(CreateResourceLoadInfo("http://www.google.com"));

  auto gen = [](int i) {
    return base::StringPrintf("http://cdn%d.google.com/script.js", i);
  };
  const int num_resources = predictor_->config_.max_origins_per_entry + 10;
  for (int i = 1; i <= num_resources; ++i) {
    resources.push_back(CreateResourceLoadInfo(
        gen(i), network::mojom::RequestDestination::kScript));
  }

  auto page_summary = CreatePageRequestSummary(
      "http://www.google.com", "http://www.google.com", resources);

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(predictor_);
  EXPECT_CALL(mock_observer, OnNavigationLearned(page_summary));

  predictor_->RecordPageRequestSummary(
      std::make_unique<PageRequestSummary>(page_summary));
  profile_->BlockUntilHistoryProcessesPendingRequests();

  OriginData origin_data = CreateOriginData("www.google.com");
  InitializeOriginStat(origin_data.add_origins(), "http://www.google.com/", 1,
                       0, 0, 1, false, true);
  for (int i = 1;
       i <= static_cast<int>(predictor_->config_.max_origins_per_entry) - 1;
       ++i) {
    InitializeOriginStat(origin_data.add_origins(),
                         GURL(gen(i)).DeprecatedGetOriginAsURL().spec(), 1, 0,
                         0, i + 1, false, true);
  }
  EXPECT_EQ(mock_tables_->origin_table_.data_,
            OriginDataMap({{origin_data.host(), origin_data}}));
}

TEST_F(ResourcePrefetchPredictorTest, RedirectUrlNotInDB) {
  std::vector<blink::mojom::ResourceLoadInfoPtr> resources;
  resources.push_back(CreateResourceLoadInfoWithRedirects(
      {"http://fb.com/google", "https://facebook.com/google"}));
  auto page_summary = CreatePageRequestSummary(
      "https://facebook.com/google", "http://fb.com/google", resources);

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(predictor_);
  EXPECT_CALL(mock_observer, OnNavigationLearned(page_summary));

  predictor_->RecordPageRequestSummary(
      std::make_unique<PageRequestSummary>(page_summary));
  profile_->BlockUntilHistoryProcessesPendingRequests();

  RedirectData host_redirect_data = CreateRedirectData("fb.com");
  InitializeRedirectStat(host_redirect_data.add_redirect_endpoints(),
                         GURL("https://facebook.com"), 1, 0, 0);
  EXPECT_EQ(mock_tables_->host_redirect_table_.data_,
            RedirectDataMap(
                {{host_redirect_data.primary_key(), host_redirect_data}}));
}

// Tests that redirect is recorded correctly for URL already present in
// the database cache.
TEST_F(ResourcePrefetchPredictorTest, RedirectUrlInDB) {
  mock_tables_->host_redirect_table_.data_ = test_host_redirect_data_;

  ResetPredictor();
  InitializePredictor();

  std::vector<blink::mojom::ResourceLoadInfoPtr> resources;
  resources.push_back(CreateResourceLoadInfoWithRedirects(
      {"http://fb.com/google", "https://facebook.com/google"}));
  auto page_summary = CreatePageRequestSummary(
      "https://facebook.com/google", "http://fb.com/google", resources);

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(predictor_);
  EXPECT_CALL(mock_observer, OnNavigationLearned(page_summary));

  predictor_->RecordPageRequestSummary(
      std::make_unique<PageRequestSummary>(page_summary));
  profile_->BlockUntilHistoryProcessesPendingRequests();

  RedirectData host_redirect_data = CreateRedirectData("fb.com");
  InitializeRedirectStat(host_redirect_data.add_redirect_endpoints(),
                         GURL("https://facebook.com"), 1, 0, 0);
  RedirectDataMap expected_host_redirect_data = test_host_redirect_data_;
  expected_host_redirect_data.erase("bbc.com");
  expected_host_redirect_data[host_redirect_data.primary_key()] =
      host_redirect_data;
  EXPECT_EQ(mock_tables_->host_redirect_table_.data_,
            expected_host_redirect_data);
}

// Tests that redirect is recorded correctly for URL already present in
// the database cache. Test with both https and http schemes for the same
// host.
TEST_F(ResourcePrefetchPredictorTest, RedirectUrlInDB_MultipleSchemes) {
  mock_tables_->host_redirect_table_.data_ = test_host_redirect_data_;

  ResetPredictor();
  InitializePredictor();

  {
    std::vector<blink::mojom::ResourceLoadInfoPtr> resources_https;
    resources_https.push_back(CreateResourceLoadInfoWithRedirects(
        {"https://fb.com/google", "https://facebook.com/google"}));
    auto page_summary_https =
        CreatePageRequestSummary("https://facebook.com/google",
                                 "https://fb.com/google", resources_https);

    StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(predictor_);
    EXPECT_CALL(mock_observer, OnNavigationLearned(page_summary_https));

    predictor_->RecordPageRequestSummary(
        std::make_unique<PageRequestSummary>(page_summary_https));
    profile_->BlockUntilHistoryProcessesPendingRequests();

    RedirectData host_redirect_data_https = CreateRedirectData("fb.com");
    InitializeRedirectStat(host_redirect_data_https.add_redirect_endpoints(),
                           GURL("https://facebook.com"), 1, 0, 0);
    RedirectDataMap expected_host_redirect_data_https =
        test_host_redirect_data_;
    expected_host_redirect_data_https.erase("bbc.com");
    expected_host_redirect_data_https[host_redirect_data_https.primary_key()] =
        host_redirect_data_https;
    EXPECT_EQ(mock_tables_->host_redirect_table_.data_,
              expected_host_redirect_data_https);
    EXPECT_EQ(1, mock_tables_->host_redirect_table_
                     .data_[host_redirect_data_https.primary_key()]
                     .redirect_endpoints()
                     .size());
    EXPECT_EQ("https", mock_tables_->host_redirect_table_
                           .data_[host_redirect_data_https.primary_key()]
                           .redirect_endpoints(0)
                           .url_scheme());
    EXPECT_EQ(443, mock_tables_->host_redirect_table_
                       .data_[host_redirect_data_https.primary_key()]
                       .redirect_endpoints(0)
                       .url_port());
  }
  {
    std::vector<blink::mojom::ResourceLoadInfoPtr> resources_http;
    resources_http.push_back(CreateResourceLoadInfoWithRedirects(
        {"http://fb.com/google", "http://facebook.com/google"}));
    auto page_summary_http = CreatePageRequestSummary(
        "http://facebook.com/google", "http://fb.com/google", resources_http);

    StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(predictor_);
    EXPECT_CALL(mock_observer, OnNavigationLearned(page_summary_http));

    predictor_->RecordPageRequestSummary(
        std::make_unique<PageRequestSummary>(page_summary_http));
    profile_->BlockUntilHistoryProcessesPendingRequests();

    RedirectData host_redirect_data_http = CreateRedirectData("fb.com");
    InitializeRedirectStat(host_redirect_data_http.add_redirect_endpoints(),
                           GURL("http://facebook.com"), 1, 0, 0);
    RedirectDataMap expected_host_redirect_data_http = test_host_redirect_data_;
    expected_host_redirect_data_http.erase("bbc.com");
    expected_host_redirect_data_http[host_redirect_data_http.primary_key()] =
        host_redirect_data_http;
    EXPECT_EQ(2, mock_tables_->host_redirect_table_
                     .data_[host_redirect_data_http.primary_key()]
                     .redirect_endpoints()
                     .size());
    EXPECT_EQ("facebook.com", mock_tables_->host_redirect_table_
                                  .data_[host_redirect_data_http.primary_key()]
                                  .redirect_endpoints(1)
                                  .url());
    EXPECT_EQ("http", mock_tables_->host_redirect_table_
                          .data_[host_redirect_data_http.primary_key()]
                          .redirect_endpoints(1)
                          .url_scheme());
    EXPECT_EQ(80, mock_tables_->host_redirect_table_
                      .data_[host_redirect_data_http.primary_key()]
                      .redirect_endpoints(1)
                      .url_port());
  }
}

TEST_F(ResourcePrefetchPredictorTest, DeleteUrls) {
  ResetPredictor(false);
  InitializePredictor();

  // Add some dummy entries to cache.

  RedirectDataMap host_redirects;
  host_redirects.insert(
      {"www.google.com", CreateRedirectData("www.google.com")});
  host_redirects.insert({"www.nike.com", CreateRedirectData("www.nike.com")});
  host_redirects.insert(
      {"www.wikipedia.org", CreateRedirectData("www.wikipedia.org")});
  for (const auto& redirect : host_redirects) {
    predictor_->host_redirect_data_->UpdateData(redirect.first,
                                                redirect.second);
  }

  // TODO(alexilin): Add origin data.

  history::URLRows rows;
  rows.push_back(history::URLRow(GURL("http://www.google.com/page2.html")));
  rows.push_back(history::URLRow(GURL("http://www.apple.com")));
  rows.push_back(history::URLRow(GURL("http://www.nike.com")));

  host_redirects.erase("www.google.com");
  host_redirects.erase("www.nike.com");

  predictor_->DeleteUrls(rows);
  EXPECT_EQ(mock_tables_->host_redirect_table_.data_, host_redirects);

  predictor_->DeleteAllUrls();
  EXPECT_TRUE(mock_tables_->host_redirect_table_.data_.empty());
}

// Tests that DeleteAllUrls deletes all urls even if called before the
// initialization is completed.
TEST_F(ResourcePrefetchPredictorTest, DeleteAllUrlsUninitialized) {
  mock_tables_->host_redirect_table_.data_ = test_host_redirect_data_;
  mock_tables_->origin_table_.data_ = test_origin_data_;
  ResetPredictor();

  CHECK_EQ(predictor_->initialization_state_,
           ResourcePrefetchPredictor::NOT_INITIALIZED);
  EXPECT_FALSE(mock_tables_->origin_table_.data_.empty());

  predictor_->DeleteAllUrls();
  // Caches aren't initialized yet, so data should be deleted only after the
  // initialization.
  EXPECT_FALSE(mock_tables_->origin_table_.data_.empty());

  InitializePredictor();
  CHECK_EQ(predictor_->initialization_state_,
           ResourcePrefetchPredictor::INITIALIZED);
  EXPECT_TRUE(mock_tables_->origin_table_.data_.empty());
}

TEST_F(ResourcePrefetchPredictorTest, GetRedirectOrigin) {
  auto& redirect_data = *predictor_->host_redirect_data_;
  url::Origin bbc_origin = url::Origin::Create(GURL("https://bbc.com/"));
  url::Origin redirect_origin;
  // Returns the initial url if data_map doesn't contain an entry for the url.
  EXPECT_TRUE(predictor_->GetRedirectOrigin(bbc_origin, redirect_data,
                                            &redirect_origin));
  EXPECT_EQ(bbc_origin, redirect_origin);

  url::Origin nyt_origin = url::Origin::Create(GURL("https://nyt.com/"));
  // The data to be requested for the confident endpoint.
  RedirectData nyt = CreateRedirectData(nyt_origin.host(), 1);
  GURL nyt_redirect_url("https://mobile.nytimes.com:8080/");
  url::Origin nyt_redirect_origin = url::Origin::Create(nyt_redirect_url);
  InitializeRedirectStat(nyt.add_redirect_endpoints(), nyt_redirect_url, 10, 0,
                         0);
  redirect_data.UpdateData(nyt.primary_key(), nyt);
  EXPECT_TRUE(predictor_->GetRedirectOrigin(nyt_origin, redirect_data,
                                            &redirect_origin));
  EXPECT_EQ(nyt_redirect_origin, redirect_origin);

  url::Origin facebook_origin = url::Origin::Create(GURL("http://fb.com/"));
  // The data to check negative result due not enough confidence.
  RedirectData facebook = CreateRedirectData(facebook_origin.host(), 3);
  GURL facebook_redirect_url("https://fb.com/");
  InitializeRedirectStat(facebook.add_redirect_endpoints(),
                         facebook_redirect_url, 5, 5, 0);
  redirect_data.UpdateData(facebook.primary_key(), facebook);
  EXPECT_FALSE(predictor_->GetRedirectOrigin(facebook_origin, redirect_data,
                                             &redirect_origin));

  // The data to check negative result due ambiguity.
  url::Origin google_origin = url::Origin::Create(GURL("https://google.com/"));
  RedirectData google = CreateRedirectData(google_origin.host(), 4);
  InitializeRedirectStat(google.add_redirect_endpoints(),
                         GURL("https://google.com"), 10, 0, 0);
  InitializeRedirectStat(google.add_redirect_endpoints(),
                         GURL("https://google.fr"), 10, 1, 0);
  InitializeRedirectStat(google.add_redirect_endpoints(),
                         GURL("https://google.ws"), 20, 20, 0);
  redirect_data.UpdateData(google.primary_key(), google);
  EXPECT_FALSE(predictor_->GetRedirectOrigin(google_origin, redirect_data,
                                             &redirect_origin));

  // Check the case of a redirect with no port or scheme in the database. The
  // redirected origin should default to HTTPS on port 443, if either is
  // missing.

  url::Origin no_port_origin =
      url::Origin::Create(GURL("https://no-port.test/"));
  RedirectData no_port = CreateRedirectData(no_port_origin.host(), 1);
  GURL no_port_redirect_url("http://redirect-destination.no-port.test/");
  url::Origin no_port_redirect_origin =
      url::Origin::Create(GURL("https://redirect-destination.no-port.test/"));
  InitializeRedirectStat(no_port.add_redirect_endpoints(), no_port_redirect_url,
                         10, 0, 0, true /* include_scheme */,
                         false /* include_port */);
  redirect_data.UpdateData(no_port.primary_key(), no_port);
  EXPECT_TRUE(predictor_->GetRedirectOrigin(no_port_origin, redirect_data,
                                            &redirect_origin));
  EXPECT_EQ(no_port_redirect_origin, redirect_origin);

  url::Origin no_scheme_origin =
      url::Origin::Create(GURL("https://no-scheme.test/"));
  RedirectData no_scheme = CreateRedirectData(no_scheme_origin.host(), 1);
  GURL no_scheme_redirect_url("http://redirect-destination.no-scheme.test/");
  url::Origin no_scheme_redirect_origin =
      url::Origin::Create(GURL("https://redirect-destination.no-scheme.test/"));
  InitializeRedirectStat(no_scheme.add_redirect_endpoints(),
                         no_scheme_redirect_url, 10, 0, 0,
                         true /* include_scheme */, false /* include_port */);
  redirect_data.UpdateData(no_scheme.primary_key(), no_scheme);
  EXPECT_TRUE(predictor_->GetRedirectOrigin(no_scheme_origin, redirect_data,
                                            &redirect_origin));
  EXPECT_EQ(no_scheme_redirect_origin, redirect_origin);
}

class ResourcePrefetchPredictorPreconnectToRedirectTargetTest
    : public ResourcePrefetchPredictorTest,
      public ::testing::WithParamInterface<bool> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    ResourcePrefetchPredictorPreconnectToRedirectTargetTest,
    ::testing::Values(false, true));

// google.com redirects to https://www.google.com and stores origin data for
// https://www.google.com. Verifies that predictions for google.com returns the
// origin data stored for https://www.google.com.
TEST_P(ResourcePrefetchPredictorPreconnectToRedirectTargetTest,
       TestPredictPreconnectOrigins) {
  const bool enable_preconnect_to_redirect_target_experiment = GetParam();

  base::test::ScopedFeatureList scoped_feature_list;

  if (enable_preconnect_to_redirect_target_experiment) {
    scoped_feature_list.InitWithFeatures(
        {features::kLoadingOnlyLearnHighPriorityResources,
         features::kLoadingPreconnectToRedirectTarget},
        {});
  } else {
    scoped_feature_list.InitWithFeatures(
        {features::kLoadingOnlyLearnHighPriorityResources},
        {features::kLoadingPreconnectToRedirectTarget});
  }

  const GURL main_frame_url("http://google.com/?query=cats");
  const net::SchemefulSite site = net::SchemefulSite(main_frame_url);
  auto network_anonymization_key =
      net::NetworkAnonymizationKey::CreateSameSite(site);
  const url::Origin www_google_origin =
      url::Origin::Create(GURL("https://www.google.com"));
  const net::SchemefulSite www_google_site =
      net::SchemefulSite(www_google_origin);
  auto www_google_network_anonymization_key =
      net::NetworkAnonymizationKey::CreateSameSite(www_google_site);
  auto prediction = std::make_unique<PreconnectPrediction>();
  // No prefetch data.
  EXPECT_FALSE(predictor_->IsUrlPreconnectable(main_frame_url));
  EXPECT_FALSE(
      predictor_->PredictPreconnectOrigins(main_frame_url, prediction.get()));

  const char* cdn_origin = "https://cdn%d.google.com";
  auto gen_origin = [cdn_origin](int n) {
    return base::StringPrintf(cdn_origin, n);
  };

  // Add origins associated with the main frame host.
  OriginData google = CreateOriginData("google.com");
  InitializeOriginStat(google.add_origins(), gen_origin(1), 10, 0, 0, 1.0, true,
                       true);  // High confidence - preconnect.
  InitializeOriginStat(google.add_origins(), gen_origin(2), 10, 5, 0, 2.0, true,
                       true);  // Medium confidence - preresolve.
  InitializeOriginStat(google.add_origins(), gen_origin(3), 1, 10, 10, 3.0,
                       true, true);  // Low confidence - ignore.
  predictor_->origin_data_->UpdateData(google.host(), google);

  prediction = std::make_unique<PreconnectPrediction>();
  EXPECT_TRUE(predictor_->IsUrlPreconnectable(main_frame_url));
  EXPECT_TRUE(
      predictor_->PredictPreconnectOrigins(main_frame_url, prediction.get()));
  EXPECT_EQ(*prediction, CreatePreconnectPrediction(
                             "google.com", false,
                             {{url::Origin::Create(GURL(gen_origin(1))), 1,
                               network_anonymization_key},
                              {url::Origin::Create(GURL(gen_origin(2))), 0,
                               network_anonymization_key}}));

  // Add a redirect.
  RedirectData redirect = CreateRedirectData("google.com", 3);
  InitializeRedirectStat(redirect.add_redirect_endpoints(),
                         GURL("https://www.google.com"), 10, 0, 0);
  predictor_->host_redirect_data_->UpdateData(redirect.primary_key(), redirect);

  // Prediction should succeed: The redirect endpoint should be associated
  // with |main_frame_url|.
  prediction = std::make_unique<PreconnectPrediction>();
  EXPECT_EQ(enable_preconnect_to_redirect_target_experiment,
            predictor_->IsUrlPreconnectable(main_frame_url));
  EXPECT_EQ(
      enable_preconnect_to_redirect_target_experiment,
      predictor_->PredictPreconnectOrigins(main_frame_url, prediction.get()));
  auto expected_prediction_1 = CreatePreconnectPrediction(
      "google.com", false,
      {{url::Origin::Create(GURL("https://www.google.com/")), 1,
        www_google_network_anonymization_key}});
  if (enable_preconnect_to_redirect_target_experiment) {
    EXPECT_EQ(expected_prediction_1, *prediction);
  } else {
    EXPECT_TRUE(prediction->requests.empty());
  }

  // Add a resource associated with the redirect endpoint
  // (https://www.google.com).
  OriginData www_google = CreateOriginData("www.google.com", 4);
  InitializeOriginStat(www_google.add_origins(), gen_origin(4), 10, 0, 0, 1.0,
                       true,
                       true);  // High confidence - preconnect.
  predictor_->origin_data_->UpdateData(www_google.host(), www_google);

  prediction = std::make_unique<PreconnectPrediction>();
  EXPECT_TRUE(predictor_->IsUrlPreconnectable(main_frame_url));
  EXPECT_TRUE(
      predictor_->PredictPreconnectOrigins(main_frame_url, prediction.get()));

  auto expected_prediction_2 =
      CreatePreconnectPrediction("www.google.com", true,
                                 {{url::Origin::Create(GURL(gen_origin(4))), 1,
                                   www_google_network_anonymization_key}});
  if (enable_preconnect_to_redirect_target_experiment) {
    // Getting the prediction for google.com should include the redirect
    // target as well. The redirect target should be present in the front.
    expected_prediction_2.requests.emplace(
        expected_prediction_2.requests.begin(),
        url::Origin::Create(GURL("https://www.google.com")), 1,
        www_google_network_anonymization_key);
  }
  EXPECT_EQ(expected_prediction_2, *prediction);
}

// Redirects from google.com to google-redirected-to.com. Origin data is added
// for www.google.com.
TEST_F(ResourcePrefetchPredictorTest,
       TestPredictPreconnectOrigins_RedirectsToNewOrigin) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kLoadingOnlyLearnHighPriorityResources,
       features::kLoadingPreconnectToRedirectTarget},
      {});

  const GURL main_frame_url("http://google.com/?query=cats");
  const net::SchemefulSite site = net::SchemefulSite(main_frame_url);
  auto network_anonymization_key =
      net::NetworkAnonymizationKey::CreateSameSite(site);
  auto prediction = std::make_unique<PreconnectPrediction>();
  // No prefetch data.
  EXPECT_FALSE(predictor_->IsUrlPreconnectable(main_frame_url));
  EXPECT_FALSE(
      predictor_->PredictPreconnectOrigins(main_frame_url, prediction.get()));

  const char* cdn_origin = "https://cdn%d.google.com";
  auto gen_origin = [cdn_origin](int n) {
    return base::StringPrintf(cdn_origin, n);
  };

  // Add origins associated with the main frame host.
  OriginData google = CreateOriginData("google.com");
  InitializeOriginStat(google.add_origins(), gen_origin(1), 10, 0, 0, 1.0, true,
                       true);  // High confidence - preconnect.
  InitializeOriginStat(google.add_origins(), gen_origin(2), 10, 5, 0, 2.0, true,
                       true);  // Medium confidence - preresolve.
  InitializeOriginStat(google.add_origins(), gen_origin(3), 1, 10, 10, 3.0,
                       true, true);  // Low confidence - ignore.
  predictor_->origin_data_->UpdateData(google.host(), google);

  prediction = std::make_unique<PreconnectPrediction>();
  EXPECT_TRUE(predictor_->IsUrlPreconnectable(main_frame_url));
  EXPECT_TRUE(
      predictor_->PredictPreconnectOrigins(main_frame_url, prediction.get()));
  EXPECT_EQ(*prediction, CreatePreconnectPrediction(
                             "google.com", false,
                             {{url::Origin::Create(GURL(gen_origin(1))), 1,
                               network_anonymization_key},
                              {url::Origin::Create(GURL(gen_origin(2))), 0,
                               network_anonymization_key}}));

  // Add a redirect.
  RedirectData redirect = CreateRedirectData("google.com", 3);
  InitializeRedirectStat(redirect.add_redirect_endpoints(),
                         GURL("https://www.google-redirected-to.com"), 10, 0,
                         0);
  predictor_->host_redirect_data_->UpdateData(redirect.primary_key(), redirect);

  // Prediction should succeed: The redirect endpoint should be associated with
  // |main_frame_url|.
  prediction = std::make_unique<PreconnectPrediction>();
  EXPECT_TRUE(predictor_->IsUrlPreconnectable(main_frame_url));
  EXPECT_TRUE(
      predictor_->PredictPreconnectOrigins(main_frame_url, prediction.get()));

  const auto www_google_redirected_to_network_anonymization_key =
      net::NetworkAnonymizationKey::CreateSameSite(
          net::SchemefulSite(GURL("https://www.google-redirected-to.com")));

  const auto expected_prediction = CreatePreconnectPrediction(
      "google.com", false,
      {{url::Origin::Create(GURL("https://www.google-redirected-to.com/")), 1,
        www_google_redirected_to_network_anonymization_key}});
  EXPECT_EQ(expected_prediction, *prediction);

  // Add a resource associated with the redirect endpoint.
  OriginData www_google = CreateOriginData("www.google.com", 4);
  InitializeOriginStat(www_google.add_origins(), gen_origin(4), 10, 0, 0, 1.0,
                       true,
                       true);  // High confidence - preconnect.
  predictor_->origin_data_->UpdateData(www_google.host(), www_google);

  // Add a resource associated with the redirect endpoint.
  OriginData www_google_redirected_to =
      CreateOriginData("www.google-redirected-to.com", 4);
  InitializeOriginStat(www_google_redirected_to.add_origins(), gen_origin(4),
                       10, 0, 0, 1.0, true,
                       true);  // High confidence - preconnect.
  predictor_->origin_data_->UpdateData(www_google_redirected_to.host(),
                                       www_google_redirected_to);

  prediction = std::make_unique<PreconnectPrediction>();
  EXPECT_TRUE(predictor_->IsUrlPreconnectable(main_frame_url));
  EXPECT_TRUE(
      predictor_->PredictPreconnectOrigins(main_frame_url, prediction.get()));
  const auto expected_prediction_redirected_to = CreatePreconnectPrediction(
      "www.google-redirected-to.com", true,
      {
          {url::Origin::Create(GURL("https://www.google-redirected-to.com")), 1,
           www_google_redirected_to_network_anonymization_key},
          {url::Origin::Create(GURL(gen_origin(4))), 1,
           www_google_redirected_to_network_anonymization_key},
      });
  EXPECT_EQ(expected_prediction_redirected_to, *prediction);
}

}  // namespace predictors
