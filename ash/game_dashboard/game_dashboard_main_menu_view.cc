// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_main_menu_view.h"

#include <memory>

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/feature_tile.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

constexpr int kBubbleCornerRadius = 8;
// Horizontal padding for the border around the main menu.
constexpr int kPaddingWidth = 12;
// Vertical padding for the border around the main menu.
constexpr int kPaddingHeight = 15;
// Padding between children in a row or column.
constexpr int kCenterPadding = 8;

// Creates an individual Game Dashboard Tile.
std::unique_ptr<FeatureTile> CreateTile(base::RepeatingClosure callback,
                                        bool is_togglable,
                                        FeatureTile::TileType type,
                                        int id,
                                        const gfx::VectorIcon& icon,
                                        const std::u16string& text) {
  auto tile =
      std::make_unique<FeatureTile>(std::move(callback), is_togglable, type);
  tile->SetID(id);
  tile->SetVisible(true);
  tile->SetVectorIcon(icon);
  tile->SetLabel(text);
  tile->SetTooltipText(text);
  return tile;
}

}  // namespace

GameDashboardMainMenuView::GameDashboardMainMenuView(
    views::Widget* main_menu_button_widget,
    aura::Window* game_window)
    : game_window_(game_window) {
  DCHECK(main_menu_button_widget);
  DCHECK(game_window_);

  set_corner_radius(kBubbleCornerRadius);
  set_close_on_deactivate(false);
  set_internal_name("GameDashboardMainMenuView");
  set_margins(gfx::Insets());
  set_parent_window(main_menu_button_widget->GetNativeWindow());
  SetAnchorView(main_menu_button_widget->GetContentsView());
  SetArrow(views::BubbleBorder::Arrow::NONE);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kPaddingHeight, kPaddingWidth), kCenterPadding));

  AddShortcutTilesRow();

  SizeToPreferredSize();
}

GameDashboardMainMenuView::~GameDashboardMainMenuView() = default;

void GameDashboardMainMenuView::OnToolbarTilePressed() {
  // TODO(b/273641426): Add support when toolbar tile is pressed.
}

void GameDashboardMainMenuView::OnRecordGameTilePressed() {
  // TODO(b/273641250): Add support when record game tile is pressed.
}

void GameDashboardMainMenuView::OnScreenshotTilePressed() {
  CaptureModeController::Get()->CaptureScreenshotOfGivenWindow(game_window_);
  GetWidget()->Close();
}

void GameDashboardMainMenuView::AddShortcutTilesRow() {
  views::BoxLayoutView* container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  container->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  container->SetBetweenChildSpacing(kCenterPadding);

  container->AddChildView(CreateTile(
      base::BindRepeating(&GameDashboardMainMenuView::OnToolbarTilePressed,
                          base::Unretained(this)),
      /*is_togglable=*/false, FeatureTile::TileType::kCompact,
      VIEW_ID_GD_TOOLBAR_TILE, vector_icons::kVideogameAssetOutlineIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_TOOLBAR_TILE_BUTTON_TITLE)));
  // TODO(b/273641132): Filter out record game button based on device info.
  container->AddChildView(CreateTile(
      base::BindRepeating(&GameDashboardMainMenuView::OnRecordGameTilePressed,
                          base::Unretained(this)),
      /*is_togglable=*/false, FeatureTile::TileType::kCompact,
      VIEW_ID_GD_RECORD_TILE, vector_icons::kVideocamIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_RECORD_GAME_TILE_BUTTON_TITLE)));
  container->AddChildView(CreateTile(
      base::BindRepeating(&GameDashboardMainMenuView::OnScreenshotTilePressed,
                          base::Unretained(this)),
      /*is_togglable=*/true, FeatureTile::TileType::kCompact,
      VIEW_ID_GD_SCREENSHOT_TILE, vector_icons::kVideocamIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_SCREENSHOT_TILE_BUTTON_TITLE)));
}

BEGIN_METADATA(GameDashboardMainMenuView, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace ash
