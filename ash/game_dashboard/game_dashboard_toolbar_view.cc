// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_toolbar_view.h"

#include <memory>

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"

namespace ash {

namespace {

// Horizontal padding for the border around the toolbar.
constexpr int kPaddingWidth = 4;
// Vertical padding for the border around the toolbar.
constexpr int kPaddingHeight = 6;
// Padding between children in the toolbar.
constexpr int kBetweenChildSpacing = 8;

std::unique_ptr<IconButton> CreateIconButton(base::RepeatingClosure callback,
                                             const gfx::VectorIcon* icon,
                                             int view_id,
                                             const std::u16string& text) {
  auto button = std::make_unique<IconButton>(
      std::move(callback), IconButton::Type::kSmallFloating, icon, text,
      /*is_togglable=*/false, /*has_border=*/true);
  button->SetID(view_id);
  return button;
}

}  // namespace

GameDashboardToolbarView::GameDashboardToolbarView(aura::Window* game_window)
    : game_window_(game_window) {
  DCHECK(game_window_);
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetBackground(views::CreateThemedSolidBackground(kColorAshShieldAndBase80));
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetInsideBorderInsets(gfx::Insets::VH(kPaddingHeight, kPaddingWidth));
  SetBetweenChildSpacing(kBetweenChildSpacing);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);

  AddShortcutTiles();
}

GameDashboardToolbarView::~GameDashboardToolbarView() = default;

void GameDashboardToolbarView::OnGamepadButtonPressed() {
  // TODO(b/283981592): Toggle toolbar expansion support.
}

void GameDashboardToolbarView::OnRecordButtonPressed() {
  // TODO(b/273641250): Add screen record support.
}

void GameDashboardToolbarView::OnScreenshotButtonPressed() {
  CaptureModeController::Get()->CaptureScreenshotOfGivenWindow(game_window_);
}

void GameDashboardToolbarView::AddShortcutTiles() {
  AddChildView(CreateIconButton(
      base::BindRepeating(&GameDashboardToolbarView::OnGamepadButtonPressed,
                          base::Unretained(this)),
      &vector_icons::kVideogameAssetOutlineIcon,
      base::to_underlying(ToolbarViewId::kGamepadButton),
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_TOOLBAR_TILE_BUTTON_TITLE)));

  // TODO(b/273641467): Add toggle Game Controls support.

  // TODO(b/273641132): Filter out record game button based on device info.
  AddChildView(CreateIconButton(
      base::BindRepeating(&GameDashboardToolbarView::OnRecordButtonPressed,
                          base::Unretained(this)),
      &vector_icons::kVideocamIcon,
      base::to_underlying(ToolbarViewId::kScreenRecordButton),
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_RECORD_GAME_TILE_BUTTON_TITLE)));

  AddChildView(CreateIconButton(
      base::BindRepeating(&GameDashboardToolbarView::OnScreenshotButtonPressed,
                          base::Unretained(this)),
      &vector_icons::kVideocamIcon,
      base::to_underlying(ToolbarViewId::kScreenshotButton),
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_SCREENSHOT_TILE_BUTTON_TITLE)));
}

BEGIN_METADATA(GameDashboardToolbarView, views::BoxLayoutView)
END_METADATA

}  // namespace ash
