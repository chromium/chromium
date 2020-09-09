// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/dark_mode/dark_mode_feature_pod_controller.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

DarkModeFeaturePodController::DarkModeFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {
  DCHECK(tray_controller_);
  AshColorProvider::Get()->AddObserver(this);
}

DarkModeFeaturePodController::~DarkModeFeaturePodController() {
  AshColorProvider::Get()->RemoveObserver(this);
}

FeaturePodButton* DarkModeFeaturePodController::CreateButton() {
  DCHECK(!button_);
  button_ = new FeaturePodButton(this);
  button_->SetVectorIcon(kUnifiedMenuDarkModeIcon);
  button_->SetLabel(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DARK_THEME));
  button_->SetLabelTooltip(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_DARK_THEME_SETTINGS_TOOLTIP));
  // TODO(minch): Add the logic for login screen.
  button_->SetVisible(
      Shell::Get()->session_controller()->IsActiveUserSessionStarted());

  UpdateButton(AshColorProvider::Get()->IsDarkModeEnabled());
  return button_;
}

void DarkModeFeaturePodController::OnIconPressed() {
  AshColorProvider::Get()->ToggleColorMode();
}

void DarkModeFeaturePodController::OnLabelPressed() {
  tray_controller_->ShowDarkModeDetailedView();
}

SystemTrayItemUmaType DarkModeFeaturePodController::GetUmaType() const {
  return SystemTrayItemUmaType::UMA_DARK_MODE;
}

void DarkModeFeaturePodController::OnColorModeChanged(bool dark_mode_enabled) {
  UpdateButton(dark_mode_enabled);
}

void DarkModeFeaturePodController::UpdateButton(bool dark_mode_enabled) {
  button_->SetToggled(dark_mode_enabled);
  button_->SetSubLabel(l10n_util::GetStringUTF16(
      dark_mode_enabled ? IDS_ASH_STATUS_TRAY_DARK_THEME_ON_STATE
                        : IDS_ASH_STATUS_TRAY_DARK_THEME_OFF_STATE));

  base::string16 tooltip_state = l10n_util::GetStringUTF16(
      dark_mode_enabled
          ? IDS_ASH_STATUS_TRAY_DARK_THEME_ENABLED_STATE_TOOLTIP
          : IDS_ASH_STATUS_TRAY_DARK_THEME_DISABLED_STATE_TOOLTIP);
  button_->SetIconTooltip(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_DARK_THEME_TOGGLE_TOOLTIP, tooltip_state));
}

}  // namespace ash
