// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_feature_pod_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/time_formatting.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

FocusModeFeaturePodController::FocusModeFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {
  FocusModeController::Get()->AddObserver(this);
}

FocusModeFeaturePodController::~FocusModeFeaturePodController() {
  FocusModeController::Get()->RemoveObserver(this);
}

FeaturePodButton* FocusModeFeaturePodController::CreateButton() {
  CHECK(!button_);
  CHECK(!features::IsQsRevampEnabled());
  std::unique_ptr<FeaturePodButton> button =
      std::make_unique<FeaturePodButton>(/*controller=*/this);
  button_ = button.get();
  button_->ShowDetailedViewArrow();
  button_->SetVectorIcon(kCaptureModeIcon);
  button_->SetLabel(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_FOCUS_MODE));
  button_->icon_button()->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_FOCUS_MODE));
  OnFocusModeChanged(FocusModeController::Get()->in_focus_session());
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
  tile_->SetLabel(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_FOCUS_MODE));
  tile_->SetIconButtonTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_FOCUS_MODE));
  tile_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_FOCUS_MODE));
  OnFocusModeChanged(FocusModeController::Get()->in_focus_session());
  return tile;
}

QsFeatureCatalogName FocusModeFeaturePodController::GetCatalogName() {
  return QsFeatureCatalogName::kFocusMode;
}

void FocusModeFeaturePodController::OnIconPressed() {
  FocusModeController::Get()->ToggleFocusMode();
}

void FocusModeFeaturePodController::OnLabelPressed() {
  TrackDiveInUMA();
  tray_controller_->ShowFocusModeDetailedView();
}

void FocusModeFeaturePodController::OnFocusModeChanged(bool in_focus_session) {
  if (features::IsQsRevampEnabled()) {
    CHECK(tile_);
    tile_->SetToggled(in_focus_session);
  } else {
    CHECK(button_);
    button_->SetToggled(in_focus_session);
  }

  UpdateUI();
}

void FocusModeFeaturePodController::OnTimerTick() {
  UpdateUI();
}

void FocusModeFeaturePodController::UpdateUI() {
  auto* controller = FocusModeController::Get();
  CHECK(controller);

  std::u16string sub_text;
  if (controller->in_focus_session()) {
    base::TimeDelta time_remaining = controller->end_time() - base::Time::Now();
    std::u16string remaining_time;
    if (!base::TimeDurationFormatWithSeconds(
            time_remaining, base::DURATION_WIDTH_SHORT, &remaining_time)) {
      remaining_time = base::UTF8ToUTF16(
          base::NumberToString(std::ceil(time_remaining.InSecondsF())));
    }
    sub_text = remaining_time;
  } else {
    sub_text = l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_FOCUS_MODE_TIME_SUBLABEL,
        base::FormatNumber(controller->session_duration().InMinutes()));
  }

  if (features::IsQsRevampEnabled()) {
    tile_->SetSubLabel(sub_text);
  } else {
    button_->SetSubLabel(sub_text);
  }
}
}  // namespace ash
