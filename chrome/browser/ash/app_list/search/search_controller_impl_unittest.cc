// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/search_controller_impl.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/ranking/launch_data.h"
#include "chrome/browser/ash/app_list/search/ranking/ranker_manager.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ash/app_list/search/test/search_controller_test_util.h"
#include "chrome/browser/ash/app_list/search/test/test_ranker_manager.h"
#include "chrome/browser/ash/app_list/search/test/test_search_provider.h"
#include "chrome/browser/ash/app_list/test/fake_app_list_model_updater.h"
#include "chrome/browser/ash/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace app_list::test {

namespace {

using testing::ElementsAreArray;
using testing::UnorderedElementsAreArray;
using Category = ash::AppListSearchResultCategory;
using DisplayType = ash::SearchResultDisplayType;
using Result = ash::AppListSearchResultType;

LaunchData CreateFakeLaunchData(const std::string& id) {
  app_list::LaunchData launch;
  launch.id = id;
  launch.result_type = ash::AppListSearchResultType::kPlayStoreApp;
  launch.score = 0.0;

  return launch;
}

class FakeObserver : public SearchController::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  void OnResultsAdded(
      const std::u16string& query,
      const std::vector<const ChromeSearchResult*>& results) override {
    results_added_ = true;
  }

  bool results_added() const { return results_added_; }

 private:
  bool results_added_ = false;
};

}  // namespace

class SearchControllerImplTest : public testing::Test {
 public:
  SearchControllerImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  SearchControllerImplTest(const SearchControllerImplTest&) = delete;
  SearchControllerImplTest& operator=(const SearchControllerImplTest&) = delete;
  ~SearchControllerImplTest() override = default;

  void SetUp() override {
    search_controller_ = std::make_unique<SearchControllerImpl>(
        /*model_updater=*/&model_updater_,
        /*list_controller=*/&list_controller_,
        /*notifier=*/nullptr, &profile_);

    auto ranker_manager = std::make_unique<TestRankerManager>(&profile_);
    ranker_manager_ = ranker_manager.get();
    search_controller_->set_ranker_manager_for_test(std::move(ranker_manager));
  }

  void ExpectIdOrder(std::vector<std::string> expected_ids) {
    const auto& actual_results = model_updater_.search_results();
    EXPECT_EQ(actual_results.size(), expected_ids.size());
    std::vector<std::string> actual_ids;
    std::transform(actual_results.begin(), actual_results.end(),
                   std::back_inserter(actual_ids),
                   [](const ChromeSearchResult* res) -> const std::string& {
                     return res->id();
                   });
    EXPECT_THAT(actual_ids, ElementsAreArray(expected_ids));
  }

  // Compares expected category burn-in iteration numbers to those recorded
  // within the search controller. The expected list should not include
  // categories for which burn-in number is unset (i.e. = -1).
  void ExpectCategoriesToBurnInIterations(
      std::vector<std::pair<Category, int>>
          expected_categories_to_burnin_iteration) {
    const auto& actual_categories_list = search_controller_->categories_;
    std::vector<std::pair<Category, int>> actual_categories_to_burnin_iteration;

    for (const auto& category : actual_categories_list) {
      if (category.burnin_iteration != -1) {
        actual_categories_to_burnin_iteration.emplace_back(
            category.category, category.burnin_iteration);
      }
    }

    EXPECT_THAT(
        actual_categories_to_burnin_iteration,
        UnorderedElementsAreArray(expected_categories_to_burnin_iteration));
  }

  void ExpectIdsToBurnInIterations(std::vector<std::pair<std::string, int>>
                                       expected_ids_to_burnin_iteration) {
    const auto& actual_ids_to_burnin_iteration =
        std::vector<std::pair<std::string, int>>(
            search_controller_->burnin_controller_
                ->ids_to_burnin_iteration_for_test()
                .begin(),
            search_controller_->burnin_controller_
                ->ids_to_burnin_iteration_for_test()
                .end());
    ASSERT_EQ(actual_ids_to_burnin_iteration.size(),
              expected_ids_to_burnin_iteration.size());
    EXPECT_THAT(actual_ids_to_burnin_iteration,
                UnorderedElementsAreArray(expected_ids_to_burnin_iteration));
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  // Add a wait period in milliseconds to allow results collected from search
  // providers. The default period is 1000 milliseconds (i.e., 1 second).
  void WaitInMilliseconds(int n = 1000) {
    task_environment_.FastForwardBy(base::Milliseconds(n));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  FakeAppListModelUpdater model_updater_{&profile_, /*order_delegate=*/nullptr};
  std::unique_ptr<SearchControllerImpl> search_controller_;
  ::test::TestAppListControllerDelegate list_controller_{};
  // Owned by |search_controller_|.
  TestRankerManager* ranker_manager_{nullptr};
};

// Tests that best matches are ordered first, and categories are ignored when
// ranking within best match.
TEST_F(SearchControllerImplTest, BestMatchesOrderedAboveOtherResults) {
  auto results_1 = MakeListResults(
      {"a", "b", "c", "d"},
      {Category::kWeb, Category::kWeb, Category::kApps, Category::kWeb},
      {0, -1, 1, -1}, {0.4, 0.7, 0.2, 0.8});
  ranker_manager_->SetCategoryRanks(
      {{Category::kApps, 0.4}, {Category::kWeb, 0.2}});

  search_controller_->StartSearch(u"abc");
  // Simulate a provider returning and containing the first set of results. A
  // single provider wouldn't return many results like this, but that's
  // unimportant for the test.
  search_controller_->SetResults(SimpleProvider(Result::kOmnibox),
                                 std::move(results_1));
  WaitInMilliseconds();
  // Expect that:
  //   - best matches are ordered first,
  //   - best matches are ordered by best match rank,
  //   - categories are ignored within best match.
  ExpectIdOrder({"a", "c", "d", "b"});

  // Simulate the arrival of another result into the best match category. Its
  // best match rank takes precedence over its relevance score in determining
  // its rank within the best matches.
  auto results_2 = MakeListResults({"e"}, {Category::kFiles}, {2}, {0.9});
  search_controller_->SetResults(SimpleProvider(Result::kFileSearch),
                                 std::move(results_2));
  ExpectIdOrder({"a", "c", "e", "d", "b"});
}

TEST_F(SearchControllerImplTest,
       BurnInIterationNumbersTrackedInQuerySearch_Results) {
  // This test focuses on the book-keeping of burn-in iteration numbers for
  // individual results, and ignores the effect that these numbers can have on
  // final sorting of the categories or results lists.

  ranker_manager_->SetCategoryRanks({{Category::kFiles, 0.1}});

  // Set up some results from two different providers.
  auto file_results = MakeListResults({"a"}, {Category::kFiles}, {-1}, {0.9});
  auto app_results = MakeListResults({"b"}, {Category::kApps}, {-1}, {0.1});

  // Set up results from a third different provider. This provider will first
  // return one set of results, then later return an updated set of results.
  auto web_results_first_arrival = MakeListResults(
      {"c", "d"}, {Category::kWeb, Category::kWeb}, {-1, -1}, {0.2, 0.1});
  auto web_results_second_arrival = MakeListResults(
      {"c", "d", "e"}, {Category::kWeb, Category::kWeb, Category::kWeb},
      {-1, -1, -1}, {0.2, 0.1, 0.4});

  // Simulate starting a search.
  search_controller_->StartSearch(u"abc");

  // Simulate providers returning results within the burn-in period.
  search_controller_->SetResults(SimpleProvider(Result::kFileSearch),
                                 std::move(file_results));
  ExpectIdsToBurnInIterations({{"a", 0}});
  search_controller_->SetResults(SimpleProvider(Result::kInstalledApp),
                                 std::move(app_results));
  ExpectIdsToBurnInIterations({{"a", 0}, {"b", 0}});

  // Simulate a provider returning results after the burn-in period.
  WaitInMilliseconds();
  search_controller_->SetResults(SimpleProvider(Result::kOmnibox),
                                 std::move(web_results_first_arrival));
  ExpectIdsToBurnInIterations({{"a", 0}, {"b", 0}, {"c", 1}, {"d", 1}});

  // Simulate a provider returning for a second time. The burn-in iteration
  // number for previously seen results is preserved, while that of newly seen
  // results is incremented.
  search_controller_->SetResults(SimpleProvider(Result::kOmnibox),
                                 std::move(web_results_second_arrival));
  ExpectIdsToBurnInIterations(
      {{"a", 0}, {"b", 0}, {"c", 1}, {"d", 1}, {"e", 2}});
}

TEST_F(SearchControllerImplTest,
       BurnInIterationNumbersTrackedInQuerySearch_Categories) {
  // This test focuses on the book-keeping of burn-in iteration numbers for
  // categories, and ignores the effect that these numbers can have on final
  // sorting of the categories or results lists.

  ranker_manager_->SetCategoryRanks({{Category::kFiles, 0.1}});

  // Set up some results from four different providers. Only their categories
  // are relevant, and individual result scores are not.
  auto file_results = MakeListResults({"a"}, {Category::kFiles}, {-1}, {0.9});
  auto app_results = MakeListResults({"b"}, {Category::kApps}, {-1}, {0.1});
  // This provider will first return one set of results, then later return an
  // updated set of results.
  auto web_results_first_arrival = MakeListResults(
      {"c", "d"}, {Category::kWeb, Category::kWeb}, {-1, -1}, {0.2, 0.1});
  auto web_results_second_arrival = MakeListResults(
      {"c", "d", "e"}, {Category::kWeb, Category::kWeb, Category::kWeb},
      {-1, -1, -1}, {0.2, 0.1, 0.4});
  auto settings_results =
      MakeListResults({"f"}, {Category::kSettings}, {-1}, {0.8});

  // Simulate starting a search.
  search_controller_->StartSearch(u"abc");

  // Simulate providers returning results within the burn-in period.
  search_controller_->SetResults(SimpleProvider(Result::kFileSearch),
                                 std::move(file_results));
  ExpectCategoriesToBurnInIterations({{Category::kFiles, 0}});

  search_controller_->SetResults(SimpleProvider(Result::kInstalledApp),
                                 std::move(app_results));
  ExpectCategoriesToBurnInIterations(
      {{Category::kFiles, 0}, {Category::kApps, 0}});

  // Simulate a third provider returning results after the burn-in period.
  WaitInMilliseconds();
  search_controller_->SetResults(SimpleProvider(Result::kOmnibox),
                                 std::move(web_results_first_arrival));
  ExpectCategoriesToBurnInIterations(
      {{Category::kFiles, 0}, {Category::kApps, 0}, {Category::kWeb, 1}});

  // Simulate the third provider returning for a second time. The burn-in
  // iteration number for that category is not updated.
  search_controller_->SetResults(SimpleProvider(Result::kOmnibox),
                                 std::move(web_results_second_arrival));
  ExpectCategoriesToBurnInIterations(
      {{Category::kFiles, 0}, {Category::kApps, 0}, {Category::kWeb, 1}});

  // Simulate a fourth provider returning for the first time.
  search_controller_->SetResults(SimpleProvider(Result::kOsSettings),
                                 std::move(settings_results));
  ExpectCategoriesToBurnInIterations({{Category::kFiles, 0},
                                      {Category::kApps, 0},
                                      {Category::kWeb, 1},
                                      {Category::kSettings, 3}});
}

// Tests that categories which arrive pre-burn-in are ordered correctly, and
// their results are grouped together and ordered by score.
TEST_F(SearchControllerImplTest, CategoriesOrderedCorrectly_PreBurnIn) {
  ranker_manager_->SetCategoryRanks(
      {{Category::kFiles, 0.3}, {Category::kWeb, 0.2}, {Category::kApps, 0.1}});
  auto file_results = MakeListResults({"a"}, {Category::kFiles}, {-1}, {0.9});
  auto web_results = MakeListResults(
      {"c", "d", "b"}, {Category::kWeb, Category::kWeb, Category::kWeb},
      {-1, -1, -1}, {0.2, 0.1, 0.4});
  auto app_results = MakeListResults({"e"}, {Category::kApps}, {-1}, {0.1});

  // Simulate starting a search.
  search_controller_->StartSearch(u"abc");
  // Simulate several providers returning results pre-burn-in.
  search_controller_->SetResults(SimpleProvider(Result::kOmnibox),
                                 std::move(web_results));
  search_controller_->SetResults(SimpleProvider(Result::kInstalledApp),
                                 std::move(app_results));
  search_controller_->SetResults(SimpleProvider(Result::kFileSearch),
                                 std::move(file_results));
  WaitInMilliseconds();

  ExpectIdOrder({"a", "b", "c", "d", "e"});
}

// Tests that categories which arrive post-burn-in are ordered correctly, and
// their results are grouped together and ordered by score.
TEST_F(SearchControllerImplTest, CategoriesOrderedCorrectly_PostBurnIn) {
  ranker_manager_->SetCategoryRanks(
      {{Category::kFiles, 0.3}, {Category::kWeb, 0.2}, {Category::kApps, 0.1}});
  auto web_results = MakeListResults(
      {"b", "c", "a"}, {Category::kWeb, Category::kWeb, Category::kWeb},
      {-1, -1, -1}, {0.2, 0.1, 0.4});
  auto app_results = MakeListResults(
      {"e", "d"}, {Category::kApps, Category::kApps}, {-1, -1}, {0.7, 0.9});
  auto file_results = MakeListResults({"f"}, {Category::kFiles}, {-1}, {0.8});

  // Simulate starting a search.
  search_controller_->StartSearch(u"abc");
  // Simulate several providers returning results post-burn-in.
  WaitInMilliseconds();
  search_controller_->SetResults(SimpleProvider(Result::kOmnibox),
                                 std::move(web_results));
  ExpectIdOrder({"a", "b", "c"});
  search_controller_->SetResults(SimpleProvider(Result::kInstalledApp),
                                 std::move(app_results));
  ExpectIdOrder({"a", "b", "c", "d", "e"});
  search_controller_->SetResults(SimpleProvider(Result::kFileSearch),
                                 std::move(file_results));
  ExpectIdOrder({"a", "b", "c", "d", "e", "f"});
}

// Tests that categories are ordered correctly, where some categories arrive
// pre-burn-in and others arrive post-burn-in. Test that their results are
// grouped together and ordered by score.
TEST_F(
    SearchControllerImplTest,
    CategoriesOrderedCorrectly_PreAndPostBurnIn_OneProviderReturnPerCategory) {
  ranker_manager_->SetCategoryRanks(
      {{Category::kFiles, 0.3}, {Category::kWeb, 0.2}, {Category::kApps, 0.1}});
  auto web_results = MakeListResults(
      {"c", "d", "b"}, {Category::kWeb, Category::kWeb, Category::kWeb},
      {-1, -1, -1}, {0.3, 0.2, 0.4});
  auto app_results = MakeListResults({"e"}, {Category::kApps}, {-1}, {0.1});
  auto file_results = MakeListResults({"a"}, {Category::kFiles}, {-1}, {0.9});

  // Simulate starting a search.
  search_controller_->StartSearch(u"abc");

  // Simulate a provider returning results within the burn-in period.
  search_controller_->SetResults(SimpleProvider(Result::kOmnibox),
                                 std::move(web_results));
  ExpectIdOrder({});

  // Expect results to appear after burn-in period has elapsed.
  WaitInMilliseconds();
  ExpectIdOrder({"b", "c", "d"});

  // Simulate several providers returning results after the burn-in period.
  search_controller_->SetResults(SimpleProvider(Result::kInstalledApp),
                                 std::move(app_results));
  ExpectIdOrder({"b", "c", "d", "e"});
  search_controller_->SetResults(SimpleProvider(Result::kFileSearch),
                                 std::move(file_results));
  ExpectIdOrder({"b", "c", "d", "e", "a"});
}

// Tests that the Search and Assistant category is ordered correctly when it
// arrives pre-burn-in, and remains correctly ordered when further categories
// arrive.
//
// At the time of its arrival, Search and Assistant should initially be pinned
// to the bottom of the categories list, but later-arriving categories should
// appear below Search and Assistant.
TEST_F(SearchControllerImplTest,
       CategoriesOrderedCorrectly_SearchAndAssistantPinnedToBottomOfPreBurnIn) {
  ranker_manager_->SetCategoryRanks({{Category::kFiles, 0.3},
                                     {Category::kSearchAndAssistant, 0.2},
                                     {Category::kApps, 0.1}});
  auto search_and_assistant_results = MakeListResults(
      {"a", "b", "c"},
      {Category::kSearchAndAssistant, Category::kSearchAndAssistant,
       Category::kSearchAndAssistant},
      {-1, -1, -1}, {0.3, 0.5, 0.4});
  auto file_results = MakeListResults({"d"}, {Category::kFiles}, {-1}, {0.2});
  auto app_results = MakeListResults({"e"}, {Category::kApps}, {-1}, {0.1});

  // Simulate starting a search.
  search_controller_->StartSearch(u"abc");

  // Simulate two providers (including Search and Assistant) returning within
  // the burn-in period.
  search_controller_->SetResults(SimpleProvider(Result::kAssistantText),
                                 std::move(search_and_assistant_results));
  search_controller_->SetResults(SimpleProvider(Result::kFileSearch),
                                 std::move(file_results));
  ExpectIdOrder({});

  // Expect results to appear after burn-in period has elapsed. Expect the
  // Search and Assistant category to appear at the bottom.
  WaitInMilliseconds();
  ExpectIdOrder({"d", "b", "c", "a"});

  // Simulate a provider returning results after the burn-in period. Expect the
  // new category to appear below Search and Assistant.
  search_controller_->SetResults(SimpleProvider(Result::kInstalledApp),
                                 std::move(app_results));
  ExpectIdOrder({"d", "b", "c", "a", "e"});
}

// Tests that results are ordered correctly, where results are of a single
// category, and originate from a single provider which returns multiple times
// both pre- and post-burn-in.
TEST_F(
    SearchControllerImplTest,
    ResultsOrderedCorrectly_PreAndPostBurnIn_SingleProviderReturnsMultipleTimes) {
  ranker_manager_->SetCategoryRanks({{Category::kWeb, 0.2}});
  auto web_results_1 = MakeListResults(
      {"b", "c", "a"}, {Category::kWeb, Category::kWeb, Category::kWeb},
      {-1, -1, -1}, {0.2, 0.1, 0.3});

  auto web_results_2 = MakeListResults(
      {"b", "c", "a", "d"},
      {Category::kWeb, Category::kWeb, Category::kWeb, Category::kWeb},
      {-1, -1, -1, -1}, {0.2, 0.1, 0.3, 0.4});

  auto web_results_3 =
      MakeListResults({"b", "c", "a", "d", "e"},
                      {Category::kWeb, Category::kWeb, Category::kWeb,
                       Category::kWeb, Category::kWeb},
                      {-1, -1, -1, -1, -1}, {0.2, 0.1, 0.3, 0.4, 0.5});

  // Simulate starting a search.
  search_controller_->StartSearch(u"abc");

  // Simulate the provider returning results within the burn-in period.
  search_controller_->SetResults(SimpleProvider(Result::kOmnibox),
                                 std::move(web_results_1));
  ExpectIdOrder({});

  // Expect results to appear after burn-in period has elapsed.
  WaitInMilliseconds();
  ExpectIdOrder({"a", "b", "c"});

  // When a single provider returns multiple times for a category, sorting by
  // result burn-in iteration number takes precedence over sorting by result
  // score.
  //
  // Simulate the provider returning results twice after the burn-in period.
  search_controller_->SetResults(SimpleProvider(Result::kOmnibox),
                                 std::move(web_results_2));
  ExpectIdOrder({"a", "b", "c", "d"});
  search_controller_->SetResults(SimpleProvider(Result::kOmnibox),
                                 std::move(web_results_3));
  ExpectIdOrder({"a", "b", "c", "d", "e"});
}

// Tests that results are ordered correctly, where results are of a single
// category, and originate from multiple providers. Providers return a single
// time, pre- or post-burn-in.
TEST_F(
    SearchControllerImplTest,
    ResultsOrderedCorrectly_PreAndPostBurnIn_MultipleProvidersReturnToSingleCategory) {
  ranker_manager_->SetCategoryRanks({{Category::kWeb, 0.2}});

  auto installed_app_results = MakeListResults(
      {"b", "c", "a"}, {Category::kApps, Category::kApps, Category::kApps},
      {-1, -1, -1}, {0.3, 0.2, 0.4});

  auto play_store_app_results = MakeListResults(
      {"e", "d"}, {Category::kApps, Category::kApps}, {-1, -1}, {0.1, 0.5});

  auto internal_app_results =
      MakeListResults({"f"}, {Category::kApps}, {-1}, {0.9});

  // Simulate starting a search.
  search_controller_->StartSearch(u"abc");

  // Simulate a provider returning results within the burn-in period.
  search_controller_->SetResults(SimpleProvider(Result::kInstalledApp),
                                 std::move(installed_app_results));
  ExpectIdOrder({});

  // Expect results to appear after burn-in period has elapsed.
  WaitInMilliseconds();
  ExpectIdOrder({"a", "b", "c"});

  // When there are multiple providers returning for a category, sorting by
  // burn-in iteration number takes precedence over sorting by result score.
  //
  // Simulate two other providers returning results after the burn-in period.
  search_controller_->SetResults(SimpleProvider(Result::kPlayStoreApp),
                                 std::move(play_store_app_results));
  ExpectIdOrder({"a", "b", "c", "d", "e"});
  search_controller_->SetResults(SimpleProvider(Result::kInternalApp),
                                 std::move(internal_app_results));
  ExpectIdOrder({"a", "b", "c", "d", "e", "f"});
}

TEST_F(SearchControllerImplTest, FirstSearchResultsNotShownInSecondSearch) {
  ranker_manager_->SetCategoryRanks({{Category::kApps, 0.1}});

  auto provider = std::make_unique<TestSearchProvider>(Result::kInstalledApp,
                                                       base::Seconds(1));
  auto* provider_ptr = provider.get();
  search_controller_->AddProvider(std::move(provider));

  // Start the first search.
  provider_ptr->SetNextResults(
      MakeListResults({"AAA"}, {Category::kApps}, {-1}, {0.1}));
  search_controller_->StartSearch(u"A");
  ExpectIdOrder({});

  // Provider has returned and the A result should be published.
  WaitInMilliseconds();
  ExpectIdOrder({"AAA"});

  provider_ptr->SetNextResults({});
  search_controller_->ClearSearch();

  // Start the second search.
  provider_ptr->SetNextResults(
      MakeListResults({"BBB"}, {Category::kApps}, {-1}, {0.1}));
  search_controller_->StartSearch(u"B");
  // The B result is not ready yet, and the A result should *not* have been
  // published.
  ExpectIdOrder({});

  // Provider has returned and the B result should be published.
  WaitInMilliseconds();
  ExpectIdOrder({"BBB"});
}

TEST_F(SearchControllerImplTest, ZeroStateResultsNotOverridingBurnIn) {
  ranker_manager_->SetCategoryRanks({{Category::kWeb, 0.2}});
  auto web_results = MakeListResults(
      {"b", "c", "a"}, {Category::kWeb, Category::kWeb, Category::kWeb},
      {-1, -1, -1}, {0.2, 0.1, 0.3});

  auto zero_state_provider = std::make_unique<TestSearchProvider>(
      Result::kZeroStateApp, base::Milliseconds(20));
  zero_state_provider->SetNextResults(MakeResults(
      {"zero"}, {DisplayType::kRecentApps}, {Category::kApps}, {-1}, {0.5}));
  search_controller_->AddProvider(std::move(zero_state_provider));

  // Simluate zero state search.
  search_controller_->StartZeroState(base::DoNothing(), base::Milliseconds(50));

  // Simulate starting a search.
  search_controller_->StartSearch(u"abc");

  // Simulate the provider returning results within the burn-in period.
  search_controller_->SetResults(SimpleProvider(Result::kOmnibox),
                                 std::move(web_results));
  ExpectIdOrder({});

  // Fast-forward time so zero state provider returns results, and zero state
  // timeout fires.
  WaitInMilliseconds(50);

  // The burn-in period has not elapsed, so no results should have been
  // published.
  ExpectIdOrder({});

  // Expect results to appear after burn-in period has elapsed.
  WaitInMilliseconds();
  ExpectIdOrder({"zero", "a", "b", "c"});
}

TEST_F(SearchControllerImplTest, ZeroStateResultsAreBlocked) {
  ranker_manager_->SetCategoryRanks({{Category::kApps, 0.1}});

  // Set up five providers, three provide zero-state results, one of which is
  // very slow.
  auto provider_a = std::make_unique<TestSearchProvider>(Result::kZeroStateApp,
                                                         base::Seconds(1));
  auto provider_b = std::make_unique<TestSearchProvider>(Result::kZeroStateFile,
                                                         base::Seconds(2));
  auto provider_c = std::make_unique<TestSearchProvider>(Result::kOsSettings,
                                                         base::Seconds(1));
  auto provider_d =
      std::make_unique<TestSearchProvider>(Result::kOmnibox, base::Seconds(4));
  auto provider_e = std::make_unique<TestSearchProvider>(
      Result::kZeroStateDrive, base::Seconds(5));

  // NOTE: The particular result categories do not matter, but display type does
  // impact published result sort order.
  provider_a->SetNextResults(MakeResults({"a"}, {DisplayType::kRecentApps},
                                         {Category::kApps}, {-1}, {0.3}));
  provider_b->SetNextResults(MakeResults({"b"}, {DisplayType::kContinue},
                                         {Category::kFiles}, {-1}, {0.2}));
  provider_c->SetNextResults(
      MakeListResults({"c"}, {Category::kApps}, {-1}, {0.1}));
  provider_d->SetNextResults(
      MakeListResults({"d"}, {Category::kApps}, {-1}, {0.4}));
  provider_e->SetNextResults(
      MakeResults({"e", "f"}, {DisplayType::kContinue, DisplayType::kContinue},
                  {Category::kApps, Category::kApps}, {-1, -1}, {0.6, 0.5}));

  search_controller_->AddProvider(std::move(provider_a));
  search_controller_->AddProvider(std::move(provider_b));
  search_controller_->AddProvider(std::move(provider_c));
  search_controller_->AddProvider(std::move(provider_d));
  search_controller_->AddProvider(std::move(provider_e));

  // Start search so non zero state test providers run.
  search_controller_->StartSearch(u"xyz");

  // Start the zero-state session. When on-done is called, we should have
  // results from all but the slowest provider.
  search_controller_->StartZeroState(base::BindLambdaForTesting([&]() {
                                       ExpectIdOrder({"a", "b", "c"});
                                     }),
                                     base::Seconds(3));

  // The fast provider has returned but shouldn't have published.
  WaitInMilliseconds();
  ExpectIdOrder({});

  // Verify results are not published if a non-zero state provider (provider_c)
  // returns results.
  WaitInMilliseconds();
  ExpectIdOrder({});

  // Fast forward time enough for the zero state callback to run.
  WaitInMilliseconds();
  ExpectIdOrder({"a", "b", "c"});

  // At this  point, provider "d" finished, but the results are not published
  // because d provider supplies non-zero state results, and zero state search
  // is in progress - in practice, non-zero state providers should not start
  // during zero state search, but test provider runs either way.
  WaitInMilliseconds();
  ExpectIdOrder({"a", "b", "c"});

  //  The latecomer should still be added when it arrives - note that the list
  //  of ids includes non-zero state result set since the results were last
  //  published.
  //  Note that results "c" and "d" are trailing due to their later burn-in
  //  iteration.
  WaitInMilliseconds(2000);
  ExpectIdOrder({"e", "f", "a", "b", "c", "d"});
}

TEST_F(SearchControllerImplTest, ZeroStateResultsGetTimedOut) {
  ranker_manager_->SetCategoryRanks({{Category::kApps, 0.1}});

  auto provider_a = std::make_unique<TestSearchProvider>(Result::kZeroStateApp,
                                                         base::Seconds(1));
  auto provider_b = std::make_unique<TestSearchProvider>(Result::kZeroStateFile,
                                                         base::Seconds(3));

  provider_a->SetNextResults(
      MakeListResults({"a"}, {Category::kApps}, {-1}, {0.3}));
  provider_b->SetNextResults(
      MakeListResults({"b"}, {Category::kFiles}, {-1}, {0.2}));

  search_controller_->AddProvider(std::move(provider_a));
  search_controller_->AddProvider(std::move(provider_b));

  search_controller_->StartZeroState(
      base::BindLambdaForTesting([&]() { ExpectIdOrder({"a"}); }),
      base::Seconds(2));

  // The fast provider has returned but shouldn't have published.
  WaitInMilliseconds();
  ExpectIdOrder({});

  // The timeout finished, the fast provider's result should be published.
  WaitInMilliseconds();
  ExpectIdOrder({"a"});

  // The slow provider should still publish when it returns.
  WaitInMilliseconds();
  ExpectIdOrder({"a", "b"});
}

TEST_F(SearchControllerImplTest, ContinueRanksDriveAboveLocal) {
  // Use the full ranking stack.
  search_controller_->set_ranker_manager_for_test(
      std::make_unique<RankerManager>(&profile_, search_controller_.get()));

  auto drive_provider = std::make_unique<TestSearchProvider>(
      Result::kZeroStateDrive, base::Seconds(0));
  auto local_provider = std::make_unique<TestSearchProvider>(
      Result::kZeroStateFile, base::Seconds(0));

  drive_provider->SetNextResults(MakeListResults(
      {"drive_a", "drive_b"}, {Category::kUnknown, Category::kUnknown},
      {-1, -1}, {0.2, 0.1}));
  local_provider->SetNextResults(MakeListResults(
      {"local_a", "local_b"}, {Category::kUnknown, Category::kUnknown},
      {-1, -1}, {0.5, 0.4}));

  search_controller_->AddProvider(std::move(local_provider));
  search_controller_->AddProvider(std::move(drive_provider));

  search_controller_->StartZeroState(base::DoNothing(), base::Seconds(1));

  Wait();
  ExpectIdOrder({"drive_a", "drive_b", "local_a", "local_b"});
}

TEST_F(SearchControllerImplTest, FindSearchResultByIdAndOpenIt) {
  auto results_1 = MakeListResults(
      {"a", "b", "c", "d"},
      {Category::kWeb, Category::kGames, Category::kApps, Category::kWeb},
      {0, -1, 1, -1}, {0.4, 0.7, 0.2, 0.8});

  search_controller_->StartSearch(u"abc");
  search_controller_->SetResults(SimpleProvider(Result::kOmnibox),
                                 std::move(results_1));
  WaitInMilliseconds();

  // Return nullptr if result cannot be found.
  ChromeSearchResult* result = search_controller_->FindSearchResult("e");
  EXPECT_EQ(result, nullptr);

  // Return the result with the target id.
  result = search_controller_->FindSearchResult("b");
  EXPECT_EQ(result->id(), "b");
  EXPECT_EQ(result->category(), Category::kGames);

  search_controller_->OpenResult(result, ui::EF_NONE);

  // We expect that |DismissView()| in |list_controller_| to be called.
  EXPECT_TRUE(list_controller_.did_dismiss_view());
}

TEST_F(SearchControllerImplTest, InvokeResult) {
  auto results_1 = MakeListResults(
      {"a", "b", "c", "d"},
      {Category::kWeb, Category::kGames, Category::kApps, Category::kWeb},
      {0, -1, 1, -1}, {0.4, 0.7, 0.2, 0.8});

  search_controller_->StartSearch(u"abc");
  search_controller_->SetResults(SimpleProvider(Result::kOmnibox),
                                 std::move(results_1));
  WaitInMilliseconds();
  ExpectIdOrder({"a", "c", "d", "b"});

  // Return the result with the target id.
  ChromeSearchResult* result = search_controller_->FindSearchResult("b");
  EXPECT_EQ(result->id(), "b");
  EXPECT_EQ(result->category(), Category::kGames);

  // The result should be removed if the invoke action is |kRemove|.
  search_controller_->InvokeResultAction(result,
                                         ash::SearchResultActionType::kRemove);
  WaitInMilliseconds();
  ExpectIdOrder({"a", "c", "d"});
}

TEST_F(SearchControllerImplTest, Train) {
  auto results_1 = MakeListResults(
      {"a", "b", "c", "d"},
      {Category::kWeb, Category::kGames, Category::kApps, Category::kWeb},
      {0, -1, 1, -1}, {0.4, 0.7, 0.2, 0.8});

  search_controller_->StartSearch(u"abc");
  search_controller_->SetResults(SimpleProvider(Result::kOmnibox),
                                 std::move(results_1));
  WaitInMilliseconds();

  search_controller_->Train(CreateFakeLaunchData("e"));
  // We expect that |Train()| in |ranker_manager_| to be called.
  EXPECT_TRUE(ranker_manager_->did_train());
}

TEST_F(SearchControllerImplTest, NotifyObserverWhenPublished) {
  // Create two fake observers.
  FakeObserver observer1;
  FakeObserver observer2;

  search_controller_->Publish();
  WaitInMilliseconds();
  // Do not observe if the observers are not added.
  EXPECT_FALSE(observer1.results_added());
  EXPECT_FALSE(observer2.results_added());

  // Add two observers and remove observer2.
  search_controller_->AddObserver(&observer1);
  search_controller_->AddObserver(&observer2);
  search_controller_->RemoveObserver(&observer2);

  search_controller_->Publish();
  WaitInMilliseconds();
  // We expect observer1 to observe while observer2 is not.
  EXPECT_TRUE(observer1.results_added());
  EXPECT_FALSE(observer2.results_added());
}

}  // namespace app_list::test
