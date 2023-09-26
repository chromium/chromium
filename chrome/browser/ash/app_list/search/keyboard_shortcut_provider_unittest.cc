// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/keyboard_shortcut_provider.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/shortcut_customization_ui/backend/search/fake_search_data.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager_test_api.h"

namespace app_list::test {

namespace {
constexpr double kResultRelevanceThreshold = 0.79;
// Threshold used by new shortcuts search.
constexpr double kRelevanceScoreThreshold = 0.52;
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

std::vector<SearchResultPtr> CreateFakeSearchResultsWithSpecifiedStates(
    const std::vector<ash::mojom::AcceleratorState>& states) {
  std::vector<SearchResultPtr> search_results;
  for (const auto state : states) {
    search_results.push_back(SearchResult::New(
        /*accelerator_layout_info=*/CreateFakeAcceleratorLayoutInfo(
            /*description=*/base::StrCat(
                {u"result with score ",
                 base::NumberToString16(kRelevanceScoreThreshold)}),
            /*source=*/ash::mojom::AcceleratorSource::kAsh,
            /*action=*/
            ash::shortcut_ui::fake_search_data::FakeActionIds::kAction1,
            /*style=*/ash::mojom::AcceleratorLayoutStyle::kDefault),
        /*accelerator_infos=*/
        ash::shortcut_ui::fake_search_data::CreateFakeAcceleratorInfoList(
            state),
        /*relevance_score=*/kRelevanceScoreThreshold));
  }

  return search_results;
}

}  // namespace

class KeyboardShortcutProviderTest : public ChromeAshTestBase,
                                     public testing::WithParamInterface<bool> {
 public:
  KeyboardShortcutProviderTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          search_features::kLauncherFuzzyMatchAcrossProviders);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          search_features::kLauncherFuzzyMatchAcrossProviders);
    }
  }

 protected:
  void SetUp() override {
    ChromeAshTestBase::SetUp();
    // A DCHECK inside a KSV metadata utility function relies on device lists
    // being complete.
    ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

    profile_ = std::make_unique<TestingProfile>();
    search_controller_ = std::make_unique<TestSearchController>();
    auto provider = std::make_unique<KeyboardShortcutProvider>(profile_.get());
    provider_ = provider.get();
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
  std::unique_ptr<Profile> profile_;
  std::unique_ptr<TestSearchController> search_controller_;
  raw_ptr<KeyboardShortcutProvider, ExperimentalAsh> provider_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(FuzzyMatchForProviders,
                         KeyboardShortcutProviderTest,
                         testing::Bool());

// Make search queries which yield shortcut results with shortcut key
// combinations of differing length and format. Check that the top result has a
// high relevance score, and correctly set title and accessible name.
TEST_P(KeyboardShortcutProviderTest, Search) {
  Wait();

  // Result format: Single Key
  StartSearch(u"overview mode");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results()[0]->title(), u"Overview mode");
  if (GetParam()) {
    EXPECT_EQ(results()[0]->relevance(), 1.0);

  } else {
    EXPECT_GT(results()[0]->relevance(), kResultRelevanceThreshold);
  }
  EXPECT_EQ(results()[0]->accessible_name(),
            u"Overview mode, Shortcuts, Overview mode key");

  // Result format: Modifier + Key
  StartSearch(u"lock");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results()[0]->title(), u"Lock screen");
  EXPECT_GT(results()[0]->relevance(), kResultRelevanceThreshold);
  EXPECT_EQ(results()[0]->accessible_name(),
            u"Lock screen, Shortcuts, Launcher+ l");

  // Result format: Modifier1 + Modifier2 + Key
  StartSearch(u"previous tab");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results()[0]->title(), u"Go to previous tab");
  EXPECT_GT(results()[0]->relevance(), kResultRelevanceThreshold);
  EXPECT_EQ(results()[0]->accessible_name(),
            u"Go to previous tab, Shortcuts, Ctrl+ Shift+ Tab");

  // Result format: Modifier1 + Key1 or Modifier2 + Key2
  StartSearch(u"focus address");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results()[0]->title(), u"Focus address bar");
  EXPECT_GT(results()[0]->relevance(), kResultRelevanceThreshold);
  EXPECT_EQ(results()[0]->accessible_name(),
            u"Focus address bar, Shortcuts, Ctrl+ l or Alt+ d");

  // Result format: Custom template string which embeds a Modifier and a Key.
  StartSearch(u"switch quickly between windows");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results()[0]->title(), u"Switch quickly between windows");
  EXPECT_GT(results()[0]->relevance(), kResultRelevanceThreshold);
  EXPECT_EQ(
      results()[0]->accessible_name(),
      u"Switch quickly between windows, Shortcuts, Press and hold Alt, tap Tab "
      u"until you get to the window you want to open, then release.");

  // Result format: Special case result for Take screenshot/recording.
  StartSearch(u"take screenshot");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results()[0]->title(), u"Take screenshot/recording");
  EXPECT_GT(results()[0]->relevance(), kResultRelevanceThreshold);
  EXPECT_EQ(results()[0]->accessible_name(),
            u"Take screenshot/recording, Shortcuts, Capture mode key or Ctrl+ "
            u"Shift+ Overview mode key");

  // Result format: Order variation result for Dim keyboard.
  StartSearch(u"keyboard dim");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results()[0]->title(),
            u"Dim keyboard (for backlit keyboards only)");
  EXPECT_GT(results()[0]->relevance(), kResultRelevanceThreshold);
  EXPECT_EQ(results()[0]->accessible_name(),
            u"Dim keyboard (for backlit keyboards only), Shortcuts, Alt+ "
            u"BrightnessDown");

  // Result format: special case result for Open emoji picker.
  StartSearch(u"emoji");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results()[0]->title(), u"Open Emoji Picker");
  EXPECT_GT(results()[0]->relevance(), kResultRelevanceThreshold);
  EXPECT_EQ(results()[0]->accessible_name(),
            u"Open Emoji Picker, Shortcuts, Shift+ Launcher+ Space");
}

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
    provider_ = provider.get();
    provider_->SetSearchHandlerForTesting(search_handler_.get());

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
  raw_ptr<KeyboardShortcutProvider, ExperimentalAsh> provider_ = nullptr;
};

// Test that when there are more than 3 results whose relevant score exceeds the
// threshold, only return top three.
TEST_F(CustomizableKeyboardShortcutProviderTest, FourQualifiedReturnThree) {
  auto search_results = CreateFakeSearchResultsWithSpecifiedScores(
      {0.9, 0.8, 0.7, 0.53, 0.5, 0.4});
  search_handler_->SetSearchResults(std::move(search_results));

  provider_->Start(u"fake query");
  Wait();

  EXPECT_EQ(kMaxResults, results().size());
  for (const auto& result : results()) {
    EXPECT_GT(result->relevance(), kRelevanceScoreThreshold);
  }
}

// Test that when there are 3 result but none of them whose relevant score
// exceeds the threshold.
TEST_F(CustomizableKeyboardShortcutProviderTest, NoneQualifiedReturnEmpty) {
  auto search_results =
      CreateFakeSearchResultsWithSpecifiedScores({0.51, 0.51, 0.5});
  search_handler_->SetSearchResults(std::move(search_results));

  provider_->Start(u"fake query");
  Wait();

  EXPECT_TRUE(results().empty());
}

// Test that when there are 4 result but only 2 of them whose relevant score
// exceeds the threshold, only return those two.
TEST_F(CustomizableKeyboardShortcutProviderTest,
       TwoQualifiedTwoNotQualifiedReturnTwo) {
  auto search_results =
      CreateFakeSearchResultsWithSpecifiedScores({0.9, 0.8, 0.51, 0.51, 0.5});
  search_handler_->SetSearchResults(std::move(search_results));

  provider_->Start(u"fake query");
  Wait();

  const size_t results_count = 2;
  EXPECT_EQ(results_count, results().size());
  for (const auto& result : results()) {
    EXPECT_GT(result->relevance(), kRelevanceScoreThreshold);
  }
}

// Test that disabled shortcuts will be filtered out.
TEST_F(CustomizableKeyboardShortcutProviderTest,
       DisabledShortcutsWillBeRemoved) {
  auto search_results = CreateFakeSearchResultsWithSpecifiedStates(
      {AcceleratorState::kDisabledByConflict,
       AcceleratorState::kDisabledByConflict,
       AcceleratorState::kDisabledByUnavailableKeys,
       AcceleratorState::kDisabledByUser});
  search_handler_->SetSearchResults(std::move(search_results));

  provider_->Start(u"fake query");
  Wait();

  const size_t results_count = 0;
  EXPECT_EQ(results_count, results().size());
}

// Test that enabled shortcuts are kept.
TEST_F(CustomizableKeyboardShortcutProviderTest,
       EnabledShortcutsWillBeKeptUpToThree) {
  auto search_results = CreateFakeSearchResultsWithSpecifiedStates(
      {AcceleratorState::kEnabled, AcceleratorState::kEnabled,
       AcceleratorState::kEnabled, AcceleratorState::kEnabled});
  search_handler_->SetSearchResults(std::move(search_results));

  provider_->Start(u"fake query");
  Wait();

  const size_t results_count = 3;
  EXPECT_EQ(results_count, results().size());
}

}  // namespace app_list::test
