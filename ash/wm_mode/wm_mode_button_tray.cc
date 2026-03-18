// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm_mode/wm_mode_button_tray.h"

#include <string>

#include "ash/constants/tray_background_view_catalog.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/tray/imaged_tray_icon.h"
#include "ash/system/tray/tray_container.h"
#include "ash/wm_mode/wm_mode_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

namespace {

bool ShouldButtonBeVisible() {
  return !Shell::Get()->session_controller()->IsUserSessionBlocked();
}

}  // namespace

// TODO(crbug.com/252558235): Localize once approved.
WmModeButtonTray::WmModeButtonTray(Shelf* shelf)
    : ImagedTrayIcon(shelf,
                     ui::ImageModel(),
                     /*tooltip=*/u"WM Mode",
                     /*accessibility_name=*/u"WM Mode",
                     TrayBackgroundViewCatalogName::kWmMode) {
  SetCallback(base::BindRepeating(
      [](const ui::Event& event) { WmModeController::Get()->Toggle(); }));

  Shell::Get()->session_controller()->AddObserver(this);
}

WmModeButtonTray::~WmModeButtonTray() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void WmModeButtonTray::UpdateButtonVisuals(bool is_wm_mode_active) {
  const ui::ColorId color_id =
      is_wm_mode_active ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                        : cros_tokens::kCrosSysOnSurface;
  image_view()->SetImage(ui::ImageModel::FromVectorIcon(
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
