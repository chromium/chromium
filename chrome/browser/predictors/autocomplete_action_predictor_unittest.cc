// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/autocomplete_action_predictor.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/uuid.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/in_memory_database.h"
#include "components/history/core/browser/url_database.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_field_trial.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using content::WebContents;
using predictors::AutocompleteActionPredictor;

using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;
using ukm::builders::Preloading_Prediction;

namespace {

struct TestUrlInfo {
  GURL url;
  std::u16string title;
  int days_from_now;
  std::u16string user_text;
  int number_of_hits;
  int number_of_misses;
  AutocompleteActionPredictor::Action expected_action;
};

AutocompleteActionPredictor::Action ExpectedActionBasedOnConfidenceOnly(
    int number_of_hits,
    int number_of_misses) {
  int total = number_of_hits + number_of_misses;
  EXPECT_GT(total, 0);
  double confidence = number_of_hits / (double)total;
  return AutocompleteActionPredictor::DecideActionByConfidence(confidence);
}

const std::vector<TestUrlInfo>& TestUrlDb() {
  static base::NoDestructor<std::vector<TestUrlInfo>> db{
      {{GURL("http://www.testsite.com/a.html"), u"Test - site - just a test", 1,
        u"j", 5, 0, ExpectedActionBasedOnConfidenceOnly(5, 0)},
       {GURL("http://www.testsite.com/b.html"), u"Test - site - just a test", 1,
        u"ju", 3, 0, ExpectedActionBasedOnConfidenceOnly(3, 0)},
       {GURL("http://www.testsite.com/c.html"), u"Test - site - just a test", 5,
        u"just", 3, 1, ExpectedActionBasedOnConfidenceOnly(3, 1)},
       {GURL("http://www.testsite.com/d.html"), u"Test - site - just a test", 5,
        u"just", 3, 0, ExpectedActionBasedOnConfidenceOnly(3, 0)},
       {GURL("http://www.testsite.com/e.html"), u"Test - site - just a test", 8,
        u"just", 3, 1, ExpectedActionBasedOnConfidenceOnly(3, 1)},
       {GURL("http://www.testsite.com/f.html"), u"Test - site - just a test", 8,
        u"just", 3, 0, ExpectedActionBasedOnConfidenceOnly(3, 0)},
       {GURL("http://www.testsite.com/g.html"), u"Test - site - just a test",
        12, std::u16string(), 5, 0, AutocompleteActionPredictor::ACTION_NONE},
       {GURL("http://www.testsite.com/h.html"), u"Test - site - just a test",
        21, u"just a test", 2, 0, AutocompleteActionPredictor::ACTION_NONE},
       {GURL("http://www.testsite.com/i.html"), u"Test - site - just a test",
        28, u"just a test", 2, 0, AutocompleteActionPredictor::ACTION_NONE}}};
  return *db;
}

// List of urls sorted by the confidence score in ascending order.
const std::vector<TestUrlInfo>& TestUrlConfidenceDb() {
  static base::NoDestructor<std::vector<TestUrlInfo>> db{{
      {GURL("http://www.testsite.com/g.html"), u"Test", 1, u"test", 0, 2,
       AutocompleteActionPredictor::ACTION_NONE},
      {GURL("http://www.testsite.com/f.html"), u"Test", 1, u"test", 1, 2,
       AutocompleteActionPredictor::ACTION_NONE},
      {GURL("http://www.testsite.com/e.html"), u"Test", 1, u"test", 2, 2,
       AutocompleteActionPredictor::ACTION_NONE},
      {GURL("http://www.testsite.com/d.html"), u"Test", 1, u"test", 3, 3,
       AutocompleteActionPredictor::ACTION_NONE},
      {GURL("http://www.testsite.com/c.html"), u"Test", 1, u"test", 3, 2,
       AutocompleteActionPredictor::ACTION_NONE},
      {GURL("http://www.testsite.com/b.html"), u"Test", 1, u"test", 3, 0,
       AutocompleteActionPredictor::ACTION_NONE},
      {GURL("http://www.testsite.com/a.html"), u"Test", 1, u"test", 5, 0,
       AutocompleteActionPredictor::ACTION_NONE},
  }};
  return *db;
}

}  // end namespace

namespace predictors {

class AutocompleteActionPredictorTest : public testing::Test {
 public:
  AutocompleteActionPredictorTest() : predictor_(nullptr) {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();

    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    ukm_entry_builder_ =
        std::make_unique<content::test::PreloadingPredictionUkmEntryBuilder>(
            chrome_preloading_predictor::kOmniboxDirectURLInput);
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    test_timer_ = std::make_unique<base::ScopedMockElapsedTimersForTest>();

    predictor_ = std::make_unique<AutocompleteActionPredictor>(profile_.get());
    profile_->BlockUntilHistoryProcessesPendingRequests();
    content::RunAllTasksUntilIdle();

    CHECK(predictor_->initialized_);
    CHECK(db_cache()->empty());
    CHECK(db_id_cache()->empty());
  }

  ~AutocompleteActionPredictorTest() override {
    web_contents_.reset();
    // Wait for all pending tasks on the DB sequence.
    content::RunAllTasksUntilIdle();
    // Since we instantiated the predictor instead of going through a factory
    // and dependencies, no one else is going to call Shutdown(), which is
    // supposed to be called as part of being a KeyedService. The behavior of
    // this method is not explicitly verified.
    predictor_->Shutdown();
  }

 protected:
  typedef AutocompleteActionPredictor::DBCacheKey DBCacheKey;
  typedef AutocompleteActionPredictor::DBCacheValue DBCacheValue;
  typedef AutocompleteActionPredictor::DBCacheMap DBCacheMap;
  typedef AutocompleteActionPredictor::DBIdCacheMap DBIdCacheMap;

  history::URLID AddRowToHistory(const TestUrlInfo& test_row) {
    history::HistoryService* history = HistoryServiceFactory::GetForProfile(
        profile_.get(), ServiceAccessType::EXPLICIT_ACCESS);
    CHECK(history);
    history::URLDatabase* url_db = history->InMemoryDatabase();
    CHECK(url_db);

    const base::Time visit_time =
        base::Time::Now() - base::Days(test_row.days_from_now);

    history::URLRow row(test_row.url);
    row.set_title(test_row.title);
    row.set_last_visit(visit_time);

    return url_db->AddURL(row);
  }

  AutocompleteActionPredictorTable::Row CreateRowFromTestUrlInfo(
      const TestUrlInfo& test_row) const {
    AutocompleteActionPredictorTable::Row row;
    row.id = base::Uuid::GenerateRandomV4().AsLowercaseString();
    row.user_text = test_row.user_text;
    row.url = test_row.url;
    row.number_of_hits = test_row.number_of_hits;
    row.number_of_misses = test_row.number_of_misses;
    return row;
  }

  void AddAllRows() {
    for (size_t i = 0; i < std::size(TestUrlDb()); ++i)
      AddRow(TestUrlDb()[i]);
  }

  std::string AddRow(const TestUrlInfo& test_row) {
    AutocompleteActionPredictorTable::Row row =
        CreateRowFromTestUrlInfo(test_row);
    predictor_->AddAndUpdateRows(
        AutocompleteActionPredictorTable::Rows(1, row),
        AutocompleteActionPredictorTable::Rows());

    return row.id;
  }

  WebContents* web_contents() { return web_contents_.get(); }

  void UpdateRow(const AutocompleteActionPredictorTable::Row& row) {
    AutocompleteActionPredictor::DBCacheKey key = { row.user_text, row.url };
    ASSERT_TRUE(db_cache()->find(key) != db_cache()->end());
    predictor_->AddAndUpdateRows(
        AutocompleteActionPredictorTable::Rows(),
        AutocompleteActionPredictorTable::Rows(1, row));
  }

  void OnURLsDeletedTest(bool expired) {
    ASSERT_NO_FATAL_FAILURE(AddAllRows());

    EXPECT_EQ(std::size(TestUrlDb()), db_cache()->size());
    EXPECT_EQ(std::size(TestUrlDb()), db_id_cache()->size());

    std::vector<size_t> expected;
    history::URLRows rows;
    for (size_t i = 0; i < std::size(TestUrlDb()); ++i) {
      bool expect_deleted = false;

      if (i < 2) {
        rows.push_back(history::URLRow(TestUrlDb()[i].url));
        expect_deleted = true;
      }

      if (!expired &&
          TestUrlDb()[i].days_from_now > maximum_days_to_keep_entry()) {
        expect_deleted = true;
      }

      if (i != 3 && i != 4)
        ASSERT_TRUE(AddRowToHistory(TestUrlDb()[i]));
      else if (!expired)
        expect_deleted = true;

      if (!expect_deleted)
        expected.push_back(i);
    }

    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            profile_.get(), ServiceAccessType::EXPLICIT_ACCESS);
    ASSERT_TRUE(history_service);

    predictor_->OnHistoryDeletions(
        history_service,
        history::DeletionInfo(history::DeletionTimeRange::Invalid(), expired,
                              rows, std::set<GURL>(), std::nullopt));

    EXPECT_EQ(expected.size(), db_cache()->size());
    EXPECT_EQ(expected.size(), db_id_cache()->size());

    for (size_t i = 0; i < std::size(TestUrlDb()); ++i) {
      DBCacheKey key = {TestUrlDb()[i].user_text, TestUrlDb()[i].url};

      bool deleted = !base::Contains(expected, i);
      EXPECT_EQ(deleted, db_cache()->find(key) == db_cache()->end());
      EXPECT_EQ(deleted, db_id_cache()->find(key) == db_id_cache()->end());
    }
  }

  void DeleteAllRows() {
    predictor_->DeleteAllRows();
  }

  void DeleteRowsFromCaches(
      const history::URLRows& rows,
      std::vector<AutocompleteActionPredictorTable::Row::Id>* id_list) {
    predictor_->DeleteRowsFromCaches(rows, id_list);
  }

  void DeleteOldIdsFromCaches(
      std::vector<AutocompleteActionPredictorTable::Row::Id>* id_list) {
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            profile_.get(), ServiceAccessType::EXPLICIT_ACCESS);
    ASSERT_TRUE(history_service);

    history::URLDatabase* url_db = history_service->InMemoryDatabase();
    ASSERT_TRUE(url_db);

    predictor_->DeleteOldIdsFromCaches(url_db, id_list);
  }

  void DeleteLowestConfidenceRowsFromCaches(
      size_t count,
      std::vector<AutocompleteActionPredictorTable::Row::Id>* id_list) {
    predictor_->DeleteLowestConfidenceRowsFromCaches(count, id_list);
  }

  AutocompleteActionPredictor* predictor() { return predictor_.get(); }

  DBCacheMap* db_cache() { return &predictor_->db_cache_; }
  DBIdCacheMap* db_id_cache() { return &predictor_->db_id_cache_; }
  std::vector<AutocompleteActionPredictor::TransitionalMatch>*
  transitional_matches() {
    return &predictor_->transitional_matches_;
  }

  static int maximum_days_to_keep_entry() {
    return AutocompleteActionPredictor::kMaximumDaysToKeepEntry;
  }

  static size_t minimum_user_text_length() {
    return AutocompleteActionPredictor::kMinimumUserTextLength;
  }

  static size_t maximum_string_length() {
    return AutocompleteActionPredictor::kMaximumStringLength;
  }

  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

  const content::test::PreloadingPredictionUkmEntryBuilder&
  ukm_entry_builder() {
    return *ukm_entry_builder_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<AutocompleteActionPredictor> predictor_;
  std::unique_ptr<content::test::PreloadingPredictionUkmEntryBuilder>
      ukm_entry_builder_;
  std::unique_ptr<WebContents> web_contents_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  std::unique_ptr<base::ScopedMockElapsedTimersForTest> test_timer_;
};


TEST_F(AutocompleteActionPredictorTest, AddRow) {
  // Add a test entry to the predictor.
  std::string guid = AddRow(TestUrlDb()[0]);

  // Get the data back out of the cache.
  const DBCacheKey key = {TestUrlDb()[0].user_text, TestUrlDb()[0].url};
  DBCacheMap::const_iterator it = db_cache()->find(key);
  EXPECT_TRUE(it != db_cache()->end());

  const DBCacheValue value = {TestUrlDb()[0].number_of_hits,
                              TestUrlDb()[0].number_of_misses};
  EXPECT_EQ(value.number_of_hits, it->second.number_of_hits);
  EXPECT_EQ(value.number_of_misses, it->second.number_of_misses);

  DBIdCacheMap::const_iterator id_it = db_id_cache()->find(key);
  EXPECT_TRUE(id_it != db_id_cache()->end());
  EXPECT_EQ(guid, id_it->second);
}

TEST_F(AutocompleteActionPredictorTest, UpdateRow) {
  ASSERT_NO_FATAL_FAILURE(AddAllRows());

  EXPECT_EQ(std::size(TestUrlDb()), db_cache()->size());
  EXPECT_EQ(std::size(TestUrlDb()), db_id_cache()->size());

  // Get the data back out of the cache.
  const DBCacheKey key = {TestUrlDb()[0].user_text, TestUrlDb()[0].url};
  DBCacheMap::const_iterator it = db_cache()->find(key);
  EXPECT_TRUE(it != db_cache()->end());

  DBIdCacheMap::const_iterator id_it = db_id_cache()->find(key);
  EXPECT_TRUE(id_it != db_id_cache()->end());

  AutocompleteActionPredictorTable::Row update_row;
  update_row.id = id_it->second;
  update_row.user_text = key.user_text;
  update_row.url = key.url;
  update_row.number_of_hits = it->second.number_of_hits + 1;
  update_row.number_of_misses = it->second.number_of_misses + 2;

  UpdateRow(update_row);

  // Get the updated version.
  DBCacheMap::const_iterator update_it = db_cache()->find(key);
  EXPECT_TRUE(update_it != db_cache()->end());

  EXPECT_EQ(update_row.number_of_hits, update_it->second.number_of_hits);
  EXPECT_EQ(update_row.number_of_misses, update_it->second.number_of_misses);

  DBIdCacheMap::const_iterator update_id_it = db_id_cache()->find(key);
  EXPECT_TRUE(update_id_it != db_id_cache()->end());

  EXPECT_EQ(id_it->second, update_id_it->second);
}

TEST_F(AutocompleteActionPredictorTest, DeleteAllRows) {
  ASSERT_NO_FATAL_FAILURE(AddAllRows());

  EXPECT_EQ(std::size(TestUrlDb()), db_cache()->size());
  EXPECT_EQ(std::size(TestUrlDb()), db_id_cache()->size());

  DeleteAllRows();

  EXPECT_TRUE(db_cache()->empty());
  EXPECT_TRUE(db_id_cache()->empty());
}

TEST_F(AutocompleteActionPredictorTest, DeleteRowsFromCaches) {
  std::vector<AutocompleteActionPredictorTable::Row::Id> all_ids;
  history::URLRows rows;
  for (size_t i = 0; i < std::size(TestUrlDb()); ++i) {
    std::string row_id = AddRow(TestUrlDb()[i]);
    all_ids.push_back(row_id);

    if (i < 2)
      rows.push_back(history::URLRow(TestUrlDb()[i].url));
  }

  EXPECT_EQ(std::size(TestUrlDb()), all_ids.size());
  EXPECT_EQ(std::size(TestUrlDb()), db_cache()->size());
  EXPECT_EQ(std::size(TestUrlDb()), db_id_cache()->size());

  std::vector<AutocompleteActionPredictorTable::Row::Id> id_list;
  DeleteRowsFromCaches(rows, &id_list);

  EXPECT_EQ(std::size(TestUrlDb()) - 2, db_cache()->size());
  EXPECT_EQ(std::size(TestUrlDb()) - 2, db_id_cache()->size());

  for (size_t i = 0; i < std::size(TestUrlDb()); ++i) {
    DBCacheKey key = {TestUrlDb()[i].user_text, TestUrlDb()[i].url};

    bool deleted = (i < 2);
    EXPECT_EQ(deleted, db_cache()->find(key) == db_cache()->end());
    EXPECT_EQ(deleted, db_id_cache()->find(key) == db_id_cache()->end());
    EXPECT_EQ(deleted, base::Contains(id_list, all_ids[i]));
  }
}

TEST_F(AutocompleteActionPredictorTest, DeleteOldIdsFromCaches) {
  std::vector<AutocompleteActionPredictorTable::Row::Id> expected;
  std::vector<AutocompleteActionPredictorTable::Row::Id> all_ids;

  for (size_t i = 0; i < std::size(TestUrlDb()); ++i) {
    std::string row_id = AddRow(TestUrlDb()[i]);
    all_ids.push_back(row_id);

    bool exclude_url =
        base::StartsWith(TestUrlDb()[i].url.path(), "/d",
                         base::CompareCase::SENSITIVE) ||
        (TestUrlDb()[i].days_from_now > maximum_days_to_keep_entry());

    if (exclude_url)
      expected.push_back(row_id);
    else
      ASSERT_TRUE(AddRowToHistory(TestUrlDb()[i]));
  }

  std::vector<AutocompleteActionPredictorTable::Row::Id> id_list;
  DeleteOldIdsFromCaches(&id_list);
  EXPECT_EQ(expected.size(), id_list.size());
  EXPECT_EQ(all_ids.size() - expected.size(), db_cache()->size());
  EXPECT_EQ(all_ids.size() - expected.size(), db_id_cache()->size());

  for (auto it = all_ids.begin(); it != all_ids.end(); ++it) {
    bool in_expected = base::Contains(expected, *it);
    bool in_list = base::Contains(id_list, *it);
    EXPECT_EQ(in_expected, in_list);
  }
}

TEST_F(AutocompleteActionPredictorTest,
       DeleteLowestConfidenceRowsFromCaches_OneByOne) {
  std::vector<AutocompleteActionPredictorTable::Row::Id> test_url_ids;
  for (const auto& info : TestUrlConfidenceDb())
    test_url_ids.push_back(AddRow(info));

  std::vector<AutocompleteActionPredictorTable::Row::Id> id_list;
  std::vector<AutocompleteActionPredictorTable::Row::Id> expected;

  for (size_t i = 0; i < std::size(TestUrlConfidenceDb()); ++i) {
    DeleteLowestConfidenceRowsFromCaches(1, &id_list);
    expected.push_back(test_url_ids[i]);
    EXPECT_THAT(id_list, ::testing::UnorderedElementsAreArray(expected));

    DBCacheKey deleted_key = {TestUrlConfidenceDb()[i].user_text,
                              TestUrlConfidenceDb()[i].url};
    EXPECT_FALSE(base::Contains(*db_cache(), deleted_key));
    EXPECT_FALSE(base::Contains(*db_id_cache(), deleted_key));
  }
}

TEST_F(AutocompleteActionPredictorTest,
       DeleteLowestConfidenceRowsFromCaches_Bulk) {
  std::vector<AutocompleteActionPredictorTable::Row::Id> test_url_ids;
  for (const auto& info : TestUrlConfidenceDb())
    test_url_ids.push_back(AddRow(info));

  std::vector<AutocompleteActionPredictorTable::Row::Id> id_list;
  std::vector<AutocompleteActionPredictorTable::Row::Id> expected;

  const size_t count_to_remove = 4;
  CHECK_LT(count_to_remove, std::size(TestUrlConfidenceDb()));

  for (size_t i = 0; i < count_to_remove; ++i)
    expected.push_back(test_url_ids[i]);

  DeleteLowestConfidenceRowsFromCaches(count_to_remove, &id_list);
  ASSERT_THAT(id_list, ::testing::UnorderedElementsAreArray(expected));

  for (size_t i = 0; i < count_to_remove; ++i) {
    DBCacheKey deleted_key = {TestUrlConfidenceDb()[i].user_text,
                              TestUrlConfidenceDb()[i].url};
    EXPECT_FALSE(base::Contains(*db_cache(), deleted_key));
    EXPECT_FALSE(base::Contains(*db_id_cache(), deleted_key));
  }
}

TEST_F(AutocompleteActionPredictorTest, OnURLsDeletedExpired) {
  OnURLsDeletedTest(true);
}

TEST_F(AutocompleteActionPredictorTest, OnURLsDeletedNonExpired) {
  OnURLsDeletedTest(false);
}

TEST_F(AutocompleteActionPredictorTest, RecommendActionURL) {
  ASSERT_NO_FATAL_FAILURE(AddAllRows());

  // Navigate to kInitial URL.
  GURL kInitialUrl("https://example.com");
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(kInitialUrl);
  content::RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
      ->InitializeRenderFrameIfNeeded();

  AutocompleteMatch match;
  match.type = AutocompleteMatchType::HISTORY_URL;

  for (size_t i = 0; i < std::size(TestUrlDb()); ++i) {
    match.destination_url = GURL(TestUrlDb()[i].url);
    EXPECT_EQ(TestUrlDb()[i].expected_action,
              predictor()->RecommendAction(TestUrlDb()[i].user_text, match,
                                           web_contents()))
        << "Unexpected action for " << match.destination_url;
  }

  // Calculate confidence_interval for the first entry to cross-check with
  // metrics.
  match.destination_url = GURL(TestUrlDb()[0].url);
  double confidence =
      predictor()->CalculateConfidence(TestUrlDb()[0].user_text, match);

  // Set the first url in the database as the destination url to cross-check the
  // metrics for the first Preloading.Prediction UKM.
  GURL kDestinationUrl(TestUrlDb()[0].url);
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(kDestinationUrl);
  ukm::SourceId ukm_source_id =
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  // Check that we have recorded Preloading.Prediction for all entries in the
  // TestUrlDb.
  auto ukm_entries = test_ukm_recorder()->GetEntries(
      Preloading_Prediction::kEntryName,
      content::test::kPreloadingPredictionUkmMetrics);
  EXPECT_EQ(ukm_entries.size(), std::size(TestUrlDb()));
  // Cross-check that we have logged the correct metrics for Prediction,
  // confidence, accurate_prediction on successful activation.
  UkmEntry expected_entry = ukm_entry_builder().BuildEntry(
      ukm_source_id, /*confidence=*/confidence * 100,
      /*accurate_prediction=*/true);
  EXPECT_EQ(ukm_entries[0], expected_entry)
      << content::test::ActualVsExpectedUkmEntryToString(ukm_entries[0],
                                                         expected_entry);
}

TEST_F(AutocompleteActionPredictorTest, RecommendActionSearch) {
  ASSERT_NO_FATAL_FAILURE(AddAllRows());

  AutocompleteMatch match;
  match.type = AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED;

  for (size_t i = 0; i < std::size(TestUrlDb()); ++i) {
    match.destination_url = GURL(TestUrlDb()[i].url);
    AutocompleteActionPredictor::Action expected_action =
        (TestUrlDb()[i].expected_action ==
         AutocompleteActionPredictor::ACTION_PRERENDER)
            ? AutocompleteActionPredictor::ACTION_PRECONNECT
            : TestUrlDb()[i].expected_action;
    EXPECT_EQ(expected_action, predictor()->RecommendAction(
                                   TestUrlDb()[i].user_text, match, nullptr))
        << "Unexpected action for " << match.destination_url;
  }
}

TEST_F(AutocompleteActionPredictorTest,
       RegisterTransitionalMatchesUserTextSizeLimits) {
  auto test = [this](const std::u16string& user_text,
                     bool should_be_registered) {
    predictor()->RegisterTransitionalMatches(user_text, AutocompleteResult());
    bool registered = base::Contains(*transitional_matches(), user_text);
    EXPECT_EQ(registered, should_be_registered);
  };

  std::u16string short_text =
      ASCIIToUTF16(std::string(minimum_user_text_length(), 'g'));
  test(short_text, true);

  std::u16string too_short_text =
      ASCIIToUTF16(std::string(minimum_user_text_length() - 1, 'g'));
  test(too_short_text, false);

  std::u16string long_text =
      ASCIIToUTF16(std::string(maximum_string_length(), 'g'));
  test(long_text, true);

  std::u16string too_long_text =
      ASCIIToUTF16(std::string(maximum_string_length() + 1, 'g'));
  test(too_long_text, false);
}

TEST_F(AutocompleteActionPredictorTest,
       RegisterTransitionalMatchesURLSizeLimits) {
  const auto test_url = [](size_t size) {
    const std::string kPrefix = "http://b/";
    return GURL(kPrefix + std::string(size - kPrefix.size(), 'c'));
  };
  GURL urls[] = {test_url(10), test_url(maximum_string_length()),
                 test_url(maximum_string_length() + 1),
                 test_url(maximum_string_length() * 10)};
  ACMatches matches;
  for (const auto& url : urls) {
    AutocompleteMatch match;
    match.destination_url = url;
    matches.push_back(match);
  }
  AutocompleteResult result;
  result.AppendMatches(matches);
  std::u16string user_text = u"google";
  predictor()->RegisterTransitionalMatches(user_text, result);
  auto it = base::ranges::find(*transitional_matches(), user_text);
  ASSERT_NE(it, transitional_matches()->end());
  EXPECT_THAT(it->urls, ::testing::ElementsAre(urls[0], urls[1]));
}

TEST_F(AutocompleteActionPredictorTest, UpdateDatabaseFromTransitionalMatches) {
  ACMatches matches;
  AutocompleteMatch match;
  GURL clicked_url = GURL("https://foo-clicked.com");
  GURL not_clicked_url = GURL("https://foo-not-clicked.com");
  match.destination_url = clicked_url;
  matches.push_back(match);
  match.destination_url = not_clicked_url;
  matches.push_back(match);
  AutocompleteResult result;
  result.AppendMatches(matches);
  std::u16string user_text = u"foo";
  predictor()->RegisterTransitionalMatches(user_text, result);
  ASSERT_EQ(transitional_matches()->size(), 1ul);

  predictor()->UpdateDatabaseFromTransitionalMatches(clicked_url);
  ASSERT_TRUE(transitional_matches()->empty());

  // Make sure the clicked URL has one hit.
  DBCacheKey key = {user_text, clicked_url};
  DBCacheMap::const_iterator it = db_cache()->find(key);
  EXPECT_TRUE(it != db_cache()->end());
  ASSERT_EQ(it->second.number_of_hits, 1);
  ASSERT_EQ(it->second.number_of_misses, 0);

  // Make sure the not clicked URL has one miss.
  key.url = not_clicked_url;
  it = db_cache()->find(key);
  EXPECT_TRUE(it != db_cache()->end());
  ASSERT_EQ(it->second.number_of_hits, 0);
  ASSERT_EQ(it->second.number_of_misses, 1);
}

}  // namespace predictors
