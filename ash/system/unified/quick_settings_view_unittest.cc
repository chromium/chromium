// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quick_settings_view.h"

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/test/test_cast_config_controller.h"
#include "ash/shell.h"
#include "ash/style/pagination_view.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/feature_tiles_container_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

class QuickSettingsViewTest : public AshTestBase {
 public:
  QuickSettingsViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  QuickSettingsViewTest(const QuickSettingsViewTest&) = delete;
  QuickSettingsViewTest& operator=(const QuickSettingsViewTest&) = delete;
  ~QuickSettingsViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    cast_config_ = std::make_unique<TestCastConfigController>();
  }

  void TearDown() override {
    cast_config_.reset();
    AshTestBase::TearDown();
  }

  FeatureTilesContainerView* GetFeatureTilesContainer() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->quick_settings_view()
        ->feature_tiles_container();
  }

  PaginationView* GetPaginationView() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->quick_settings_view()
        ->pagination_view_for_test();
  }

  PaginationModel* pagination_model() {
    return GetFeatureTilesContainer()->pagination_model_;
  }

  FeatureTile* GetTileById(int tile_view_id) {
    views::View* tile_view = GetPrimaryUnifiedSystemTray()
                                 ->bubble()
                                 ->quick_settings_view()
                                 ->GetViewByID(tile_view_id);
    return static_cast<FeatureTile*>(tile_view);
  }

  views::View* GetAccessibilityFocusHelperView() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->quick_settings_view()
        ->GetAccessibilityFocusHelperViewForTesting();
  }

 private:
  // This is required to make the cast tile visible in the
  // `CastAndAutoRotateCompactTiles` unit test. Cast features will not be used.
  std::unique_ptr<TestCastConfigController> cast_config_;
};

// Tests that the cast and auto-rotate tiles are presented in their compact
// version when they are both visible.
TEST_F(QuickSettingsViewTest, CastAndAutoRotateCompactTiles) {
  auto* tray = GetPrimaryUnifiedSystemTray();
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();

  // Test that the cast tile is in its primary form when in clamshell mode,
  // when the auto-rotate tile is not visible.
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  tray->ShowBubble();

  FeatureTile* cast_tile = GetTileById(VIEW_ID_FEATURE_TILE_CAST);
  ASSERT_TRUE(cast_tile);
  EXPECT_TRUE(cast_tile->GetVisible());
  EXPECT_EQ(cast_tile->tile_type(), FeatureTile::TileType::kPrimary);

  FeatureTile* autorotate_tile = GetTileById(VIEW_ID_FEATURE_TILE_AUTOROTATE);
  EXPECT_FALSE(autorotate_tile->GetVisible());

  tray->CloseBubble();

  // Test that cast and auto-rotate tiles are compact in tablet mode.
  tablet_mode_controller->SetEnabledForTest(true);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  tray->ShowBubble();

  cast_tile = GetTileById(VIEW_ID_FEATURE_TILE_CAST);
  EXPECT_TRUE(cast_tile->GetVisible());
  EXPECT_EQ(cast_tile->tile_type(), FeatureTile::TileType::kCompact);

  autorotate_tile = GetTileById(VIEW_ID_FEATURE_TILE_AUTOROTATE);
  EXPECT_TRUE(autorotate_tile->GetVisible());
  EXPECT_EQ(autorotate_tile->tile_type(), FeatureTile::TileType::kCompact);

  tray->CloseBubble();
}

// Tests that the screen capture and DND tiles are presented in their compact
// version when they are both visible.
TEST_F(QuickSettingsViewTest, CaptureAndDNDCompactTiles) {
  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();

  FeatureTile* capture_tile = GetTileById(VIEW_ID_FEATURE_TILE_SCREEN_CAPTURE);
  EXPECT_TRUE(capture_tile->GetVisible());
  EXPECT_EQ(capture_tile->tile_type(), FeatureTile::TileType::kCompact);

  FeatureTile* dnd_tile = GetTileById(VIEW_ID_FEATURE_TILE_DND);
  EXPECT_TRUE(dnd_tile->GetVisible());
  EXPECT_EQ(dnd_tile->tile_type(), FeatureTile::TileType::kCompact);

  tray->CloseBubble();

  // TODO(b/266000781): Add test cases for when one tile is visible but the
  // other is not, to test they show in their primary forms.
}

// Tests that the page indicator is only visible with two or more pages.
TEST_F(QuickSettingsViewTest, PageIndicatorVisibility) {
  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();

  // Page indicator is not visible with one page.
  pagination_model()->SetTotalPages(1);
  EXPECT_FALSE(GetPaginationView()->GetVisible());

  // Page indicator is visible with two or more pages.
  pagination_model()->SetTotalPages(2);
  EXPECT_TRUE(GetPaginationView()->GetVisible());

  tray->CloseBubble();
}

TEST_F(QuickSettingsViewTest, ResetSelectedPageAfterClosingBubble) {
  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();

  // Select a page other than the first one.
  pagination_model()->SetTotalPages(2);
  pagination_model()->SelectPage(/*page=*/1, /*animate=*/false);
  EXPECT_EQ(1, pagination_model()->selected_page());

  // Selected page resets to zero after closing and opening bubble.
  GetPrimaryUnifiedSystemTray()->CloseBubble();
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  EXPECT_EQ(0, pagination_model()->selected_page());
}

TEST_F(QuickSettingsViewTest,
       AccessibilityFocusHelperViewAccessibleProperties) {
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  ui::AXNodeData data;
  auto* helper_view = GetAccessibilityFocusHelperView();

  ASSERT_TRUE(helper_view);
  helper_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kListItem);

  GetPrimaryUnifiedSystemTray()->CloseBubble();
}

}  // namespace ash
