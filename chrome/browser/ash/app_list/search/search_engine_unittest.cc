// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/search_engine.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ash/app_list/search/test/search_controller_test_util.h"
#include "chrome/browser/ash/app_list/search/test/test_search_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace app_list::test {
namespace {
using Result = ash::AppListSearchResultType;
}

class SearchEngineTest : public testing::Test {
 public:
  SearchEngineTest() = default;
  SearchEngineTest(const SearchEngineTest&) = delete;
  SearchEngineTest& operator=(const SearchEngineTest&) = delete;
  ~SearchEngineTest() override = default;

  void SetUp() override {
    search_engine_ = std::make_unique<SearchEngine>(nullptr);
    auto provider_a = std::make_unique<TestSearchProvider>(
        Result::kOsSettings, base::Milliseconds(100),
        SearchCategory::kSettings);
    auto provider_b = std::make_unique<TestSearchProvider>(
        Result::kOmnibox, base::Milliseconds(400), SearchCategory::kOmnibox);

    search_provider_a_ = provider_a.get();
    search_provider_b_ = provider_b.get();

    search_engine_->AddProvider(std::move(provider_a));
    search_engine_->AddProvider(std::move(provider_b));
  }

  void TearDown() override {
    search_provider_a_ = nullptr;
    search_provider_b_ = nullptr;
    search_engine_.reset();
  }

 protected:
  raw_ptr<TestSearchProvider> search_provider_a_;
  raw_ptr<TestSearchProvider> search_provider_b_;
  std::unique_ptr<SearchEngine> search_engine_;
};

TEST_F(SearchEngineTest, StartSearchDefault) {
  base::test::SingleThreadTaskEnvironment task_environment;

  search_provider_a_->SetNextResults(
      MakeListResults({"test"}, {Category::kApps}, {-1}, {0.1}));
  search_provider_b_->SetNextResults(
      MakeListResults({"test1"}, {Category::kApps}, {-1}, {0.4}));

  std::vector<ash::AppListSearchResultType> result_type;
  std::vector<std::vector<std::unique_ptr<ChromeSearchResult>>> results;

  base::RunLoop run_loop;
  int count = 0;
  SearchOptions search_options;
  search_engine_->StartSearch(
      u"test", std::move(search_options),
      base::BindLambdaForTesting(
          [&](ash::AppListSearchResultType _result_type,
              std::vector<std::unique_ptr<ChromeSearchResult>> _results) {
            count += 1;
            result_type.push_back(_result_type);
            results.push_back(std::move(_results));

            if (count == 2) {
              run_loop.Quit();
            }
          }));
  run_loop.Run();

  EXPECT_EQ(results.size(), 2u);
  ASSERT_EQ(result_type[0], Result::kOsSettings);
  ASSERT_EQ(result_type[1], Result::kOmnibox);
  ASSERT_EQ(results[0][0]->id(), "test");
  ASSERT_EQ(results[0][0]->category(), Category::kApps);
  ASSERT_EQ(results[1][0]->id(), "test1");
  ASSERT_EQ(results[1][0]->category(), Category::kApps);
}

TEST_F(SearchEngineTest, StartSearchFilter) {
  base::test::SingleThreadTaskEnvironment task_environment;

  search_provider_a_->SetNextResults(
      MakeListResults({"test"}, {Category::kApps}, {-1}, {0.1}));
  search_provider_b_->SetNextResults(
      MakeListResults({"test1"}, {Category::kApps}, {-1}, {0.4}));

  std::vector<ash::AppListSearchResultType> result_type;
  std::vector<std::vector<std::unique_ptr<ChromeSearchResult>>> results;

  base::RunLoop run_loop;
  int count = 0;
  SearchOptions search_options;
  search_options.search_categories = {SearchCategory::kOmnibox};
  search_engine_->StartSearch(
      u"test", std::move(search_options),
      base::BindLambdaForTesting(
          [&](ash::AppListSearchResultType _result_type,
              std::vector<std::unique_ptr<ChromeSearchResult>> _results) {
            count += 1;
            result_type.push_back(_result_type);
            results.push_back(std::move(_results));

            if (count == 1) {
              run_loop.Quit();
            }
          }));
  run_loop.Run();

  EXPECT_EQ(results.size(), 1u);
  ASSERT_EQ(result_type[0], Result::kOmnibox);
  ASSERT_EQ(results[0][0]->id(), "test1");
  ASSERT_EQ(results[0][0]->category(), Category::kApps);
}

}  // namespace app_list::test
