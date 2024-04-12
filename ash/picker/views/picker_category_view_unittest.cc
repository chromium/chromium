// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_category_view.h"

#include "ash/picker/mock_picker_asset_fetcher.h"
#include "ash/picker/views/picker_skeleton_loader_view.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/test/views_test_base.h"

namespace ash {
namespace {

using ::testing::SizeIs;

constexpr int kPickerWidth = 320;

class PickerCategoryViewTest : public views::ViewsTestBase {
 public:
  PickerCategoryViewTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

TEST_F(PickerCategoryViewTest, InitialStateIsEmptyResults) {
  MockPickerAssetFetcher asset_fetcher;
  PickerCategoryView view(kPickerWidth, base::DoNothing(), &asset_fetcher);

  EXPECT_TRUE(view.search_results_view_for_testing().GetVisible());
  EXPECT_FALSE(view.skeleton_loader_view_for_testing().GetVisible());
}

TEST_F(PickerCategoryViewTest, ShowLoadingShowsLoaderView) {
  MockPickerAssetFetcher asset_fetcher;
  PickerCategoryView view(kPickerWidth, base::DoNothing(), &asset_fetcher);

  view.ShowLoadingAnimation();

  EXPECT_FALSE(view.search_results_view_for_testing().GetVisible());
  EXPECT_TRUE(view.skeleton_loader_view_for_testing().GetVisible());
  EXPECT_FALSE(view.skeleton_loader_view_for_testing()
                   .layer()
                   ->GetAnimator()
                   ->is_animating());
}

TEST_F(PickerCategoryViewTest, ShowLoadingAnimatesAfterDelay) {
  MockPickerAssetFetcher asset_fetcher;
  PickerCategoryView view(kPickerWidth, base::DoNothing(), &asset_fetcher);

  view.ShowLoadingAnimation();

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  task_environment()->FastForwardBy(PickerCategoryView::kLoadingAnimationDelay);
  EXPECT_TRUE(view.skeleton_loader_view_for_testing()
                  .layer()
                  ->GetAnimator()
                  ->is_animating());
}

TEST_F(PickerCategoryViewTest, SetResultsShowsResults) {
  MockPickerAssetFetcher asset_fetcher;
  PickerCategoryView view(kPickerWidth, base::DoNothing(), &asset_fetcher);

  view.SetResults({PickerSearchResultsSection(PickerSectionType::kLinks,
                                              {PickerSearchResult::Text(u"1")},
                                              /*has_more_results=*/false)});

  EXPECT_TRUE(view.search_results_view_for_testing().GetVisible());
  EXPECT_FALSE(view.skeleton_loader_view_for_testing().GetVisible());
  EXPECT_THAT(
      view.search_results_view_for_testing().section_views_for_testing(),
      SizeIs(1));
}

TEST_F(PickerCategoryViewTest, SetResultsDuringLoadingStopsAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  MockPickerAssetFetcher asset_fetcher;
  PickerCategoryView view(kPickerWidth, base::DoNothing(), &asset_fetcher);
  view.ShowLoadingAnimation();
  task_environment()->FastForwardBy(PickerCategoryView::kLoadingAnimationDelay);

  view.SetResults({PickerSearchResultsSection(PickerSectionType::kLinks,
                                              {PickerSearchResult::Text(u"1")},
                                              /*has_more_results=*/false)});

  EXPECT_FALSE(view.skeleton_loader_view_for_testing().GetVisible());
  EXPECT_FALSE(view.skeleton_loader_view_for_testing()
                   .layer()
                   ->GetAnimator()
                   ->is_animating());
}

TEST_F(PickerCategoryViewTest, SetResultsDuringLoadingSetsResults) {
  MockPickerAssetFetcher asset_fetcher;
  PickerCategoryView view(kPickerWidth, base::DoNothing(), &asset_fetcher);
  view.ShowLoadingAnimation();

  view.SetResults({PickerSearchResultsSection(PickerSectionType::kLinks,
                                              {PickerSearchResult::Text(u"1")},
                                              /*has_more_results=*/false)});

  EXPECT_TRUE(view.search_results_view_for_testing().GetVisible());
  EXPECT_FALSE(view.skeleton_loader_view_for_testing().GetVisible());
  EXPECT_THAT(
      view.search_results_view_for_testing().section_views_for_testing(),
      SizeIs(1));
}

}  // namespace
}  // namespace ash
