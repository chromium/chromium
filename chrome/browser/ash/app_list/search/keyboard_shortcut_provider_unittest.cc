// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/keyboard_shortcut_provider.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/webui/shortcut_customization_ui/backend/search/fake_search_data.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
namespace app_list::test {
namespace {

constexpr std::u16string kText = u"fake query";
constexpr std::u16string kIrrelevantText = u"irrelevant";

constexpr double kResultRelevanceThreshold = 0.79;
constexpr size_t kMaxResults = 3u;

using ash::mojom::AcceleratorState;
using ash::shortcut_customization::mojom::SearchResult;
using ash::shortcut_customization::mojom::SearchResultPtr;

std::vector<SearchResultPtr> CreateFakeSearchResultsWithSpecifiedScores(
    const std::vector<double>& scores) {
  std::vector<SearchResultPtr> search_results;
  for (double score : scores) {
    search_results.push_back(SearchResult::New(
        /*accelerator_layout_info=*/CreateFakeAcceleratorLayoutInfo(
            /*description=*/base::StrCat(
                {u"result with score ", base::NumberToString16(score)}),
            /*source=*/ash::mojom::AcceleratorSource::kAsh,
            /*action=*/
            ash::shortcut_ui::fake_search_data::FakeActionIds::kAction1,
            /*style=*/ash::mojom::AcceleratorLayoutStyle::kDefault),
        /*accelerator_infos=*/
        ash::shortcut_ui::fake_search_data::CreateFakeAcceleratorInfoList(),
        /*relevance_score=*/score));
  }

  return search_results;
}

std::vector<SearchResultPtr> CreateFakeSearchResults(int num_relevant,
                                                     int num_irrelevant) {
  std::vector<SearchResultPtr> search_results;
  for (int i = 0; i < num_relevant; i++) {
    search_results.push_back(SearchResult::New(
        /*accelerator_layout_info=*/CreateFakeAcceleratorLayoutInfo(
            /*description=*/kText,
            /*source=*/ash::mojom::AcceleratorSource::kAsh,
            /*action=*/
            ash::shortcut_ui::fake_search_data::FakeActionIds::kAction1,
            /*style=*/ash::mojom::AcceleratorLayoutStyle::kDefault),
        /*accelerator_infos=*/
        ash::shortcut_ui::fake_search_data::CreateFakeAcceleratorInfoList(),
        /*relevance_score=*/kResultRelevanceThreshold));
  }

  for (int i = 0; i < num_irrelevant; i++) {
    search_results.push_back(SearchResult::New(
        /*accelerator_layout_info=*/CreateFakeAcceleratorLayoutInfo(
            /*description=*/kIrrelevantText,
            /*source=*/ash::mojom::AcceleratorSource::kAsh,
            /*action=*/
            ash::shortcut_ui::fake_search_data::FakeActionIds::kAction1,
            /*style=*/ash::mojom::AcceleratorLayoutStyle::kDefault),
        /*accelerator_infos=*/
        ash::shortcut_ui::fake_search_data::CreateFakeAcceleratorInfoList(),
        /*relevance_score=*/kResultRelevanceThreshold));
  }

  return search_results;
}

std::vector<SearchResultPtr> CreateFakeSearchResultsWithSpecifiedStates(
    const std::vector<ash::mojom::AcceleratorState>& states) {
  std::vector<SearchResultPtr> search_results;
  for (const auto state : states) {
    search_results.push_back(SearchResult::New(
        /*accelerator_layout_info=*/CreateFakeAcceleratorLayoutInfo(
            /*description=*/kText,
            /*source=*/ash::mojom::AcceleratorSource::kAsh,
            /*action=*/
            ash::shortcut_ui::fake_search_data::FakeActionIds::kAction1,
            /*style=*/ash::mojom::AcceleratorLayoutStyle::kDefault),
        /*accelerator_infos=*/
        ash::shortcut_ui::fake_search_data::CreateFakeAcceleratorInfoList(
            state),
        /*relevance_score=*/kResultRelevanceThreshold));
  }
  return search_results;
}

}  // namespace
class FakeSearchHandler : public ash::shortcut_ui::SearchHandler {
 public:
  FakeSearchHandler(
      ash::shortcut_ui::SearchConceptRegistry* search_concept_registry,
      ash::local_search_service::LocalSearchServiceProxy*
          local_search_service_proxy)
      : ash::shortcut_ui::SearchHandler(search_concept_registry,
                                        local_search_service_proxy) {}

  void Search(const std::u16string& query,
              uint32_t max_num_results,
              SearchCallback callback) override {
    ASSERT_TRUE(search_result_ != nullptr);
    std::move(callback).Run(std::move(*search_result_));
  }

  void AddSearchResultsAvailabilityObserver(
      mojo::PendingRemote<
          ash::shortcut_customization::mojom::SearchResultsAvailabilityObserver>
          observer) override {
    // No op.
  }

  void SetSearchResults(
      std::vector<ash::shortcut_customization::mojom::SearchResultPtr> result) {
    search_result_ = std::make_unique<
        std::vector<ash::shortcut_customization::mojom::SearchResultPtr>>(
        std::move(result));
  }

 private:
  std::unique_ptr<
      std::vector<ash::shortcut_customization::mojom::SearchResultPtr>>
      search_result_;
};

class CustomizableKeyboardShortcutProviderTest : public ChromeAshTestBase {
 public:
  CustomizableKeyboardShortcutProviderTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kSearchCustomizableShortcutsInLauncher);
  }

 protected:
  void SetUp() override {
    ChromeAshTestBase::SetUp();
    // Initialize search_handler_;
    local_search_service_proxy_ =
        std::make_unique<ash::local_search_service::LocalSearchServiceProxy>(
            /*for_testing=*/true);
    search_concept_registry_ =
        std::make_unique<ash::shortcut_ui::SearchConceptRegistry>(
            *local_search_service_proxy_.get());
    search_handler_ = std::make_unique<FakeSearchHandler>(
        search_concept_registry_.get(), local_search_service_proxy_.get());

    // Initialize provider_;
    profile_ = std::make_unique<TestingProfile>();
    auto provider = std::make_unique<KeyboardShortcutProvider>(profile_.get());
    provider->MaybeInitialize(search_handler_.get());
    provider_ = provider.get();

    // Initialize search_controller_;
    search_controller_ = std::make_unique<TestSearchController>();
    search_controller_->AddProvider(std::move(provider));
  }

  void Wait() { task_environment()->RunUntilIdle(); }

  const SearchProvider::Results& results() {
    return search_controller_->last_results();
  }

  void StartSearch(const std::u16string& query) {
    search_controller_->StartSearch(query);
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<ash::local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_;
  std::unique_ptr<ash::shortcut_ui::SearchConceptRegistry>
      search_concept_registry_;
  std::unique_ptr<FakeSearchHandler> search_handler_;
  std::unique_ptr<Profile> profile_;
  std::unique_ptr<TestSearchController> search_controller_;
  raw_ptr<KeyboardShortcutProvider> provider_ = nullptr;
};

// Test that the relevance scores of the results returned by `search_handler`
// will be overwritten.
TEST_F(CustomizableKeyboardShortcutProviderTest, ResultOverwritten) {
  auto search_results = CreateFakeSearchResultsWithSpecifiedScores(
      {1.0, 0.9, 0.8, 0.7, 0.5, 0.4});
  search_handler_->SetSearchResults(std::move(search_results));

  StartSearch(kText);
  Wait();

  EXPECT_TRUE(results().empty());
}

// Test that when there are more than 3 results whose relevant score exceeds
// the threshold, only return top three.
TEST_F(CustomizableKeyboardShortcutProviderTest, FourQualifiedReturnThree) {
  auto search_results =
      CreateFakeSearchResults(/*num_relevant=*/4, /*num_irrelevant=*/2);
  search_handler_->SetSearchResults(std::move(search_results));

  StartSearch(kText);
  Wait();

  EXPECT_EQ(kMaxResults, results().size());
  for (const auto& result : results()) {
    EXPECT_GT(result->relevance(), kResultRelevanceThreshold);
  }
}

// Test that when there are 3 result but none of them whose relevant score
// exceeds the threshold.
TEST_F(CustomizableKeyboardShortcutProviderTest, NoneQualifiedReturnEmpty) {
  auto search_results =
      CreateFakeSearchResults(/*num_relevant=*/0, /*num_irrelevant=*/3);
  search_handler_->SetSearchResults(std::move(search_results));

  StartSearch(kText);
  Wait();

  EXPECT_TRUE(results().empty());
}

// Test that when there are 4 result but only 2 of them whose relevant score
// exceeds the threshold, only return those two.
TEST_F(CustomizableKeyboardShortcutProviderTest,
       TwoQualifiedTwoNotQualifiedReturnTwo) {
  auto search_results =
      CreateFakeSearchResults(/*num_relevant=*/2, /*num_irrelevant=*/2);
  search_handler_->SetSearchResults(std::move(search_results));

  StartSearch(kText);
  Wait();

  EXPECT_EQ(2u, results().size());
  for (const auto& result : results()) {
    EXPECT_GT(result->relevance(), kResultRelevanceThreshold);
  }
}

// Test that disabled shortcuts are kept. Specifically, a disabled shortcut
// should still appear in the search results with a "No shortcut assigned"
// message.
TEST_F(CustomizableKeyboardShortcutProviderTest,
       DisabledShortcutsWillBeRemoved) {
  auto search_results = CreateFakeSearchResultsWithSpecifiedStates(
      {AcceleratorState::kDisabledByConflict, AcceleratorState::kEnabled,
       AcceleratorState::kDisabledByUser});
  search_handler_->SetSearchResults(std::move(search_results));

  StartSearch(kText);
  Wait();

  EXPECT_EQ(3u, results().size());
}

}  // namespace app_list::test
