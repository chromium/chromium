// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_feature_pod_controller.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

CaptureModeFeaturePodController::CaptureModeFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {}

CaptureModeFeaturePodController::~CaptureModeFeaturePodController() = default;

// static
bool CaptureModeFeaturePodController::CalculateButtonVisibility() {
  return !Shell::Get()->session_controller()->IsUserSessionBlocked();
}

std::unique_ptr<FeatureTile> CaptureModeFeaturePodController::CreateTile(
    bool compact) {
  auto feature_tile = std::make_unique<FeatureTile>(
      base::BindRepeating(&FeaturePodControllerBase::OnIconPressed,
                          weak_ptr_factory_.GetWeakPtr()),
      /*is_togglable=*/false,
      compact ? FeatureTile::TileType::kCompact
              : FeatureTile::TileType::kPrimary);

  const bool target_visibility = CalculateButtonVisibility();
  feature_tile->SetVisible(target_visibility);
  if (target_visibility) {
    TrackVisibilityUMA();
  }

  feature_tile->SetVectorIcon(kCaptureModeIcon);
  const auto label_text =
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAPTURE_MODE_BUTTON_LABEL);
  feature_tile->SetLabel(label_text);
  if (!compact) {
    feature_tile->SetSubLabelVisibility(false);
  }
  feature_tile->SetTooltipText(label_text);
  return feature_tile;
}

QsFeatureCatalogName CaptureModeFeaturePodController::GetCatalogName() {
  return QsFeatureCatalogName::kCaptureMode;
}

void CaptureModeFeaturePodController::OnIconPressed() {
  TrackToggleUMA(/*target_toggle_state=*/true);

  // Close the system tray bubble. Deletes |this|.
  tray_controller_->CloseBubble();

  CaptureModeController::Get()->Start(CaptureModeEntryType::kQuickSettings);
}

}  // namespace ash
