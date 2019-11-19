// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/autocomplete_action_predictor.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_test_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/in_memory_database.h"
#include "components/history/core/browser/url_database.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using predictors::AutocompleteActionPredictor;

namespace {

struct TestUrlInfo {
  GURL url;
  base::string16 title;
  int days_from_now;
  base::string16 user_text;
  int number_of_hits;
  int number_of_misses;
  AutocompleteActionPredictor::Action expected_action;
} test_url_db[] = {
  { GURL("http://www.testsite.com/a.html"),
    ASCIIToUTF16("Test - site - just a test"), 1,
    ASCIIToUTF16("j"), 5, 0,
    AutocompleteActionPredictor::ACTION_PRERENDER },
  { GURL("http://www.testsite.com/b.html"),
    ASCIIToUTF16("Test - site - just a test"), 1,
    ASCIIToUTF16("ju"), 3, 0,
    AutocompleteActionPredictor::ACTION_PRERENDER },
  { GURL("http://www.testsite.com/c.html"),
    ASCIIToUTF16("Test - site - just a test"), 5,
    ASCIIToUTF16("just"), 3, 1,
    AutocompleteActionPredictor::ACTION_PRECONNECT },
  { GURL("http://www.testsite.com/d.html"),
    ASCIIToUTF16("Test - site - just a test"), 5,
    ASCIIToUTF16("just"), 3, 0,
    AutocompleteActionPredictor::ACTION_PRERENDER },
  { GURL("http://www.testsite.com/e.html"),
    ASCIIToUTF16("Test - site - just a test"), 8,
    ASCIIToUTF16("just"), 3, 1,
    AutocompleteActionPredictor::ACTION_PRECONNECT },
  { GURL("http://www.testsite.com/f.html"),
    ASCIIToUTF16("Test - site - just a test"), 8,
    ASCIIToUTF16("just"), 3, 0,
    AutocompleteActionPredictor::ACTION_PRERENDER },
  { GURL("http://www.testsite.com/g.html"),
    ASCIIToUTF16("Test - site - just a test"), 12,
    base::string16(), 5, 0,
    AutocompleteActionPredictor::ACTION_NONE },
  { GURL("http://www.testsite.com/h.html"),
    ASCIIToUTF16("Test - site - just a test"), 21,
    ASCIIToUTF16("just a test"), 2, 0,
    AutocompleteActionPredictor::ACTION_NONE },
  { GURL("http://www.testsite.com/i.html"),
    ASCIIToUTF16("Test - site - just a test"), 28,
    ASCIIToUTF16("just a test"), 2, 0,
    AutocompleteActionPredictor::ACTION_NONE }
};

// List of urls sorted by the confidence score in ascending order.
TestUrlInfo test_url_confidence_db[] = {
    {GURL("http://www.testsite.com/g.html"), ASCIIToUTF16("Test"), 1,
     ASCIIToUTF16("test"), 0, 2, AutocompleteActionPredictor::ACTION_NONE},
    {GURL("http://www.testsite.com/f.html"), ASCIIToUTF16("Test"), 1,
     ASCIIToUTF16("test"), 1, 2, AutocompleteActionPredictor::ACTION_NONE},
    {GURL("http://www.testsite.com/e.html"), ASCIIToUTF16("Test"), 1,
     ASCIIToUTF16("test"), 2, 2, AutocompleteActionPredictor::ACTION_NONE},
    {GURL("http://www.testsite.com/d.html"), ASCIIToUTF16("Test"), 1,
     ASCIIToUTF16("test"), 3, 3, AutocompleteActionPredictor::ACTION_NONE},
    {GURL("http://www.testsite.com/c.html"), ASCIIToUTF16("Test"), 1,
     ASCIIToUTF16("test"), 3, 2, AutocompleteActionPredictor::ACTION_NONE},
    {GURL("http://www.testsite.com/b.html"), ASCIIToUTF16("Test"), 1,
     ASCIIToUTF16("test"), 3, 0, AutocompleteActionPredictor::ACTION_NONE},
    {GURL("http://www.testsite.com/a.html"), ASCIIToUTF16("Test"), 1,
     ASCIIToUTF16("test"), 5, 0, AutocompleteActionPredictor::ACTION_NONE},
};

GURL GenerateTestURL(size_t size) {
  std::string prefix = "http://b/";
  // Cannot generate an URL shorter than |prefix|.
  DCHECK_GE(size, prefix.size());
  size_t suffix_len = size - prefix.size();
  std::string suffix(suffix_len, 'c');
  GURL url(prefix + suffix);
  DCHECK_EQ(url.spec().size(), size);
  return url;
}

}  // end namespace

namespace predictors {

class AutocompleteActionPredictorTest : public testing::Test {
 public:
  AutocompleteActionPredictorTest()
      : profile_(std::make_unique<TestingProfile>()), predictor_(nullptr) {
    CHECK(profile_->CreateHistoryService(true, false));
    predictor_ = std::make_unique<AutocompleteActionPredictor>(profile_.get());
    predictor_->CreateLocalCachesFromDatabase();
    profile_->BlockUntilHistoryProcessesPendingRequests();
    content::RunAllTasksUntilIdle();

    CHECK(predictor_->initialized_);
    CHECK(db_cache()->empty());
    CHECK(db_id_cache()->empty());
  }

  ~AutocompleteActionPredictorTest() override {
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
        base::Time::Now() - base::TimeDelta::FromDays(
            test_row.days_from_now);

    history::URLRow row(test_row.url);
    row.set_title(test_row.title);
    row.set_last_visit(visit_time);

    return url_db->AddURL(row);
  }

  AutocompleteActionPredictorTable::Row CreateRowFromTestUrlInfo(
      const TestUrlInfo& test_row) const {
    AutocompleteActionPredictorTable::Row row;
    row.id = base::GenerateGUID();
    row.user_text = test_row.user_text;
    row.url = test_row.url;
    row.number_of_hits = test_row.number_of_hits;
    row.number_of_misses = test_row.number_of_misses;
    return row;
  }

  void AddAllRows() {
    for (size_t i = 0; i < base::size(test_url_db); ++i)
      AddRow(test_url_db[i]);
  }

  std::string AddRow(const TestUrlInfo& test_row) {
    AutocompleteActionPredictorTable::Row row =
        CreateRowFromTestUrlInfo(test_row);
    predictor_->AddAndUpdateRows(
        AutocompleteActionPredictorTable::Rows(1, row),
        AutocompleteActionPredictorTable::Rows());

    return row.id;
  }

  void UpdateRow(const AutocompleteActionPredictorTable::Row& row) {
    AutocompleteActionPredictor::DBCacheKey key = { row.user_text, row.url };
    ASSERT_TRUE(db_cache()->find(key) != db_cache()->end());
    predictor_->AddAndUpdateRows(
        AutocompleteActionPredictorTable::Rows(),
        AutocompleteActionPredictorTable::Rows(1, row));
  }

  void OnURLsDeletedTest(bool expired) {
    ASSERT_NO_FATAL_FAILURE(AddAllRows());

    EXPECT_EQ(base::size(test_url_db), db_cache()->size());
    EXPECT_EQ(base::size(test_url_db), db_id_cache()->size());

    std::vector<size_t> expected;
    history::URLRows rows;
    for (size_t i = 0; i < base::size(test_url_db); ++i) {
      bool expect_deleted = false;

      if (i < 2) {
        rows.push_back(history::URLRow(test_url_db[i].url));
        expect_deleted = true;
      }

      if (!expired &&
          test_url_db[i].days_from_now > maximum_days_to_keep_entry()) {
        expect_deleted = true;
      }

      if (i != 3 && i != 4)
        ASSERT_TRUE(AddRowToHistory(test_url_db[i]));
      else if (!expired)
        expect_deleted = true;

      if (!expect_deleted)
        expected.push_back(i);
    }

    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            profile_.get(), ServiceAccessType::EXPLICIT_ACCESS);
    ASSERT_TRUE(history_service);

    predictor_->OnURLsDeleted(
        history_service,
        history::DeletionInfo(history::DeletionTimeRange::Invalid(), expired,
                              rows, std::set<GURL>(), base::nullopt));

    EXPECT_EQ(expected.size(), db_cache()->size());
    EXPECT_EQ(expected.size(), db_id_cache()->size());

    for (size_t i = 0; i < base::size(test_url_db); ++i) {
      DBCacheKey key = {test_url_db[i].user_text, test_url_db[i].url};

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

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<AutocompleteActionPredictor> predictor_;
};


TEST_F(AutocompleteActionPredictorTest, AddRow) {
  // Add a test entry to the predictor.
  std::string guid = AddRow(test_url_db[0]);

  // Get the data back out of the cache.
  const DBCacheKey key = { test_url_db[0].user_text, test_url_db[0].url };
  DBCacheMap::const_iterator it = db_cache()->find(key);
  EXPECT_TRUE(it != db_cache()->end());

  const DBCacheValue value = { test_url_db[0].number_of_hits,
                               test_url_db[0].number_of_misses };
  EXPECT_EQ(value.number_of_hits, it->second.number_of_hits);
  EXPECT_EQ(value.number_of_misses, it->second.number_of_misses);

  DBIdCacheMap::const_iterator id_it = db_id_cache()->find(key);
  EXPECT_TRUE(id_it != db_id_cache()->end());
  EXPECT_EQ(guid, id_it->second);
}

TEST_F(AutocompleteActionPredictorTest, UpdateRow) {
  ASSERT_NO_FATAL_FAILURE(AddAllRows());

  EXPECT_EQ(base::size(test_url_db), db_cache()->size());
  EXPECT_EQ(base::size(test_url_db), db_id_cache()->size());

  // Get the data back out of the cache.
  const DBCacheKey key = { test_url_db[0].user_text, test_url_db[0].url };
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

  EXPECT_EQ(base::size(test_url_db), db_cache()->size());
  EXPECT_EQ(base::size(test_url_db), db_id_cache()->size());

  DeleteAllRows();

  EXPECT_TRUE(db_cache()->empty());
  EXPECT_TRUE(db_id_cache()->empty());
}

TEST_F(AutocompleteActionPredictorTest, DeleteRowsFromCaches) {
  std::vector<AutocompleteActionPredictorTable::Row::Id> all_ids;
  history::URLRows rows;
  for (size_t i = 0; i < base::size(test_url_db); ++i) {
    std::string row_id = AddRow(test_url_db[i]);
    all_ids.push_back(row_id);

    if (i < 2)
      rows.push_back(history::URLRow(test_url_db[i].url));
  }

  EXPECT_EQ(base::size(test_url_db), all_ids.size());
  EXPECT_EQ(base::size(test_url_db), db_cache()->size());
  EXPECT_EQ(base::size(test_url_db), db_id_cache()->size());

  std::vector<AutocompleteActionPredictorTable::Row::Id> id_list;
  DeleteRowsFromCaches(rows, &id_list);

  EXPECT_EQ(base::size(test_url_db) - 2, db_cache()->size());
  EXPECT_EQ(base::size(test_url_db) - 2, db_id_cache()->size());

  for (size_t i = 0; i < base::size(test_url_db); ++i) {
    DBCacheKey key = { test_url_db[i].user_text, test_url_db[i].url };

    bool deleted = (i < 2);
    EXPECT_EQ(deleted, db_cache()->find(key) == db_cache()->end());
    EXPECT_EQ(deleted, db_id_cache()->find(key) == db_id_cache()->end());
    EXPECT_EQ(deleted, base::Contains(id_list, all_ids[i]));
  }
}

TEST_F(AutocompleteActionPredictorTest, DeleteOldIdsFromCaches) {
  std::vector<AutocompleteActionPredictorTable::Row::Id> expected;
  std::vector<AutocompleteActionPredictorTable::Row::Id> all_ids;

  for (size_t i = 0; i < base::size(test_url_db); ++i) {
    std::string row_id = AddRow(test_url_db[i]);
    all_ids.push_back(row_id);

    bool exclude_url =
        base::StartsWith(test_url_db[i].url.path(), "/d",
                         base::CompareCase::SENSITIVE) ||
        (test_url_db[i].days_from_now > maximum_days_to_keep_entry());

    if (exclude_url)
      expected.push_back(row_id);
    else
      ASSERT_TRUE(AddRowToHistory(test_url_db[i]));
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
  for (const auto& info : test_url_confidence_db)
    test_url_ids.push_back(AddRow(info));

  std::vector<AutocompleteActionPredictorTable::Row::Id> id_list;
  std::vector<AutocompleteActionPredictorTable::Row::Id> expected;

  for (size_t i = 0; i < base::size(test_url_confidence_db); ++i) {
    DeleteLowestConfidenceRowsFromCaches(1, &id_list);
    expected.push_back(test_url_ids[i]);
    EXPECT_THAT(id_list, ::testing::UnorderedElementsAreArray(expected));

    DBCacheKey deleted_key = {test_url_confidence_db[i].user_text,
                              test_url_confidence_db[i].url};
    EXPECT_FALSE(base::Contains(*db_cache(), deleted_key));
    EXPECT_FALSE(base::Contains(*db_id_cache(), deleted_key));
  }
}

TEST_F(AutocompleteActionPredictorTest,
       DeleteLowestConfidenceRowsFromCaches_Bulk) {
  std::vector<AutocompleteActionPredictorTable::Row::Id> test_url_ids;
  for (const auto& info : test_url_confidence_db)
    test_url_ids.push_back(AddRow(info));

  std::vector<AutocompleteActionPredictorTable::Row::Id> id_list;
  std::vector<AutocompleteActionPredictorTable::Row::Id> expected;

  const size_t count_to_remove = 4;
  CHECK_LT(count_to_remove, base::size(test_url_confidence_db));

  for (size_t i = 0; i < count_to_remove; ++i)
    expected.push_back(test_url_ids[i]);

  DeleteLowestConfidenceRowsFromCaches(count_to_remove, &id_list);
  ASSERT_THAT(id_list, ::testing::UnorderedElementsAreArray(expected));

  for (size_t i = 0; i < count_to_remove; ++i) {
    DBCacheKey deleted_key = {test_url_confidence_db[i].user_text,
                              test_url_confidence_db[i].url};
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

  AutocompleteMatch match;
  match.type = AutocompleteMatchType::HISTORY_URL;
  prerender::test_utils::RestorePrerenderMode restore_prerender_mode;
  prerender::PrerenderManager::SetMode(
      prerender::PrerenderManager::PRERENDER_MODE_NOSTATE_PREFETCH);

  for (size_t i = 0; i < base::size(test_url_db); ++i) {
    match.destination_url = GURL(test_url_db[i].url);
    EXPECT_EQ(test_url_db[i].expected_action,
              predictor()->RecommendAction(test_url_db[i].user_text, match))
        << "Unexpected action for " << match.destination_url;
  }
}

TEST_F(AutocompleteActionPredictorTest, RecommendActionSearch) {
  ASSERT_NO_FATAL_FAILURE(AddAllRows());

  AutocompleteMatch match;
  match.type = AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED;
  prerender::test_utils::RestorePrerenderMode restore_prerender_mode;
  prerender::PrerenderManager::SetMode(
      prerender::PrerenderManager::PRERENDER_MODE_NOSTATE_PREFETCH);

  for (size_t i = 0; i < base::size(test_url_db); ++i) {
    match.destination_url = GURL(test_url_db[i].url);
    AutocompleteActionPredictor::Action expected_action =
        (test_url_db[i].expected_action ==
         AutocompleteActionPredictor::ACTION_PRERENDER) ?
        AutocompleteActionPredictor::ACTION_PRECONNECT :
        test_url_db[i].expected_action;
    EXPECT_EQ(expected_action,
              predictor()->RecommendAction(test_url_db[i].user_text, match))
        << "Unexpected action for " << match.destination_url;
  }
}

TEST_F(AutocompleteActionPredictorTest,
       RegisterTransitionalMatchesUserTextSizeLimits) {
  auto test = [this](const base::string16& user_text,
                     bool should_be_registered) {
    predictor()->RegisterTransitionalMatches(user_text, AutocompleteResult());
    bool registered = base::Contains(*transitional_matches(), user_text);
    EXPECT_EQ(registered, should_be_registered);
  };

  base::string16 short_text =
      ASCIIToUTF16(std::string(minimum_user_text_length(), 'g'));
  test(short_text, true);

  base::string16 too_short_text =
      ASCIIToUTF16(std::string(minimum_user_text_length() - 1, 'g'));
  test(too_short_text, false);

  base::string16 long_text =
      ASCIIToUTF16(std::string(maximum_string_length(), 'g'));
  test(long_text, true);

  base::string16 too_long_text =
      ASCIIToUTF16(std::string(maximum_string_length() + 1, 'g'));
  test(too_long_text, false);
}

TEST_F(AutocompleteActionPredictorTest,
       RegisterTransitionalMatchesURLSizeLimits) {
  GURL urls[] = {GenerateTestURL(10), GenerateTestURL(maximum_string_length()),
                 GenerateTestURL(maximum_string_length() + 1),
                 GenerateTestURL(maximum_string_length() * 10)};
  ACMatches matches;
  for (const auto& url : urls) {
    AutocompleteMatch match;
    match.destination_url = url;
    matches.push_back(match);
  }
  AutocompleteResult result;
  result.AppendMatches(AutocompleteInput(), matches);
  base::string16 user_text = ASCIIToUTF16("google");
  predictor()->RegisterTransitionalMatches(user_text, result);
  auto it = std::find(transitional_matches()->begin(),
                      transitional_matches()->end(), user_text);
  ASSERT_NE(it, transitional_matches()->end());
  EXPECT_THAT(it->urls, ::testing::ElementsAre(urls[0], urls[1]));
}

}  // namespace predictors
