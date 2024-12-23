// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/tab_app_selection_view.h"

#include "ash/birch/birch_item_remover.h"
#include "ash/birch/birch_model.h"
#include "ash/birch/test_birch_client.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/coral/coral_test_util.h"
#include "ash/wm/overview/birch/birch_chip_button.h"
#include "ash/wm/overview/birch/tab_app_selection_host.h"
#include "ash/wm/overview/birch/tab_app_selection_view.h"
#include "ash/wm/overview/overview_utils.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/views/view_utils.h"

namespace ash {

class TabAppSelectionViewTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    // Create test birch client.
    auto* birch_model = Shell::Get()->birch_model();
    birch_client_ = std::make_unique<TestBirchClient>(birch_model);
    birch_model->SetClientAndInit(birch_client_.get());

    base::RunLoop run_loop;
    birch_model->GetItemRemoverForTest()->SetProtoInitCallbackForTest(
        run_loop.QuitClosure());
    run_loop.Run();

    // Prepare a coral response so we have a coral glanceable to click.
    std::vector<coral::mojom::GroupPtr> test_groups;
    test_groups.push_back(CreateDefaultTestGroup());
    OverrideTestResponse(std::move(test_groups));
  }

  void TearDown() override {
    Shell::Get()->birch_model()->SetClientAndInit(nullptr);
    birch_client_.reset();
    AshTestBase::TearDown();
  }

 private:
  std::unique_ptr<TestBirchClient> birch_client_;

  base::test::ScopedFeatureList feature_list_{features::kCoralFeature};
};

// Tests that the menu can be toggled to show and hide.
TEST_F(TabAppSelectionViewTest, ToggleMenu) {
  TabAppSelectionHost* menu = ShowAndGetSelectorMenu(GetEventGenerator());
  ASSERT_TRUE(menu);
  EXPECT_TRUE(menu->IsVisible());

  LeftClickOn(menu->owner_for_testing()->addon_view());
  EXPECT_FALSE(menu->IsVisible());

  LeftClickOn(menu->owner_for_testing()->addon_view());
  EXPECT_TRUE(menu->IsVisible());
}

TEST_F(TabAppSelectionViewTest, EscapeHidesMenu) {
  TabAppSelectionHost* menu = ShowAndGetSelectorMenu(GetEventGenerator());
  ASSERT_TRUE(menu);
  EXPECT_TRUE(menu->IsVisible());

  PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_FALSE(menu->IsVisible());
  EXPECT_TRUE(IsInOverviewSession());
}

// Tests clicking the close buttons on the selector menu.
TEST_F(TabAppSelectionViewTest, CloseSelectorItems) {
  TabAppSelectionHost* menu = ShowAndGetSelectorMenu(GetEventGenerator());
  ASSERT_TRUE(menu);

  auto* selection_view =
      views::AsViewClass<TabAppSelectionView>(menu->GetContentsView());
  ASSERT_TRUE(selection_view);

  // Currently there are 3 tabs and 2 apps and they are hardcoded in
  // `TabAppSelectionViewTest`. There should be 2 subtitles.
  ASSERT_TRUE(selection_view->GetViewByID(TabAppSelectionView::kTabSubtitleID));
  ASSERT_TRUE(selection_view->GetViewByID(TabAppSelectionView::kAppSubtitleID));
  EXPECT_EQ(5u, selection_view->item_views_.size());

  // Simulate clicking the close button on the 3 tab items. We do this since
  // `TabAppSelectionItemView` is not exposed. Verify that the tab items are
  // gone, the tab subtitle is also gone, and all the close buttons are gone
  // since we need at least 2 items.
  selection_view->OnCloseButtonPressed(selection_view->item_views_.front());
  selection_view->OnCloseButtonPressed(selection_view->item_views_.front());
  selection_view->OnCloseButtonPressed(selection_view->item_views_.front());
  EXPECT_EQ(2u, selection_view->item_views_.size());
  EXPECT_FALSE(
      selection_view->GetViewByID(TabAppSelectionView::kTabSubtitleID));
  EXPECT_FALSE(
      selection_view->GetViewByID(TabAppSelectionView::kCloseButtonID));
}

// Tests clicking outside the selector view closes it.
TEST_F(TabAppSelectionViewTest, PressToHideMenu) {
  TabAppSelectionHost* menu = ShowAndGetSelectorMenu(GetEventGenerator());
  ASSERT_TRUE(menu);

  // Clicks on the selector itself should not hide it.
  LeftClickOn(menu->GetContentsView());
  EXPECT_TRUE(menu->IsVisible());

  // Test clicking outside the selector.
  GetEventGenerator()->MoveMouseTo(gfx::Point(1, 1));
  GetEventGenerator()->ClickLeftButton();
  EXPECT_TRUE(!menu->IsVisible());

  // Test tapping outside the selector.
  menu = ShowAndGetSelectorMenu(GetEventGenerator());
  ASSERT_TRUE(menu);
  GetEventGenerator()->GestureTapAt(gfx::Point(1, 1));
  EXPECT_TRUE(!menu->IsVisible());
}

// Tests that corresponding metrics are recorded when showing the menu, removing
// the item, and clicking on the thumbs up/down buttons.
TEST_F(TabAppSelectionViewTest, RecordsHistogram) {
  base::HistogramTester histograms;

  TabAppSelectionHost* menu = ShowAndGetSelectorMenu(GetEventGenerator());
  ASSERT_TRUE(menu);
  EXPECT_TRUE(menu->IsVisible());

  // One menu expanded is recorded.
  histograms.ExpectBucketCount("Ash.Birch.Coral.ClusterExpanded", true, 1);

  auto* selection_view =
      views::AsViewClass<TabAppSelectionView>(menu->GetContentsView());
  ASSERT_TRUE(selection_view);
  // There are 5 items.
  ASSERT_EQ(selection_view->item_views_.size(), 5u);

  // Close two items.
  selection_view->OnCloseButtonPressed(selection_view->item_views_.front());
  selection_view->OnCloseButtonPressed(selection_view->item_views_.front());

  // Click on thumbs up button.
  LeftClickOn(
      selection_view->GetViewByID(TabAppSelectionView::ViewID::kThumbsUpID));
  // One thumbs up is recorded.
  histograms.ExpectBucketCount("Ash.Birch.Coral.UserFeedback", true, 1);

  LeftClickOn(menu->owner_for_testing()->addon_view());
  EXPECT_FALSE(menu->IsVisible());

  LeftClickOn(menu->owner_for_testing()->addon_view());
  EXPECT_TRUE(menu->IsVisible());

  // Another menu expanded is recorded.
  histograms.ExpectBucketCount("Ash.Birch.Coral.ClusterExpanded", true, 2);

  // There are 3 items.
  ASSERT_EQ(selection_view->item_views_.size(), 3u);

  // Close one item.
  selection_view->OnCloseButtonPressed(selection_view->item_views_.front());

  // Click on the thumbs down button.
  LeftClickOn(
      selection_view->GetViewByID(TabAppSelectionView::ViewID::kThumbsDownID));
  // One thumbs down is recorded.
  histograms.ExpectBucketCount("Ash.Birch.Coral.UserFeedback", false, 1);

  ExitOverview();
  // After exiting Overview, the total number of removed items is recorded.
  histograms.ExpectBucketCount("Ash.Birch.Coral.ClusterItemRemoved", 3, 1);
}

}  // namespace ash
