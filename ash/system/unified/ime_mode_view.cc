// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/ime_mode_view.h"

#include "ash/ime/ime_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"

namespace ash {

ImeModeView::ImeModeView(Shelf* shelf) : TrayItemView(shelf) {
  SetVisible(false);
  CreateLabel();
  SetupLabelForTray(label());
  Update();

  Shell::Get()->system_tray_notifier()->AddIMEObserver(this);
  Shell::Get()->system_tray_model()->locale()->AddObserver(this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
}

ImeModeView::~ImeModeView() {
  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  Shell::Get()->system_tray_model()->locale()->RemoveObserver(this);
  Shell::Get()->system_tray_notifier()->RemoveIMEObserver(this);
}

void ImeModeView::OnIMERefresh() {
  Update();
}

void ImeModeView::OnIMEMenuActivationChanged(bool is_active) {
  ime_menu_on_shelf_activated_ = is_active;
  Update();
}

void ImeModeView::OnLocaleListSet() {
  Update();
}

void ImeModeView::OnTabletModeStarted() {
  Update();
}

void ImeModeView::OnTabletModeEnded() {
  Update();
}

void ImeModeView::OnSessionStateChanged(session_manager::SessionState state) {
  Update();
}

const char* ImeModeView::GetClassName() const {
  return "ImeModeView";
}

void ImeModeView::Update() {
  // Hide the IME mode icon when the locale is shown, because showing locale and
  // IME together is confusing.
  if (Shell::Get()
          ->system_tray_model()
          ->locale()
          ->ShouldShowCurrentLocaleInStatusArea()) {
    SetVisible(false);
    return;
  }

  // Do not show IME mode icon in tablet mode as it's less useful and screen
  // space is limited.
  if (Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    SetVisible(false);
    return;
  }

  ImeController* ime_controller = Shell::Get()->ime_controller();

  size_t ime_count = ime_controller->available_imes().size();
  SetVisible(!ime_menu_on_shelf_activated_ &&
             (ime_count > 1 || ime_controller->managed_by_policy()));

  label()->SetText(ime_controller->current_ime().short_name);
  label()->SetEnabledColor(
      TrayIconColor(Shell::Get()->session_controller()->GetSessionState()));
  base::string16 description =
      l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_INDICATOR_IME_TOOLTIP,
                                 ime_controller->current_ime().name);
  label()->SetTooltipText(description);
  label()->SetCustomAccessibleName(description);
  label()->SetElideBehavior(gfx::NO_ELIDE);

  Layout();
}

}  // namespace ash
