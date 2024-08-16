// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/ime_mode_view.h"

#include "ash/ime/ime_controller_impl.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"

namespace ash {

ImeModeView::ImeModeView(Shelf* shelf) : TrayItemView(shelf) {
  SetVisible(false);
  CreateLabel();
  SetupLabelForTray(label());
  Update();
  SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      kUnifiedTrayTextTopPadding, 0, 0, kUnifiedTrayTextRightPadding)));

  Shell::Get()->system_tray_notifier()->AddIMEObserver(this);
  Shell::Get()->system_tray_model()->locale()->AddObserver(this);
}

ImeModeView::~ImeModeView() {
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

void ImeModeView::OnDisplayTabletStateChanged(display::TabletState state) {
  if (state == display::TabletState::kInClamshellMode ||
      state == display::TabletState::kInTabletMode) {
    Update();
  }
}

void ImeModeView::HandleLocaleChange() {
  Update();
}

void ImeModeView::UpdateLabelOrImageViewColor(bool active) {
  TrayItemView::UpdateLabelOrImageViewColor(active);

  label()->SetEnabledColorId(active
                                 ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                                 : cros_tokens::kCrosSysOnSurface);
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
  if (display::Screen::GetScreen()->InTabletMode()) {
    SetVisible(false);
    return;
  }

  ImeControllerImpl* ime_controller = Shell::Get()->ime_controller();

  if (!ime_controller->IsCurrentImeVisible()) {
    SetVisible(false);
    return;
  }

  size_t ime_count = ime_controller->GetVisibleImes().size();
  SetVisible(!ime_menu_on_shelf_activated_ &&
             (ime_count > 1 || ime_controller->managed_by_policy()));

  label()->SetText(ime_controller->current_ime().short_name);
  UpdateLabelOrImageViewColor(is_active());
  std::u16string description =
      l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_INDICATOR_IME_TOOLTIP,
                                 ime_controller->current_ime().name);
  label()->SetTooltipText(description);
  label()->SetCustomAccessibleName(description);
  label()->SetElideBehavior(gfx::NO_ELIDE);

  DeprecatedLayoutImmediately();
}

BEGIN_METADATA(ImeModeView)
END_METADATA

}  // namespace ash
