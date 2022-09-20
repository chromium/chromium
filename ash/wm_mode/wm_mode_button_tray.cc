// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm_mode/wm_mode_button_tray.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/wm_mode/wm_mode_controller.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

bool ShouldButtonBeVisible() {
  return !Shell::Get()->session_controller()->IsUserSessionBlocked();
}

}  // namespace

WmModeButtonTray::WmModeButtonTray(Shelf* shelf)
    : TrayBackgroundView(shelf),
      image_view_(tray_container()->AddChildView(
          std::make_unique<views::ImageView>())) {
  image_view_->SetTooltipText(GetAccessibleNameForTray());
  image_view_->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
  image_view_->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  image_view_->SetPreferredSize(gfx::Size(kTrayItemSize, kTrayItemSize));

  Shell::Get()->session_controller()->AddObserver(this);
}

WmModeButtonTray::~WmModeButtonTray() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void WmModeButtonTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();
  UpdateButtonIcon();
}

void WmModeButtonTray::UpdateAfterLoginStatusChange() {
  UpdateButtonVisibility();
}

std::u16string WmModeButtonTray::GetAccessibleNameForTray() {
  // TODO(crbug.com/1366034): Localize once approved.
  return u"WM Mode";
}

bool WmModeButtonTray::PerformAction(const ui::Event& event) {
  DCHECK(event.type() == ui::ET_MOUSE_RELEASED ||
         event.type() == ui::ET_GESTURE_TAP ||
         event.type() == ui::ET_KEY_PRESSED);

  WmModeController::Get()->Toggle();
  SetIsActive(WmModeController::Get()->is_active());
  UpdateButtonIcon();

  return true;
}

void WmModeButtonTray::OnSessionStateChanged(
    session_manager::SessionState state) {
  UpdateButtonVisibility();
}

void WmModeButtonTray::UpdateButtonIcon() {
  image_view_->SetImage(gfx::CreateVectorIcon(
      WmModeController::Get()->is_active() ? kWmModeOnIcon : kWmModeOffIcon,
      GetColorProvider()->GetColor(kColorAshIconColorPrimary)));
}

void WmModeButtonTray::UpdateButtonVisibility() {
  SetVisiblePreferred(ShouldButtonBeVisible());
}

}  // namespace ash
