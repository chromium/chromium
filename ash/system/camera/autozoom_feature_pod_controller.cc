// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/camera/autozoom_feature_pod_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/camera/autozoom_controller_impl.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {
namespace {

bool IsButtonVisible() {
  return Shell::Get()->autozoom_controller()->IsAutozoomControlEnabled() &&
         Shell::Get()->session_controller()->ShouldEnableSettings();
}

}  // namespace

AutozoomFeaturePodController::AutozoomFeaturePodController() {
  Shell::Get()->autozoom_controller()->AddObserver(this);
}

AutozoomFeaturePodController::~AutozoomFeaturePodController() {
  Shell::Get()->autozoom_controller()->RemoveObserver(this);
}

FeaturePodButton* AutozoomFeaturePodController::CreateButton() {
  DCHECK(!button_);
  DCHECK(!features::IsQsRevampEnabled());
  button_ = new FeaturePodButton(this);
  button_->SetVectorIcon(kUnifiedMenuAutozoomIcon);

  button_->SetLabel(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUTOZOOM_BUTTON_LABEL));
  auto description = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_AUTOZOOM_TOGGLE_ACCESSIBILITY_DESCRIPTION);
  button_->icon_button()->GetViewAccessibility().OverrideDescription(
      description);
  button_->label_button()->GetViewAccessibility().OverrideDescription(
      description);
  // Init the button with invisible state. The `UpdateButton` method will update
  // the visibility based on the current condition.
  button_->SetVisible(false);
  UpdateButton(Shell::Get()->autozoom_controller()->GetState());
  return button_;
}

std::unique_ptr<FeatureTile> AutozoomFeaturePodController::CreateTile(
    bool compact) {
  DCHECK(!tile_);
  DCHECK(features::IsQsRevampEnabled());
  auto tile = std::make_unique<FeatureTile>(
      base::BindRepeating(&AutozoomFeaturePodController::OnIconPressed,
                          weak_factory_.GetWeakPtr()));
  tile_ = tile.get();
  tile_->SetVectorIcon(kUnifiedMenuAutozoomIcon);

  tile_->SetLabel(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUTOZOOM_BUTTON_LABEL));
  auto description = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_AUTOZOOM_TOGGLE_ACCESSIBILITY_DESCRIPTION);
  tile_->GetViewAccessibility().OverrideDescription(description);
  // `UpdateButton` will update visibility.
  tile_->SetVisible(false);
  UpdateButton(Shell::Get()->autozoom_controller()->GetState());
  return tile;
}

QsFeatureCatalogName AutozoomFeaturePodController::GetCatalogName() {
  return QsFeatureCatalogName::kAutozoom;
}

void AutozoomFeaturePodController::OnIconPressed() {
  TrackToggleUMA(
      /*target_toggle_state=*/Shell::Get()->autozoom_controller()->GetState() !=
      cros::mojom::CameraAutoFramingState::ON_SINGLE);
  Shell::Get()->autozoom_controller()->Toggle();
}

void AutozoomFeaturePodController::UpdateButtonVisibility() {
  if (!button_)
    return;

  const bool visible = IsButtonVisible();

  if (!button_->GetVisible() && visible)
    TrackVisibilityUMA();

  button_->SetVisible(visible);
}

void AutozoomFeaturePodController::UpdateTileVisibility() {
  if (!tile_) {
    return;
  }
  const bool visible = IsButtonVisible();
  if (!tile_->GetVisible() && visible) {
    TrackVisibilityUMA();
  }
  tile_->SetVisible(visible);
}

void AutozoomFeaturePodController::OnAutozoomStateChanged(
    cros::mojom::CameraAutoFramingState state) {
  UpdateButton(state);
}

void AutozoomFeaturePodController::OnAutozoomControlEnabledChanged(
    bool enabled) {
  if (features::IsQsRevampEnabled()) {
    UpdateTileVisibility();
  } else {
    UpdateButtonVisibility();
  }
}

void AutozoomFeaturePodController::UpdateButton(
    cros::mojom::CameraAutoFramingState state) {
  const bool is_qs_revamp_enabled = features::IsQsRevampEnabled();
  if (is_qs_revamp_enabled) {
    if (!tile_) {
      return;
    }
    tile_->SetToggled(state != cros::mojom::CameraAutoFramingState::OFF);
    UpdateTileVisibility();
  } else {
    if (!button_) {
      return;
    }
    button_->SetToggled(state != cros::mojom::CameraAutoFramingState::OFF);
    UpdateButtonVisibility();
  }

  std::u16string tooltip_state;
  std::u16string button_label;

  switch (state) {
    case cros::mojom::CameraAutoFramingState::OFF:
      button_label =
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUTOZOOM_OFF_STATE);
      tooltip_state = l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUTOZOOM_OFF_STATE_TOOLTIP);
      break;
    case cros::mojom::CameraAutoFramingState::ON_SINGLE:
    case cros::mojom::CameraAutoFramingState::ON_MULTI:
      button_label =
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUTOZOOM_ON_STATE);
      tooltip_state = l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUTOZOOM_ON_STATE_TOOLTIP);
      break;
  }

  if (is_qs_revamp_enabled) {
    tile_->SetSubLabel(button_label);
    tile_->SetTooltipText(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_AUTOZOOM_TOGGLE_TOOLTIP, tooltip_state));
  } else {
    button_->SetSubLabel(button_label);
    button_->SetIconAndLabelTooltips(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_AUTOZOOM_TOGGLE_TOOLTIP, tooltip_state));
  }
}

}  // namespace ash
