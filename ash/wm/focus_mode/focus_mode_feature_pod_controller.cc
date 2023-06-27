// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/focus_mode/focus_mode_feature_pod_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/check.h"
#include "base/functional/bind.h"

namespace ash {

namespace {

// TODO(b/288975135): replace these placeholders with string ids later.
constexpr const char16_t kLabelText[] = u"Focus Mode";
constexpr const char16_t kSubLabelText[] = u"30 mins";

}  // namespace

FocusModeFeaturePodController::FocusModeFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {}

FocusModeFeaturePodController::~FocusModeFeaturePodController() = default;

FeaturePodButton* FocusModeFeaturePodController::CreateButton() {
  CHECK(!button_);
  CHECK(!features::IsQsRevampEnabled());
  std::unique_ptr<FeaturePodButton> button =
      std::make_unique<FeaturePodButton>(/*controller=*/this);
  button_ = button.get();
  button_->ShowDetailedViewArrow();
  button_->SetVectorIcon(kCaptureModeIcon);
  button_->SetLabel(kLabelText);
  button_->SetSubLabel(kSubLabelText);
  button_->icon_button()->SetTooltipText(kLabelText);
  button_->SetLabelTooltip(kSubLabelText);
  button_->SetToggled(false);
  return button.release();
}

std::unique_ptr<FeatureTile> FocusModeFeaturePodController::CreateTile(
    bool compact) {
  CHECK(features::IsQsRevampEnabled());
  auto tile = std::make_unique<FeatureTile>(
      base::BindRepeating(&FocusModeFeaturePodController::OnLabelPressed,
                          weak_factory_.GetWeakPtr()));
  tile_ = tile.get();
  tile_->SetIconClickable(true);
  tile_->SetIconClickCallback(
      base::BindRepeating(&FocusModeFeaturePodController::OnIconPressed,
                          weak_factory_.GetWeakPtr()));
  tile_->CreateDecorativeDrillInArrow();
  tile_->SetVectorIcon(kCaptureModeIcon);
  tile_->SetLabel(kLabelText);
  tile_->SetSubLabel(kSubLabelText);
  tile_->SetIconButtonTooltipText(kLabelText);
  tile_->SetTooltipText(kLabelText);
  tile_->SetToggled(false);
  return tile;
}

QsFeatureCatalogName FocusModeFeaturePodController::GetCatalogName() {
  return QsFeatureCatalogName::kFocusMode;
}

void FocusModeFeaturePodController::OnIconPressed() {
  // TODO(b/286931230): Toggle Focus Mode.
  TrackToggleUMA(/*target_toggle_state=*/false);
}

void FocusModeFeaturePodController::OnLabelPressed() {
  TrackDiveInUMA();
  tray_controller_->ShowFocusModeDetailedView();
}

}  // namespace ash
