// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/rotation/rotation_lock_feature_pod_controller.h"

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

RotationLockFeaturePodController::RotationLockFeaturePodController() {
  DCHECK(Shell::Get());
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  Shell::Get()->screen_orientation_controller()->AddObserver(this);
}

RotationLockFeaturePodController::~RotationLockFeaturePodController() {
  if (Shell::Get()->screen_orientation_controller())
    Shell::Get()->screen_orientation_controller()->RemoveObserver(this);
  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
}

FeaturePodButton* RotationLockFeaturePodController::CreateButton() {
  DCHECK(!button_);
  button_ = new FeaturePodButton(this);
  button_->DisableLabelButtonFocus();
  // Init the button with invisible state. The `UpdateButton` method will update
  // the visibility based on the current condition.
  button_->SetVisible(false);
  UpdateButton();
  return button_;
}

QsFeatureCatalogName RotationLockFeaturePodController::GetCatalogName() {
  return QsFeatureCatalogName::kRotationLock;
}

void RotationLockFeaturePodController::OnIconPressed() {
  TrackToggleUMA(/*target_toggle_state=*/!Shell::Get()
                     ->screen_orientation_controller()
                     ->user_rotation_locked());

  Shell::Get()->screen_orientation_controller()->ToggleUserRotationLock();
}

void RotationLockFeaturePodController::OnTabletPhysicalStateChanged() {
  UpdateButton();
}

void RotationLockFeaturePodController::OnUserRotationLockChanged() {
  UpdateButton();
}

void RotationLockFeaturePodController::UpdateButton() {
  // Even though auto-rotation is also supported when the device is not in a
  // tablet physical state but kSupportsClamshellAutoRotation is set. The "Auto
  // rotate" feature pod button in the system tray menu is not expected to be
  // shown in the case.
  const bool is_auto_rotation_allowed =
      Shell::Get()->tablet_mode_controller()->is_in_tablet_physical_state();

  if (!button_->GetVisible() && is_auto_rotation_allowed)
    TrackVisibilityUMA();

  button_->SetVisible(is_auto_rotation_allowed);

  if (!is_auto_rotation_allowed)
    return;

  auto* screen_orientation_controller =
      Shell::Get()->screen_orientation_controller();
  const bool rotation_locked =
      screen_orientation_controller->user_rotation_locked();
  const bool is_portrait =
      screen_orientation_controller->IsUserLockedOrientationPortrait();

  button_->SetToggled(rotation_locked);

  std::u16string tooltip_state;

  if (rotation_locked && is_portrait) {
    button_->SetVectorIcon(kUnifiedMenuRotationLockPortraitIcon);
    button_->SetLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_ROTATION_LOCK_LOCKED_LABEL));
    button_->SetSubLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_ROTATION_LOCK_LOCKED_VERTICAL_SUBLABEL));
    tooltip_state = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_ROTATION_LOCK_LOCKED_VERTICAL_TOOLTIP);
  } else if (rotation_locked && !is_portrait) {
    button_->SetVectorIcon(kUnifiedMenuRotationLockLandscapeIcon);
    button_->SetLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_ROTATION_LOCK_LOCKED_LABEL));
    button_->SetSubLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_ROTATION_LOCK_LOCKED_HORIZONTAL_SUBLABEL));
    tooltip_state = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_ROTATION_LOCK_LOCKED_HORIZONTAL_TOOLTIP);
  } else {
    button_->SetVectorIcon(kUnifiedMenuRotationLockAutoIcon);
    button_->SetLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_ROTATION_LOCK_AUTO_LABEL));
    button_->SetSubLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_ROTATION_LOCK_AUTO_SUBLABEL));
    tooltip_state =
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ROTATION_LOCK_AUTO_LABEL);
  }

  button_->SetIconAndLabelTooltips(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_ROTATION_LOCK_TOOLTIP, tooltip_state));
}

}  // namespace ash
