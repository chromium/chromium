// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/help_app_ui/search/search_handler.h"

#include "ash/webui/help_app_ui/search/search.mojom-test-utils.h"
#include "ash/webui/help_app_ui/search/search_tag_registry.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace help_app {
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

    handler_remote_->Observe(observer_.GenerateRemote());
    handler_remote_.FlushForTesting();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_ =
          std::make_unique<local_search_service::LocalSearchServiceProxy>(
              /*for_testing=*/true);
  SearchTagRegistry search_tag_registry_;
  SearchHandler handler_;
  mojo::Remote<mojom::SearchHandler> handler_remote_;
  FakeObserver observer_;
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

  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Update(std::move(search_concepts));
  handler_remote_.FlushForTesting();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(1u, observer_.num_calls());

  std::vector<mojom::SearchResultPtr> search_results;

  // 2 results should be available for a "test tag" query.
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"test tag",
              /*max_num_results=*/3u, &search_results);
  EXPECT_EQ(search_results.size(), 2u);

  // Limit results to 1 max and ensure that only 1 result is returned.
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"test tag",
              /*max_num_results=*/1u, &search_results);
  EXPECT_EQ(search_results.size(), 1u);

  // Search for a query which should return no results.
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"QueryWithNoResults",
              /*max_num_results=*/3u, &search_results);
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

  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Update(std::move(search_concepts));
  handler_remote_.FlushForTesting();
  task_environment_.RunUntilIdle();

  std::vector<mojom::SearchResultPtr> search_results;
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"Printing",
              /*max_num_results=*/3u, &search_results);

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

  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Update(std::move(search_concepts));
  handler_remote_.FlushForTesting();
  task_environment_.RunUntilIdle();

  std::vector<mojom::SearchResultPtr> search_results;
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"relevant tag",
              /*max_num_results=*/3u, &search_results);

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
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"test query", /*max_num_results=*/3u, &search_results);

  EXPECT_TRUE(search_results.empty());
  // 0 is kNotReadyAndEmptyIndex.
  histogram_tester.ExpectUniqueSample(
      "Discover.SearchHandler.SearchResultStatus", 0, 1);
}

TEST_F(HelpAppSearchHandlerTest, SearchStatusNotReadyAndOtherStatus) {
  base::HistogramTester histogram_tester;
  std::vector<mojom::SearchResultPtr> search_results;

  // The empty search query makes the LSS respond with kEmptyQuery rather than
  // kEmptyIndex.
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"", /*max_num_results=*/3u, &search_results);

  EXPECT_TRUE(search_results.empty());
  // 1 is kNotReadyAndOtherStatus.
  histogram_tester.ExpectUniqueSample(
      "Discover.SearchHandler.SearchResultStatus", 1, 1);
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
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Update(std::move(search_concepts));
  handler_remote_.FlushForTesting();
  task_environment_.RunUntilIdle();

  base::HistogramTester histogram_tester;
  std::vector<mojom::SearchResultPtr> search_results;

  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"Printing", /*max_num_results=*/3u, &search_results);

  EXPECT_EQ(search_results.size(), 1u);
  // 2 is kReadyAndSuccess.
  histogram_tester.ExpectUniqueSample(
      "Discover.SearchHandler.SearchResultStatus", 2, 1);
}

TEST_F(HelpAppSearchHandlerTest, SearchStatusReadyAndEmptyIndex) {
  // Update using an empty list. This can happen if there is no localized
  // content for the current locale.
  std::vector<mojom::SearchConceptPtr> search_concepts;
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Update(std::move(search_concepts));
  handler_remote_.FlushForTesting();
  task_environment_.RunUntilIdle();

  base::HistogramTester histogram_tester;
  std::vector<mojom::SearchResultPtr> search_results;

  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"Printing", /*max_num_results=*/3u, &search_results);

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
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Update(std::move(search_concepts));
  handler_remote_.FlushForTesting();
  task_environment_.RunUntilIdle();

  base::HistogramTester histogram_tester;
  std::vector<mojom::SearchResultPtr> search_results;

  // Searching with an empty query results in a different status: kEmptyQuery.
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"", /*max_num_results=*/3u, &search_results);

  EXPECT_TRUE(search_results.empty());
  // 4 is kReadyAndOtherStatus.
  histogram_tester.ExpectUniqueSample(
      "Discover.SearchHandler.SearchResultStatus", 4, 1);
}

}  // namespace help_app
}  // namespace ash
