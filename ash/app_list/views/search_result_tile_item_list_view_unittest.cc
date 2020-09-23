// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_tile_item_list_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/test/app_list_test_view_delegate.h"
#include "ash/app_list/test/test_search_result.h"
#include "ash/app_list/views/search_result_tile_item_view.h"
#include "ash/app_list/views/search_result_view.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/widget_test.h"

namespace ash {

namespace {
constexpr size_t kInstalledApps = 4;
constexpr size_t kPlayStoreApps = 2;
constexpr size_t kRecommendedApps = 1;

// used to test when multiple chips with specified display
// indexes have been added
constexpr size_t kRecommendedAppsWithDisplayIndex = 3;
}  // namespace

class SearchResultTileItemListViewTest
    : public views::test::WidgetTest,
      public ::testing::WithParamInterface<std::pair<bool, bool>> {
 public:
  SearchResultTileItemListViewTest() = default;
  ~SearchResultTileItemListViewTest() override = default;

  // Overridden from testing::Test:
  void SetUp() override {
    views::test::WidgetTest::SetUp();
    widget_ = CreateTopLevelPlatformWidget();
  }

  void TearDown() override {
    view_.reset();
    widget_->CloseNow();
    views::test::WidgetTest::TearDown();
  }

 protected:
  void CreateSearchResultTileItemListView() {
    std::vector<base::Feature> enabled_features, disabled_features;
    // Zero State affects the UI behavior significantly. This test tests the
    // UI behavior with zero state being disable.
    // TODO(crbug.com/925195): Write new test cases for zero state.
    if (IsReinstallAppRecommendationEnabled()) {
      enabled_features.push_back(
          app_list_features::kEnableAppReinstallZeroState);
    } else {
      disabled_features.push_back(
          app_list_features::kEnableAppReinstallZeroState);
    }

    disabled_features.push_back(app_list_features::kEnableZeroStateSuggestions);

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

    ASSERT_EQ(IsReinstallAppRecommendationEnabled(),
              app_list_features::IsAppReinstallZeroStateEnabled());

    // Sets up the views.
    textfield_ = std::make_unique<views::Textfield>();
    view_ = std::make_unique<SearchResultTileItemListView>(textfield_.get(),
                                                           &view_delegate_);
    widget_->SetBounds(gfx::Rect(0, 0, 300, 200));
    widget_->GetContentsView()->AddChildView(view_.get());
    widget_->Show();
    view_->SetResults(view_delegate_.GetSearchModel()->results());
  }

  bool IsReinstallAppRecommendationEnabled() const { return GetParam().first; }

  SearchResultTileItemListView* view() { return view_.get(); }

  SearchModel::SearchResults* GetResults() {
    return view_delegate_.GetSearchModel()->results();
  }

  void SetUpSearchResults() {
    SearchModel::SearchResults* results = GetResults();

    // Populate results for installed applications.
    for (size_t i = 0; i < kInstalledApps; ++i) {
      std::unique_ptr<TestSearchResult> result =
          std::make_unique<TestSearchResult>();
      result->set_result_id("InstalledApp " + base::NumberToString(i));
      result->set_display_type(SearchResultDisplayType::kTile);
      result->set_result_type(AppListSearchResultType::kInstalledApp);
      result->set_title(base::ASCIIToUTF16("InstalledApp ") +
                        base::NumberToString16(i));
      results->Add(std::move(result));
    }

    // Populate results for Play Store search applications.
    for (size_t i = 0; i < kPlayStoreApps; ++i) {
      std::unique_ptr<TestSearchResult> result =
          std::make_unique<TestSearchResult>();
      result->set_result_id("PlayStoreApp " + base::NumberToString(i));
      result->set_display_type(SearchResultDisplayType::kTile);
      result->set_result_type(AppListSearchResultType::kPlayStoreApp);
      result->set_title(base::ASCIIToUTF16("PlayStoreApp ") +
                        base::NumberToString16(i));
      result->SetRating(1 + i);
      result->SetFormattedPrice(base::ASCIIToUTF16("Price ") +
                                base::NumberToString16(i));
      results->Add(std::move(result));
    }

    if (IsReinstallAppRecommendationEnabled()) {
      for (size_t i = 0; i < kRecommendedApps; ++i) {
        std::unique_ptr<TestSearchResult> result =
            std::make_unique<TestSearchResult>();
        result->set_result_id("RecommendedApp " + base::NumberToString(i));
        result->set_display_type(SearchResultDisplayType::kTile);
        result->set_is_recommendation(true);
        result->set_result_type(
            AppListSearchResultType::kPlayStoreReinstallApp);
        result->set_display_index(SearchResultDisplayIndex::kSixthIndex);
        result->set_title(base::ASCIIToUTF16("RecommendedApp ") +
                          base::NumberToString16(i));
        result->SetRating(1 + i);
        results->Add(std::move(result));
      }
    }

    // Adding results calls SearchResultContainerView::ScheduleUpdate().
    // It will post a delayed task to update the results and relayout.
    RunPendingMessages();
  }

  void SetUpSearchResultsWithMultipleDisplayIndexesRequested() {
    SearchModel::SearchResults* results = GetResults();

    // Populate results for installed applications.
    for (size_t i = 0; i < kInstalledApps; ++i) {
      std::unique_ptr<TestSearchResult> result =
          std::make_unique<TestSearchResult>();
      result->set_result_id("InstalledApp " + base::NumberToString(i));
      result->set_display_type(SearchResultDisplayType::kTile);
      result->set_result_type(AppListSearchResultType::kInstalledApp);
      result->set_title(base::ASCIIToUTF16("InstalledApp ") +
                        base::NumberToString16(i));
      results->Add(std::move(result));
    }

    // Populate results for Play Store search applications.
    for (size_t i = 0; i < kPlayStoreApps; ++i) {
      std::unique_ptr<TestSearchResult> result =
          std::make_unique<TestSearchResult>();
      result->set_result_id("PlayStoreApp " + base::NumberToString(i));
      result->set_display_type(SearchResultDisplayType::kTile);
      result->set_result_type(AppListSearchResultType::kPlayStoreApp);
      result->set_title(base::ASCIIToUTF16("PlayStoreApp ") +
                        base::NumberToString16(i));
      result->SetRating(1 + i);
      result->SetFormattedPrice(base::ASCIIToUTF16("Price ") +
                                base::NumberToString16(i));
      results->Add(std::move(result));
    }

    const SearchResultDisplayIndex
        display_indexes[kRecommendedAppsWithDisplayIndex] = {
            SearchResultDisplayIndex::kFourthIndex,
            SearchResultDisplayIndex::kFifthIndex,
            SearchResultDisplayIndex::kSixthIndex,
        };

    if (IsReinstallAppRecommendationEnabled()) {
      for (size_t i = 0; i < kRecommendedAppsWithDisplayIndex; ++i) {
        std::unique_ptr<TestSearchResult> result =
            std::make_unique<TestSearchResult>();
        result->set_result_id("RecommendedApp " + base::NumberToString(i));
        result->set_display_type(SearchResultDisplayType::kTile);
        result->set_is_recommendation(true);
        result->set_result_type(
            AppListSearchResultType::kPlayStoreReinstallApp);
        result->set_display_index(display_indexes[i]);
        result->set_title(base::ASCIIToUTF16("RecommendedApp ") +
                          base::NumberToString16(i));
        result->SetRating(1 + i);
        results->AddAt(result->display_index(), std::move(result));
      }
    }

    // Adding results calls SearchResultContainerView::ScheduleUpdate().
    // It will post a delayed task to update the results and relayout.
    RunPendingMessages();
  }

  size_t GetOpenResultCount(int ranking) {
    return view_delegate_.open_search_result_counts()[ranking];
  }

  void ResetOpenResultCount() {
    view_delegate_.open_search_result_counts().clear();
  }

  size_t GetResultCount() const { return view_->num_results(); }

 private:
  test::AppListTestViewDelegate view_delegate_;
  std::unique_ptr<SearchResultTileItemListView> view_;
  views::Widget* widget_;
  std::unique_ptr<views::Textfield> textfield_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultTileItemListViewTest);
};

TEST_P(SearchResultTileItemListViewTest, Basic) {
  CreateSearchResultTileItemListView();
  SetUpSearchResults();

  const size_t results = GetResultCount();
  size_t expected_results = kInstalledApps;
  expected_results += kPlayStoreApps;

  if (IsReinstallAppRecommendationEnabled()) {
    expected_results += kRecommendedApps;
  }
  constexpr size_t kMaxNumSearchResultTiles = 6;
  expected_results = std::min(kMaxNumSearchResultTiles, expected_results);

  ASSERT_EQ(expected_results, results);

  // When the Play Store app search or app reinstallation results are
  // present, for each result, we added a separator for result type grouping.
  const size_t child_step = 2;
  const size_t expected_num_children = kMaxNumSearchResultTiles * child_step;
  EXPECT_EQ(expected_num_children, view()->children().size());

  /// Test accessibility descriptions of tile views.
  const size_t first_child = child_step - 1;
  for (size_t i = 0; i < kInstalledApps; ++i) {
    ui::AXNodeData node_data;
    view()->children()[first_child + i * child_step]->GetAccessibleNodeData(
        &node_data);
    EXPECT_EQ(ax::mojom::Role::kListBoxOption, node_data.role);
    EXPECT_EQ(l10n_util::GetStringFUTF8(
                  IDS_APP_ACCESSIBILITY_INSTALLED_APP_ANNOUNCEMENT,
                  base::UTF8ToUTF16("InstalledApp " + base::NumberToString(i))),
              node_data.GetStringAttribute(ax::mojom::StringAttribute::kName));
  }

  const size_t expected_install_apps =
      expected_results -
      (IsReinstallAppRecommendationEnabled() ? kRecommendedApps : 0) -
      kInstalledApps;
  for (size_t i = 0; i < expected_install_apps; ++i) {
    ui::AXNodeData node_data;
    view()
        ->children()[first_child + (i + kInstalledApps) * child_step]
        ->GetAccessibleNodeData(&node_data);
    EXPECT_EQ(ax::mojom::Role::kListBoxOption, node_data.role);
    EXPECT_EQ(
        l10n_util::GetStringFUTF8(
            IDS_APP_ACCESSIBILITY_ARC_APP_ANNOUNCEMENT,
            base::UTF8ToUTF16("PlayStoreApp " + base::NumberToString(i))) +
            ", Star rating " + base::NumberToString(i + 1) + ".0, Price " +
            base::NumberToString(i),
        node_data.GetStringAttribute(ax::mojom::StringAttribute::kName));
  }

  // Recommendations.
  const size_t start_index = kInstalledApps + expected_install_apps;
  for (size_t i = 0; i < expected_results - start_index; ++i) {
    ui::AXNodeData node_data;
    view()
        ->children()[first_child + (i + start_index) * child_step]
        ->GetAccessibleNodeData(&node_data);
    EXPECT_EQ(ax::mojom::Role::kListBoxOption, node_data.role);
    EXPECT_EQ(
        l10n_util::GetStringFUTF8(
            IDS_APP_ACCESSIBILITY_APP_RECOMMENDATION_ARC,
            base::UTF8ToUTF16("RecommendedApp " + base::NumberToString(i))) +
            ", Star rating " + base::NumberToString(i + 1) + ".0",
        node_data.GetStringAttribute(ax::mojom::StringAttribute::kName));
  }

  ResetOpenResultCount();
  for (size_t i = 0; i < results; ++i) {
    ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, ui::EF_NONE);
    for (size_t j = 0; j <= i; ++j)
      view()->tile_views_for_test()[i]->OnKeyEvent(&event);
    // When app reinstalls is enabled, we actually instantiate 7 results,
    // but only show 6. So we have to look, for exactly 1 result, a "skip"
    // ahead for the reinstall result.
    if (IsReinstallAppRecommendationEnabled() && i == (results - 1)) {
      EXPECT_EQ(i + 1, GetOpenResultCount(i + 1));
    } else {
      EXPECT_EQ(i + 1, GetOpenResultCount(i));
    }
  }
}

// Tests that when multiple apps with specified indexes are added to the app
// results list, they are found at the indexes they requested.
TEST_P(SearchResultTileItemListViewTest, TestRecommendations) {
  if (!IsReinstallAppRecommendationEnabled())
    return;

  CreateSearchResultTileItemListView();
  SetUpSearchResultsWithMultipleDisplayIndexesRequested();

  const size_t child_step = 2;

  size_t first_index = kInstalledApps + kRecommendedAppsWithDisplayIndex;

  size_t stepper = 3;
  for (size_t i = 0; i < stepper; ++i) {
    ui::AXNodeData node_data;
    view()->children()[first_index + i * child_step]->GetAccessibleNodeData(
        &node_data);
    EXPECT_EQ(ax::mojom::Role::kListBoxOption, node_data.role);
    EXPECT_EQ(
        l10n_util::GetStringFUTF8(
            IDS_APP_ACCESSIBILITY_APP_RECOMMENDATION_ARC,
            base::UTF8ToUTF16("RecommendedApp " + base::NumberToString(i))) +
            ", Star rating " + base::NumberToString(i + 1) + ".0",
        node_data.GetStringAttribute(ax::mojom::StringAttribute::kName));
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         SearchResultTileItemListViewTest,
                         testing::ValuesIn({std::make_pair(false, false),
                                            std::make_pair(false, true),
                                            std::make_pair(true, false),
                                            std::make_pair(true, true)}));

}  // namespace ash
