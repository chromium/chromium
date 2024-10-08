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
#include "ash/wm/overview/birch/birch_bar_controller.h"
#include "ash/wm/overview/birch/birch_chip_button.h"
#include "ash/wm/overview/birch/birch_chip_button_base.h"
#include "ash/wm/overview/birch/tab_app_selection_host.h"
#include "ash/wm/overview/birch/tab_app_selection_view.h"
#include "ash/wm/overview/overview_grid_test_api.h"
#include "ash/wm/overview/overview_utils.h"
#include "base/test/scoped_feature_list.h"
#include "ui/views/view_utils.h"

namespace ash {

class TabAppSelectionViewTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    // Create test birch client and test coral provider.
    auto* birch_model = Shell::Get()->birch_model();
    birch_client_ = std::make_unique<TestBirchClient>(birch_model);
    birch_model->SetClientAndInit(birch_client_.get());

    auto coral_provider =
        std::make_unique<TestBirchDataProvider<BirchCoralItem>>(
            base::BindRepeating(&BirchModel::SetCoralItems,
                                base::Unretained(birch_model)),
            prefs::kBirchUseCoral);
    coral_provider_ = coral_provider.get();
    birch_model->OverrideCoralProviderForTest(std::move(coral_provider));

    base::RunLoop run_loop;
    birch_model->GetItemRemoverForTest()->SetProtoInitCallbackForTest(
        run_loop.QuitClosure());
    run_loop.Run();

    // Prepare a coral item so we have a coral glanceable to click.
    std::vector<GURL> page_urls;
    page_urls.emplace_back(("https://www.reddit.com/"));
    page_urls.emplace_back(("https://www.figma.com/"));
    page_urls.emplace_back(("https://www.notion.so/"));

    std::vector<std::string> app_ids;
    app_ids.emplace_back("lgnggepjiihbfdbedefdhcffnmhcahbm");
    app_ids.emplace_back("odknhmnlageboeamepcngndbggdpaobj");

    coral_provider_->set_items(
        {BirchCoralItem(u"Title", u"Text", /*page_urls=*/page_urls,
                        /*app_ids=*/app_ids, /*cluster_id=*/0)});
  }

  void TearDown() override {
    Shell::Get()->birch_model()->SetClientAndInit(nullptr);
    coral_provider_ = nullptr;
    birch_client_.reset();
    AshTestBase::TearDown();
  }

  // Brings up the selector menu host object by entering overview and clicking
  // the birch coral chip.
  TabAppSelectionHost* ShowAndGetSelectorMenu() {
    EnterOverview();

    const std::vector<raw_ptr<BirchChipButtonBase>>& birch_chips =
        OverviewGridTestApi(Shell::GetPrimaryRootWindow()).GetBirchChips();
    CHECK_EQ(1u, birch_chips.size());

    auto* coral_button = views::AsViewClass<BirchChipButton>(birch_chips[0]);
    CHECK_EQ(BirchItemType::kCoral, coral_button->GetItem()->GetType());

    LeftClickOn(coral_button->addon_view());
    return coral_button->tab_app_selection_widget_.get();
  }

 private:
  std::unique_ptr<TestBirchClient> birch_client_;
  raw_ptr<TestBirchDataProvider<BirchCoralItem>> coral_provider_;

  base::test::ScopedFeatureList feature_list_{features::kBirchCoral};
};

// Tests that the menu can be toggled to show and hide.
TEST_F(TabAppSelectionViewTest, ToggleMenu) {
  TabAppSelectionHost* menu = ShowAndGetSelectorMenu();
  ASSERT_TRUE(menu);
  EXPECT_TRUE(menu->IsVisible());

  LeftClickOn(menu->owner_for_testing()->addon_view());
  EXPECT_FALSE(menu->IsVisible());

  LeftClickOn(menu->owner_for_testing()->addon_view());
  EXPECT_TRUE(menu->IsVisible());
}

TEST_F(TabAppSelectionViewTest, EscapeHidesMenu) {
  TabAppSelectionHost* menu = ShowAndGetSelectorMenu();
  ASSERT_TRUE(menu);
  EXPECT_TRUE(menu->IsVisible());

  PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_FALSE(menu->IsVisible());
  EXPECT_TRUE(IsInOverviewSession());
}

// Tests clicking the close buttons on the selector menu.
TEST_F(TabAppSelectionViewTest, CloseSelectorItems) {
  TabAppSelectionHost* menu = ShowAndGetSelectorMenu();
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
  TabAppSelectionHost* menu = ShowAndGetSelectorMenu();
  ASSERT_TRUE(menu);

  // Clicks on the selector itself should not hide it.
  LeftClickOn(menu->GetContentsView());
  EXPECT_TRUE(menu->IsVisible());

  // Test clicking outside the selector.
  GetEventGenerator()->MoveMouseTo(gfx::Point(1, 1));
  GetEventGenerator()->ClickLeftButton();
  EXPECT_TRUE(!menu->IsVisible());

  // Test tapping outside the selector.
  menu = ShowAndGetSelectorMenu();
  ASSERT_TRUE(menu);
  GetEventGenerator()->GestureTapAt(gfx::Point(1, 1));
  EXPECT_TRUE(!menu->IsVisible());
}

}  // namespace ash
