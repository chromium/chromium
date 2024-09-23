// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm_mode/wm_mode_button_tray.h"

#include "ash/constants/tray_background_view_catalog.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/wm_mode/wm_mode_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

bool ShouldButtonBeVisible() {
  return !Shell::Get()->session_controller()->IsUserSessionBlocked();
}

}  // namespace

WmModeButtonTray::WmModeButtonTray(Shelf* shelf)
    : TrayBackgroundView(shelf, TrayBackgroundViewCatalogName::kWmMode),
      image_view_(tray_container()->AddChildView(
          std::make_unique<views::ImageView>())) {
  SetCallback(base::BindRepeating(
      [](const ui::Event& event) { WmModeController::Get()->Toggle(); }));

  image_view_->SetTooltipText(GetAccessibleNameForTray());
  image_view_->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
  image_view_->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  image_view_->SetPreferredSize(gfx::Size(kTrayItemSize, kTrayItemSize));

  Shell::Get()->session_controller()->AddObserver(this);
}

WmModeButtonTray::~WmModeButtonTray() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void WmModeButtonTray::UpdateButtonVisuals(bool is_wm_mode_active) {
  const ui::ColorId color_id =
      is_wm_mode_active ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                        : cros_tokens::kCrosSysOnSurface;
  image_view_->SetImage(ui::ImageModel::FromVectorIcon(
      is_wm_mode_active ? kWmModeOnIcon : kWmModeOffIcon, color_id));
  SetIsActive(is_wm_mode_active);
}

void WmModeButtonTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();
  UpdateButtonVisuals(WmModeController::Get()->is_active());
}

void WmModeButtonTray::UpdateAfterLoginStatusChange() {
  UpdateButtonVisibility();
}

std::u16string WmModeButtonTray::GetAccessibleNameForTray() {
  // TODO(crbug.com/1366034): Localize once approved.
  return u"WM Mode";
}

void WmModeButtonTray::OnSessionStateChanged(
    session_manager::SessionState state) {
  UpdateButtonVisibility();
}

void WmModeButtonTray::UpdateButtonVisibility() {
  SetVisiblePreferred(ShouldButtonBeVisible());
}

BEGIN_METADATA(WmModeButtonTray)
END_METADATA

}  // namespace ash
