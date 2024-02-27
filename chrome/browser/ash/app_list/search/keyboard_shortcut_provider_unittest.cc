// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/keyboard_shortcut_provider.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/shortcut_viewer/strings/grit/shortcut_viewer_strings.h"
#include "ash/webui/shortcut_customization_ui/backend/search/fake_search_data.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/keyboard_shortcut_data.h"
#include "chrome/browser/ash/app_list/search/manatee/manatee_cache.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/ash/app_list/search/test/test_manatee_cache.h"
#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"
#include "chrome/browser/browser_process.h"
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
constexpr double kResultRelevanceManateeThreshold = 0.75;

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

// TODO(longbowei): Remove KeyboardShortcutProviderTest when deprecating old
// shortcut app.
class KeyboardShortcutProviderFuzzyMatchTest
    : public ChromeAshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  KeyboardShortcutProviderFuzzyMatchTest() {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{search_features::
                                    kLauncherFuzzyMatchAcrossProviders},
          /*disabled_features=*/{
              ash::features::kSearchCustomizableShortcutsInLauncher});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{
              search_features::kLauncherFuzzyMatchAcrossProviders,
              ash::features::kSearchCustomizableShortcutsInLauncher});
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
    std::unique_ptr<TestManateeCache> test_manatee_cache_ =
        std::make_unique<TestManateeCache>();
    auto provider = std::make_unique<KeyboardShortcutProvider>(
        profile_.get(), std::move(test_manatee_cache_));
    provider_ = provider.get();
    search_controller_->AddProvider(std::move(provider));

    // TODO(b/326514738): bypassed the filtering in the unit test.
    provider_->set_should_apply_query_filtering_for_test(false);
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
  raw_ptr<KeyboardShortcutProvider> provider_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(FuzzyMatchForProviders,
                         KeyboardShortcutProviderFuzzyMatchTest,
                         testing::Bool());

// Make search queries which yield shortcut results with shortcut key
// combinations of differing length and format. Check that the top result has a
// high relevance score, and correctly set title and accessible name.
TEST_P(KeyboardShortcutProviderFuzzyMatchTest, Search) {
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

// Parameterized by feature kLauncherManateeForKeyboardShortcuts.
class KeyboardShortcutProviderManateeTest
    : public ChromeAshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  KeyboardShortcutProviderManateeTest() {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{search_features::
                                    kLauncherManateeForKeyboardShortcuts},
          /*disabled_features=*/{
              ash::features::kSearchCustomizableShortcutsInLauncher});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{
              search_features::kLauncherManateeForKeyboardShortcuts,
              ash::features::kSearchCustomizableShortcutsInLauncher});
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
    auto test_manatee_cache = std::make_unique<TestManateeCache>();
    test_manatee_cache_ = test_manatee_cache.get();
    // Values are arbitrary and used to avoid making a call to model.
    embeddings_ = {{0.1, 0.2, 0.3}, {0.4, 0.5, 0.6}, {0.7, 0.8, 0.9}};
    test_manatee_cache_->SetResponseForTest(embeddings_);
    auto provider = std::make_unique<KeyboardShortcutProvider>(
        profile_.get(), std::move(test_manatee_cache));
    provider_ = provider.get();
    search_controller_->AddProvider(std::move(provider));
    test_shortcut_data_ = {
        KeyboardShortcutData(u"Open the link in a new tab",
                             IDS_KSV_DESCRIPTION_DRAG_LINK_IN_NEW_TAB,
                             IDS_KSV_SHORTCUT_DRAG_LINK_IN_NEW_TAB),
        KeyboardShortcutData(u"Open the link in the tab",
                             IDS_KSV_DESCRIPTION_DRAG_LINK_IN_SAME_TAB,
                             IDS_KSV_SHORTCUT_DRAG_LINK_IN_SAME_TAB),
        KeyboardShortcutData(u"Highlight the next item on your shelf",
                             IDS_KSV_DESCRIPTION_HIGHLIGHT_NEXT_ITEM_ON_SHELF,
                             IDS_KSV_SHORTCUT_HIGHLIGHT_NEXT_ITEM_ON_SHELF)};
    provider_->set_shortcut_data_for_test(test_shortcut_data_);
    // TODO(b/326514738): bypassed the filtering in the unit test.
    provider_->set_should_apply_query_filtering_for_test(false);
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
  raw_ptr<KeyboardShortcutProvider> provider_ = nullptr;
  raw_ptr<TestManateeCache> test_manatee_cache_;
  EmbeddingsList embeddings_;
  std::vector<KeyboardShortcutData> test_shortcut_data_;
};

INSTANTIATE_TEST_SUITE_P(ManateeForProviders,
                         KeyboardShortcutProviderManateeTest,
                         testing::Bool());

TEST_P(KeyboardShortcutProviderManateeTest, EmbeddingsSet) {
  Wait();
  StartSearch(u"example query");
  Wait();

  if (GetParam()) {
    std::vector<KeyboardShortcutData> shortcut_data_list =
        provider_->shortcut_data();
    for (size_t i = 0; i < shortcut_data_list.size(); ++i) {
      const auto& data_item = shortcut_data_list[i];

      // Returned embeddings are mocked for testing and content is not
      // important.
      ASSERT_FALSE(data_item.embedding().empty());
      ASSERT_EQ(data_item.embedding().size(), 3u);
      ASSERT_EQ(data_item.embedding(), embeddings_[i]);
    }
  } else {
    for (const auto& data_item : provider_->shortcut_data()) {
      ASSERT_TRUE(data_item.embedding().empty());
    }
  }
}

TEST_P(KeyboardShortcutProviderManateeTest, ManateeSearch) {
  // Initial query to set the embeddings will use fuzzy match.
  Wait();
  StartSearch(u"example query");
  Wait();

  test_manatee_cache_->SetResponseForTest({{0.1, 0.2, 0.3}});

  // Second query to use Manatee search.
  Wait();
  StartSearch(u"example query");
  Wait();

  if (GetParam()) {
    ASSERT_FALSE(results().empty());
    EXPECT_EQ(results()[0]->title(), u"Open the link in a new tab");
    EXPECT_GT(results()[0]->relevance(), kResultRelevanceManateeThreshold);
    EXPECT_EQ(results()[0]->relevance(), 1.0);
  } else {
    ASSERT_TRUE(results().empty());
  }
}

// System will default back to fuzzy-match when response from the model is
// an invalid length.
TEST_P(KeyboardShortcutProviderManateeTest, InvalidResponseLength) {
  // Initial query to set the embeddings will use fuzzy match.
  Wait();
  StartSearch(u"example query");
  Wait();

  test_manatee_cache_->SetResponseForTest({});

  // Second query to use Manatee search.
  Wait();
  StartSearch(u"Open the link in a new tab");
  Wait();

  if (GetParam()) {
    ASSERT_FALSE(results().empty());
    EXPECT_EQ(results()[0]->title(), u"Open the link in a new tab");
    EXPECT_GT(results()[0]->relevance(), kResultRelevanceManateeThreshold);
  } else {
    ASSERT_FALSE(results().empty());
    EXPECT_EQ(results()[0]->title(), u"Open the link in a new tab");
  }
}

TEST_P(KeyboardShortcutProviderManateeTest, MultipleQueries) {
  // Initial query to set the embeddings will use fuzzy match.
  Wait();
  StartSearch(u"example query");
  Wait();

  test_manatee_cache_->SetResponseForTest({{0.1, 0.2, 0.3}});

  // Following queries to use Manatee search.
  Wait();
  StartSearch(u"example query");
  Wait();

  if (GetParam()) {
    ASSERT_FALSE(results().empty());
    EXPECT_EQ(results()[0]->title(), u"Open the link in a new tab");
    EXPECT_GT(results()[0]->relevance(), kResultRelevanceManateeThreshold);
  } else {
    ASSERT_TRUE(results().empty());
  }

  test_manatee_cache_->SetResponseForTest({{0.4, 0.5, 0.6}});

  // Following queries to use Manatee search.
  Wait();
  StartSearch(u"example query");
  Wait();

  if (GetParam()) {
    ASSERT_FALSE(results().empty());
    EXPECT_EQ(results()[0]->title(), u"Open the link in the tab");
    EXPECT_GT(results()[0]->relevance(), kResultRelevanceManateeThreshold);
  } else {
    ASSERT_TRUE(results().empty());
  }

  test_manatee_cache_->SetResponseForTest({{0.7, 0.8, 0.9}});

  Wait();
  StartSearch(u"example query");
  Wait();

  if (GetParam()) {
    ASSERT_FALSE(results().empty());
    EXPECT_EQ(results()[0]->title(), u"Highlight the next item on your shelf");
    EXPECT_GT(results()[0]->relevance(), kResultRelevanceManateeThreshold);
  } else {
    ASSERT_TRUE(results().empty());
  }
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
    std::unique_ptr<TestManateeCache> test_manatee_cache_ =
        std::make_unique<TestManateeCache>();
    auto provider = std::make_unique<KeyboardShortcutProvider>(
        profile_.get(), std::move(test_manatee_cache_));
    provider_ = provider.get();
    provider_->SetSearchHandlerForTesting(search_handler_.get());

    // Initialize search_controller_;
    search_controller_ = std::make_unique<TestSearchController>();
    search_controller_->AddProvider(std::move(provider));

    // TODO(b/326514738): bypassed the filtering in the unit test.
    provider_->set_should_apply_query_filtering_for_test(false);
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

// Test that when there are more than 3 results whose relevant score exceeds
// the threshold, only return top three.
TEST_F(CustomizableKeyboardShortcutProviderTest, FourQualifiedReturnThree) {
  auto search_results = CreateFakeSearchResultsWithSpecifiedScores(
      {0.9, 0.8, 0.7, 0.53, 0.5, 0.4});
  search_handler_->SetSearchResults(std::move(search_results));

  StartSearch(u"fake query");
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

  StartSearch(u"fake query");
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

  StartSearch(u"fake query");
  Wait();

  const size_t results_count = 2;
  EXPECT_EQ(results_count, results().size());
  for (const auto& result : results()) {
    EXPECT_GT(result->relevance(), kRelevanceScoreThreshold);
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

  StartSearch(u"fake query");
  Wait();

  const size_t results_count = 3;
  EXPECT_EQ(results_count, results().size());
}

}  // namespace app_list::test
