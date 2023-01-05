// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/rotation/rotation_lock_feature_pod_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/functional/bind.h"
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
  DCHECK(!features::IsQsRevampEnabled());
  button_ = new FeaturePodButton(this);
  button_->DisableLabelButtonFocus();
  // Init the button with invisible state. The `UpdateButton` method will update
  // the visibility based on the current condition.
  button_->SetVisible(false);
  UpdateButton();
  return button_;
}

std::unique_ptr<FeatureTile> RotationLockFeaturePodController::CreateTile() {
  DCHECK(!tile_);
  DCHECK(features::IsQsRevampEnabled());
  // TODO(b/251724698): Create the tile as FeatureTile::TileType::kCompact
  // after adding logic to shrink the Cast tile to compact when auto-rotation
  // is allowed. Also remove the call to SetSubLabelVisibility().
  auto tile = std::make_unique<FeatureTile>(
      base::BindRepeating(&RotationLockFeaturePodController::OnIconPressed,
                          weak_factory_.GetWeakPtr()));
  tile_ = tile.get();
  // The tile label is always "Auto rotate" and there is no sub-label.
  tile_->SetLabel(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ROTATION_LOCK_AUTO));
  tile_->SetSubLabelVisibility(false);
  // UpdateTile() will update visibility.
  tile_->SetVisible(false);
  UpdateTile();
  return tile;
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
  if (features::IsQsRevampEnabled()) {
    UpdateTile();
  } else {
    UpdateButton();
  }
}

void RotationLockFeaturePodController::OnUserRotationLockChanged() {
  if (features::IsQsRevampEnabled()) {
    UpdateTile();
  } else {
    UpdateButton();
  }
}

void RotationLockFeaturePodController::UpdateButton() {
  DCHECK(!features::IsQsRevampEnabled());
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

void RotationLockFeaturePodController::UpdateTile() {
  DCHECK(features::IsQsRevampEnabled());
  // Auto-rotation is also supported when the device is not in a tablet physical
  // state but kSupportsClamshellAutoRotation is set. The "Auto rotate" feature
  // tile in the system tray menu is not expected to be shown in the case.
  const bool is_auto_rotation_allowed =
      Shell::Get()->tablet_mode_controller()->is_in_tablet_physical_state();

  if (!tile_->GetVisible() && is_auto_rotation_allowed) {
    TrackVisibilityUMA();
  }
  tile_->SetVisible(is_auto_rotation_allowed);

  if (!is_auto_rotation_allowed) {
    return;
  }

  auto* screen_orientation_controller =
      Shell::Get()->screen_orientation_controller();
  const bool rotation_locked =
      screen_orientation_controller->user_rotation_locked();
  const bool is_portrait =
      screen_orientation_controller->IsUserLockedOrientationPortrait();

  // The tile is toggled when auto-rotate is on.
  tile_->SetToggled(!rotation_locked);

  std::u16string tooltip_state;
  if (rotation_locked && is_portrait) {
    tile_->SetVectorIcon(kUnifiedMenuRotationLockAutoIcon);
    tooltip_state = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_ROTATION_LOCK_LOCKED_VERTICAL_TOOLTIP);
  } else if (rotation_locked && !is_portrait) {
    tile_->SetVectorIcon(kUnifiedMenuRotationLockAutoIcon);
    tooltip_state = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_ROTATION_LOCK_LOCKED_HORIZONTAL_TOOLTIP);
  } else {
    // TODO(b/264428682): Custom icon for auto-rotate (non-locked) state.
    tile_->SetVectorIcon(kUnifiedMenuRotationLockAutoIcon);
    tooltip_state =
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ROTATION_LOCK_AUTO_LABEL);
  }

  tile_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_ROTATION_LOCK_TOOLTIP, tooltip_state));
}

}  // namespace ash
