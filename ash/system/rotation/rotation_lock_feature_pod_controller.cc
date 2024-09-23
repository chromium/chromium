// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/rotation/rotation_lock_feature_pod_controller.h"

#include <memory>

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/functional/bind.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

RotationLockFeaturePodController::RotationLockFeaturePodController() {
  DCHECK(Shell::Get());
  Shell::Get()->screen_orientation_controller()->AddObserver(this);
}

RotationLockFeaturePodController::~RotationLockFeaturePodController() {
  if (Shell::Get()->screen_orientation_controller())
    Shell::Get()->screen_orientation_controller()->RemoveObserver(this);
}

// static
bool RotationLockFeaturePodController::CalculateButtonVisibility() {
  // Auto-rotation is supported in both tablet mode and in clamshell mode with
  // `kSupportsClamshellAutoRotation` set, however the button is shown only if
  // the device is in tablet mode.
  return Shell::Get()->tablet_mode_controller()->is_in_tablet_physical_state();
}

std::unique_ptr<FeatureTile> RotationLockFeaturePodController::CreateTile(
    bool compact) {
  DCHECK(!tile_);
  auto tile = std::make_unique<FeatureTile>(
      base::BindRepeating(&RotationLockFeaturePodController::OnIconPressed,
                          weak_factory_.GetWeakPtr()),
      /*is_togglable=*/true,
      compact ? FeatureTile::TileType::kCompact
              : FeatureTile::TileType::kPrimary);
  tile_ = tile.get();

  // The tile label is always "Auto rotate" and there is no sub-label when the
  // tile is `TileType::kPrimary`.
  tile_->SetLabel(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ROTATION_LOCK_AUTO));
  if (!compact) {
    tile_->SetSubLabelVisibility(false);
  }

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

void RotationLockFeaturePodController::OnUserRotationLockChanged() {
    if (!Shell::Get()
             ->tablet_mode_controller()
             ->is_in_tablet_physical_state()) {
      // Tablet mode transition results in a call to
      // `OnUserRotationLockChanged()`, but the rotation lock feature pod is not
      // visible in clamshell mode. The parent bubble is destroyed during the
      // tablet mode transition. Calling `UpdateTile()` which results in one of
      // two compact `FeatureTile`'s visibilities updating results in
      // undesirable UI. See b/272747756.
      return;
    }
    UpdateTile();
}

void RotationLockFeaturePodController::UpdateTile() {
  bool target_visibility = CalculateButtonVisibility();

  if (!tile_->GetVisible() && target_visibility) {
    TrackVisibilityUMA();
  }
  tile_->SetVisible(target_visibility);

  if (!target_visibility) {
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
