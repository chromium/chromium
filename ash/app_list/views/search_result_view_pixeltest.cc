// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_bubble_search_page.h"
#include "ash/app_list/views/app_list_search_view.h"
#include "ash/app_list/views/result_selection_controller.h"
#include "ash/app_list/views/search_result_container_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/app_list/views/search_result_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view_utils.h"

namespace ash {

class SearchResultViewPixelTest
    : public AshTestBase,
      public testing::WithParamInterface<
          std::tuple</*use_rtl=*/bool, /*use_tablet_mode=*/bool>> {
 public:
  SearchResultViewPixelTest()
      : use_rtl_(std::get<0>(GetParam())),
        use_tablet_mode_(std::get<1>(GetParam())) {}

  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams init_params;
    init_params.under_rtl = use_rtl();
    return init_params;
  }

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kCrosWebAppShortcutUiUpdate,
         features::kSeparateWebAppShortcutBadgeIcon},
        {});
    AshTestBase::SetUp();

    if (use_tablet_mode_) {
      ash::TabletModeControllerTestApi().EnterTabletMode();
    }
  }

  bool use_rtl() const { return use_rtl_; }

  bool use_tablet_mode() const { return use_tablet_mode_; }

  AppListSearchView* GetSearchView() {
    if (use_tablet_mode()) {
      return GetAppListTestHelper()
          ->GetFullscreenSearchResultPageView()
          ->search_view();
    }
    return GetAppListTestHelper()->GetBubbleAppListSearchView();
  }

  bool IsWebAppShortcutStyle(SearchResultView* view) {
    return view->use_webapp_shortcut_style_;
  }

  bool IsBadgeIconViewVisible(SearchResultView* view) {
    return view->badge_icon_view_->GetVisible();
  }

  void SetUpWebAppSearchResult(SearchModel::SearchResults* results,
                               int init_id,
                               int display_score,
                               bool best_match) {
    std::unique_ptr<TestSearchResult> result =
        std::make_unique<TestSearchResult>();
    result->set_result_id(base::NumberToString(init_id));
    result->set_display_type(ash::SearchResultDisplayType::kList);
    result->SetTitle(
        base::UTF8ToUTF16(base::StringPrintf("Result %d", init_id)));
    result->set_display_score(display_score);
    result->SetDetails(u"Detail");
    result->set_best_match(best_match);
    result->set_category(AppListSearchResultCategory::kAppShortcuts);
    result->set_result_type(AppListSearchResultType::kAppShortcutV2);
    result->SetIconAndBadgeIcon();
    results->Add(std::move(result));
  }

  std::vector<raw_ptr<SearchResultContainerView, VectorExperimental>>
  GetAppShortcutResultContainers() {
    std::vector<raw_ptr<SearchResultContainerView, VectorExperimental>>
        app_result_containers = {};
    for (const auto& result_container :
         GetSearchView()->result_container_views_for_test()) {
      if (result_container->GetFirstResultView() &&
          result_container->GetFirstResultView()->result()->result_type() ==
              SearchResult::ResultType::kAppShortcutV2) {
        app_result_containers.push_back(result_container);
      }
    }
    return app_result_containers;
  }

 private:
  const bool use_rtl_;
  const bool use_tablet_mode_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SearchResultViewPixelTest,
                         testing::Combine(
                             /*use_rtl=*/testing::Bool(),
                             /*use_tablet_mode=*/testing::Bool()));

TEST_P(SearchResultViewPixelTest, WebAppShortcutIconEffectsExists) {
  auto* test_helper = GetAppListTestHelper();
  test_helper->ShowAppList();

  PressAndReleaseKey(ui::VKEY_A);
  SearchModel::SearchResults* results = test_helper->GetSearchResults();

  // Create categorized results and order categories as kApps.
  std::vector<ash::AppListSearchResultCategory>* ordered_categories =
      test_helper->GetOrderedResultCategories();
  ordered_categories->push_back(
      ash::AppListSearchResultCategory::kAppShortcuts);
  SetUpWebAppSearchResult(results, 1, 1, false);

  // Verify that search containers have a scheduled update, and ensure they get
  // run.
  std::vector<raw_ptr<SearchResultContainerView, VectorExperimental>>
      result_containers = GetSearchView()->result_container_views_for_test();
  for (ash::SearchResultContainerView* container : result_containers) {
    EXPECT_TRUE(container->RunScheduledUpdateForTest());
  }

  // Verify result container with web app shortuct result is visible.
  std::vector<raw_ptr<SearchResultContainerView, VectorExperimental>>
      app_result_containers = GetAppShortcutResultContainers();
  EXPECT_EQ(app_result_containers.size(), 1.0f);
  EXPECT_TRUE(app_result_containers[0]->GetVisible());

  // Verify web app shortcut badge logic.
  SearchResultView* app_result = views::AsViewClass<SearchResultView>(
      app_result_containers[0]->GetFirstResultView());
  EXPECT_TRUE(IsWebAppShortcutStyle(app_result));
  EXPECT_FALSE(IsBadgeIconViewVisible(app_result));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "web_app_shortcut_icon_effects_exists", /*revision_number*/ 0,
      app_result));
}

}  // namespace ash
