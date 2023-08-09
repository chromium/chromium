// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/help_app_ui/search/search_handler.h"
#include <cstddef>

#include "ash/webui/help_app_ui/search/search_concept.h"
#include "ash/webui/help_app_ui/search/search_tag_registry.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::help_app {
namespace {

class FakeObserver : public mojom::SearchResultsObserver {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  mojo::PendingRemote<mojom::SearchResultsObserver> GenerateRemote() {
    mojo::PendingRemote<mojom::SearchResultsObserver> remote;
    receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  size_t num_calls() const { return num_calls_; }

 private:
  // mojom::SearchResultsObserver:
  void OnSearchResultAvailabilityChanged() override { ++num_calls_; }

  size_t num_calls_ = 0;
  mojo::Receiver<mojom::SearchResultsObserver> receiver_{this};
};

}  // namespace

class HelpAppSearchHandlerTest : public testing::Test {
 protected:
  HelpAppSearchHandlerTest()
      : search_tag_registry_(local_search_service_proxy_.get()),
        handler_(&search_tag_registry_, local_search_service_proxy_.get()) {}
  ~HelpAppSearchHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    handler_.BindInterface(handler_remote_.BindNewPipeAndPassReceiver());

    handler_remote_->Observe(search_results_observer_.GenerateRemote());
    handler_remote_.FlushForTesting();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void SetupInitialPersistenceSearchConcepts() {
    SearchConcept persistence(GetPersistencePath());
    std::vector<mojom::SearchConceptPtr> search_concepts;
    mojom::SearchConceptPtr new_concept_1 = mojom::SearchConcept::New(
        /*id=*/"test-id-1",
        /*title=*/u"Title 1",
        /*main_category=*/u"Help",
        /*tags=*/std::vector<std::u16string>{u"Test tag", u"Tag 2"},
        /*tag_locale=*/"en",
        /*url_path_with_parameters=*/"help",
        /*locale=*/"");
    mojom::SearchConceptPtr new_concept_2 = mojom::SearchConcept::New(
        /*id=*/"test-id-2",
        /*title=*/u"Title 2",
        /*main_category=*/u"Help",
        /*tags=*/std::vector<std::u16string>{u"Another test tag"},
        /*tag_locale=*/"en",
        /*url_path_with_parameters=*/"help",
        /*locale=*/"");
    search_concepts.push_back(std::move(new_concept_1));
    search_concepts.push_back(std::move(new_concept_2));
    persistence.UpdateSearchConcepts(search_concepts);
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(base::PathExists(GetPersistencePath()));
  }

  void SimulateWebDataUpdate() {
    std::vector<mojom::SearchConceptPtr> new_search_concepts;
    mojom::SearchConceptPtr new_concept_1 = mojom::SearchConcept::New(
        /*id=*/"test-id-1",
        /*title=*/u"Title 1",
        /*main_category=*/u"Help",
        /*tags=*/std::vector<std::u16string>{u"Printing"},
        /*tag_locale=*/"en",
        /*url_path_with_parameters=*/"help",
        /*locale=*/"");
    new_search_concepts.push_back(std::move(new_concept_1));
    Update(std::move(new_search_concepts));
    handler_remote_.FlushForTesting();
    task_environment_.RunUntilIdle();
  }

  base::FilePath GetTempPath() { return temp_dir_.GetPath(); }

  base::FilePath GetPersistencePath() {
    return temp_dir_.GetPath()
        .AppendASCII("help_app/")
        .AppendASCII("persistence.pb");
  }

  void OnRead(size_t expected_size,
              std::vector<mojom::SearchConceptPtr> search_concepts) {
    EXPECT_EQ(search_concepts.size(), expected_size);
  }

  base::OnceCallback<void(std::vector<mojom::SearchConceptPtr>)> ReadCallback(
      size_t expected_size) {
    return base::BindOnce(&HelpAppSearchHandlerTest::OnRead,
                          base::Unretained(this), expected_size);
  }

  std::vector<mojom::SearchResultPtr> Search(const std::u16string& query,
                                             int32_t max_num_results) {
    base::test::TestFuture<std::vector<mojom::SearchResultPtr>> future;
    handler_remote_->Search(query, max_num_results, future.GetCallback());
    return future.Take();
  }

  void Update(std::vector<mojom::SearchConceptPtr> search_concepts) {
    base::test::TestFuture<void> future;
    handler_remote_->Update(std::move(search_concepts), future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_ =
          std::make_unique<local_search_service::LocalSearchServiceProxy>(
              /*for_testing=*/true);
  SearchTagRegistry search_tag_registry_;
  SearchHandler handler_;
  mojo::Remote<mojom::SearchHandler> handler_remote_;
  FakeObserver search_results_observer_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(HelpAppSearchHandlerTest, UpdateAndSearch) {
  // Add some search tags.
  std::vector<mojom::SearchConceptPtr> search_concepts;
  mojom::SearchConceptPtr new_concept_1 = mojom::SearchConcept::New(
      /*id=*/"test-id-1",
      /*title=*/u"Title 1",
      /*main_category=*/u"Help",
      /*tags=*/std::vector<std::u16string>{u"Test tag", u"Tag 2"},
      /*tag_locale=*/"en",
      /*url_path_with_parameters=*/"help",
      /*locale=*/"");
  mojom::SearchConceptPtr new_concept_2 = mojom::SearchConcept::New(
      /*id=*/"test-id-2",
      /*title=*/u"Title 2",
      /*main_category=*/u"Help",
      /*tags=*/std::vector<std::u16string>{u"Another test tag"},
      /*tag_locale=*/"en",
      /*url_path_with_parameters=*/"help",
      /*locale=*/"");
  search_concepts.push_back(std::move(new_concept_1));
  search_concepts.push_back(std::move(new_concept_2));

  Update(std::move(search_concepts));
  handler_remote_.FlushForTesting();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(1u, search_results_observer_.num_calls());

  std::vector<mojom::SearchResultPtr> search_results;

  // 2 results should be available for a "test tag" query.
  search_results = Search(u"test tag",
                          /*max_num_results=*/3u);
  EXPECT_EQ(search_results.size(), 2u);

  // Limit results to 1 max and ensure that only 1 result is returned.
  search_results = Search(u"test tag",
                          /*max_num_results=*/1u);
  EXPECT_EQ(search_results.size(), 1u);

  // Search for a query which should return no results.
  search_results = Search(u"QueryWithNoResults",
                          /*max_num_results=*/3u);
  EXPECT_TRUE(search_results.empty());
}

TEST_F(HelpAppSearchHandlerTest, SearchResultMetadata) {
  // Add some search tags.
  std::vector<mojom::SearchConceptPtr> search_concepts;
  mojom::SearchConceptPtr new_concept_1 = mojom::SearchConcept::New(
      /*id=*/"test-id-1",
      /*title=*/u"Title 1",
      /*main_category=*/u"Help",
      /*tags=*/std::vector<std::u16string>{u"Test tag", u"Printing"},
      /*tag_locale=*/"en",
      /*url_path_with_parameters=*/"help",
      /*locale=*/"");
  search_concepts.push_back(std::move(new_concept_1));

  Update(std::move(search_concepts));
  handler_remote_.FlushForTesting();
  task_environment_.RunUntilIdle();

  std::vector<mojom::SearchResultPtr> search_results;
  search_results = Search(u"Printing",
                          /*max_num_results=*/3u);

  EXPECT_EQ(search_results.size(), 1u);
  EXPECT_EQ(search_results[0]->id, "test-id-1");
  EXPECT_EQ(search_results[0]->title, u"Title 1");
  EXPECT_EQ(search_results[0]->main_category, u"Help");
  EXPECT_EQ(search_results[0]->locale, "");
  EXPECT_GT(search_results[0]->relevance_score, 0.01);
}

TEST_F(HelpAppSearchHandlerTest, SearchResultOrdering) {
  // Add some search tags.
  std::vector<mojom::SearchConceptPtr> search_concepts;
  mojom::SearchConceptPtr new_concept_1 = mojom::SearchConcept::New(
      /*id=*/"test-id-less",
      /*title=*/u"Title 1",
      /*main_category=*/u"Help",
      /*tags=*/std::vector<std::u16string>{u"less relevant concept"},
      /*tag_locale=*/"en",
      /*url_path_with_parameters=*/"help",
      /*locale=*/"");
  mojom::SearchConceptPtr new_concept_2 = mojom::SearchConcept::New(
      /*id=*/"test-id-more",
      /*title=*/u"Title 2",
      /*main_category=*/u"Help",
      /*tags=*/std::vector<std::u16string>{u"more relevant tag", u"Tag"},
      /*tag_locale=*/"en",
      /*url_path_with_parameters=*/"help",
      /*locale=*/"");
  search_concepts.push_back(std::move(new_concept_1));
  search_concepts.push_back(std::move(new_concept_2));

  Update(std::move(search_concepts));
  handler_remote_.FlushForTesting();
  task_environment_.RunUntilIdle();

  std::vector<mojom::SearchResultPtr> search_results;
  search_results = Search(u"relevant tag",
                          /*max_num_results=*/3u);

  // The more relevant concept should be first, but the other concept still has
  // some relevance.
  ASSERT_EQ(search_results.size(), 2u);
  EXPECT_EQ(search_results[0]->id, "test-id-more");
  EXPECT_EQ(search_results[1]->id, "test-id-less");
  EXPECT_GT(search_results[0]->relevance_score,
            search_results[1]->relevance_score);
  EXPECT_GT(search_results[1]->relevance_score, 0.01);
}

TEST_F(HelpAppSearchHandlerTest, SearchStatusNotReadyAndEmptyIndex) {
  base::HistogramTester histogram_tester;
  std::vector<mojom::SearchResultPtr> search_results;

  // Search without updating the index.
  search_results = Search(u"test query", /*max_num_results=*/3u);

  EXPECT_TRUE(search_results.empty());
  // 0 is kNotReadyAndEmptyIndex.
  histogram_tester.ExpectUniqueSample(
      "Discover.SearchHandler.SearchResultStatus", 0, 1);
}

TEST_F(HelpAppSearchHandlerTest, SearchStatusReadyAndSuccess) {
  // Add one item to the search index.
  std::vector<mojom::SearchConceptPtr> search_concepts;
  mojom::SearchConceptPtr new_concept_1 = mojom::SearchConcept::New(
      /*id=*/"test-id-1",
      /*title=*/u"Title 1",
      /*main_category=*/u"Help",
      /*tags=*/std::vector<std::u16string>{u"Test tag", u"Printing"},
      /*tag_locale=*/"en",
      /*url_path_with_parameters=*/"help",
      /*locale=*/"");
  search_concepts.push_back(std::move(new_concept_1));
  Update(std::move(search_concepts));
  handler_remote_.FlushForTesting();
  task_environment_.RunUntilIdle();

  base::HistogramTester histogram_tester;
  std::vector<mojom::SearchResultPtr> search_results;

  search_results = Search(u"Printing", /*max_num_results=*/3u);

  EXPECT_EQ(search_results.size(), 1u);
  // 2 is kReadyAndSuccess.
  histogram_tester.ExpectUniqueSample(
      "Discover.SearchHandler.SearchResultStatus", 2, 1);
}

TEST_F(HelpAppSearchHandlerTest, SearchStatusReadyAndEmptyIndex) {
  // Update using an empty list. This can happen if there is no localized
  // content for the current locale.
  std::vector<mojom::SearchConceptPtr> search_concepts;
  Update(std::move(search_concepts));
  handler_remote_.FlushForTesting();
  task_environment_.RunUntilIdle();

  base::HistogramTester histogram_tester;
  std::vector<mojom::SearchResultPtr> search_results;

  search_results = Search(u"Printing", /*max_num_results=*/3u);

  EXPECT_TRUE(search_results.empty());
  // 3 is kReadyAndEmptyIndex.
  histogram_tester.ExpectUniqueSample(
      "Discover.SearchHandler.SearchResultStatus", 3, 1);
}

TEST_F(HelpAppSearchHandlerTest, SearchStatusReadyAndOtherStatus) {
  // Add one item to the search index.
  std::vector<mojom::SearchConceptPtr> search_concepts;
  mojom::SearchConceptPtr new_concept_1 = mojom::SearchConcept::New(
      /*id=*/"test-id-1",
      /*title=*/u"Title 1",
      /*main_category=*/u"Help",
      /*tags=*/std::vector<std::u16string>{u"Test tag", u"Printing"},
      /*tag_locale=*/"en",
      /*url_path_with_parameters=*/"help",
      /*locale=*/"");
  search_concepts.push_back(std::move(new_concept_1));
  Update(std::move(search_concepts));
  handler_remote_.FlushForTesting();
  task_environment_.RunUntilIdle();

  base::HistogramTester histogram_tester;
  std::vector<mojom::SearchResultPtr> search_results;

  // Searching with an empty query results in a different status: kEmptyQuery.
  search_results = Search(u"", /*max_num_results=*/3u);

  EXPECT_TRUE(search_results.empty());
  // 4 is kReadyAndOtherStatus.
  histogram_tester.ExpectUniqueSample(
      "Discover.SearchHandler.SearchResultStatus", 4, 1);
}

TEST_F(HelpAppSearchHandlerTest, InitializeWithoutPersistence) {
  // Load when persistence not exist.
  EXPECT_FALSE(base::PathExists(GetPersistencePath()));
  handler_.OnProfileDirAvailable(GetTempPath());
  handler_remote_.FlushForTesting();
  task_environment_.RunUntilIdle();

  // Nothing is updated.
  EXPECT_EQ(0u, search_results_observer_.num_calls());

  // Web data comes.
  SimulateWebDataUpdate();

  // Updated from web data.
  EXPECT_EQ(1u, search_results_observer_.num_calls());

  // Check the persistence is generated.
  EXPECT_TRUE(base::PathExists(GetPersistencePath()));
  SearchConcept persistence(GetPersistencePath());
  persistence.GetSearchConcepts(ReadCallback(1u));
}

TEST_F(HelpAppSearchHandlerTest, InitializeWithPersistence) {
  // Add persistence to disk.
  SetupInitialPersistenceSearchConcepts();

  // Load from persistence.
  handler_.OnProfileDirAvailable(GetTempPath());
  handler_remote_.FlushForTesting();
  task_environment_.RunUntilIdle();

  // Updated from persistence.
  EXPECT_EQ(1u, search_results_observer_.num_calls());

  std::vector<mojom::SearchResultPtr> search_results;

  // There should be results.
  search_results = Search(u"test tag",
                          /*max_num_results=*/3u);
  EXPECT_EQ(search_results.size(), 2u);
}

TEST_F(HelpAppSearchHandlerTest, PersistenceUpdateWithNewData) {
  // Add persistence to disk.
  SetupInitialPersistenceSearchConcepts();

  // Load from persistence.
  handler_.OnProfileDirAvailable(GetTempPath());
  handler_remote_.FlushForTesting();
  task_environment_.RunUntilIdle();

  // Updated from persistence.
  EXPECT_EQ(1u, search_results_observer_.num_calls());

  // Web data comes.
  SimulateWebDataUpdate();

  // Updated from new data.
  EXPECT_EQ(2u, search_results_observer_.num_calls());

  std::vector<mojom::SearchResultPtr> search_results;

  // There should be new results.
  search_results = Search(u"Printing", /*max_num_results=*/3u);
  EXPECT_EQ(search_results.size(), 1u);

  // There should be no old results.
  search_results = Search(u"test tag",
                          /*max_num_results=*/3u);
  EXPECT_EQ(search_results.size(), 0u);

  // Check the persistence is also updated.
  SearchConcept persistence(GetPersistencePath());
  persistence.GetSearchConcepts(ReadCallback(1u));
}

TEST_F(HelpAppSearchHandlerTest, NewDataComesBeforePersistenceLoad) {
  // Add persistence to disk.
  SetupInitialPersistenceSearchConcepts();

  // Web data comes.
  SimulateWebDataUpdate();

  // Updated from web data.
  EXPECT_EQ(1u, search_results_observer_.num_calls());

  // Load from persistence after the new data comes.
  handler_.OnProfileDirAvailable(GetTempPath());
  handler_remote_.FlushForTesting();
  task_environment_.RunUntilIdle();

  // No update from persistence.
  EXPECT_EQ(1u, search_results_observer_.num_calls());

  std::vector<mojom::SearchResultPtr> search_results;

  // There should be new results.
  search_results = Search(u"Printing", /*max_num_results=*/3u);
  EXPECT_EQ(search_results.size(), 1u);

  // There should be no persistence results.
  search_results = Search(u"test tag",
                          /*max_num_results=*/3u);
  EXPECT_EQ(search_results.size(), 0u);

  // Check the persistence is also updated.
  SearchConcept persistence(GetPersistencePath());
  persistence.GetSearchConcepts(ReadCallback(1u));
}

}  // namespace ash::help_app
