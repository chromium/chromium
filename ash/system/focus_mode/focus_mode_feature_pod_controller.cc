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
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

FocusModeFeaturePodController::FocusModeFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {
  CHECK(features::IsFocusModeEnabled());
  FocusModeController::Get()->AddObserver(this);
}

FocusModeFeaturePodController::~FocusModeFeaturePodController() {
  FocusModeController::Get()->RemoveObserver(this);
}


std::unique_ptr<FeatureTile> FocusModeFeaturePodController::CreateTile(
    bool compact) {
  auto tile = std::make_unique<FeatureTile>(
      base::BindRepeating(&FocusModeFeaturePodController::OnLabelPressed,
                          weak_factory_.GetWeakPtr()));
  tile_ = tile.get();

  const bool target_visibility =
      !Shell::Get()->session_controller()->IsUserSessionBlocked();
  tile_->SetVisible(target_visibility);
  if (target_visibility) {
    TrackVisibilityUMA();
  }

  tile_->SetIconClickable(true);
  tile_->SetIconClickCallback(
      base::BindRepeating(&FocusModeFeaturePodController::OnIconPressed,
                          weak_factory_.GetWeakPtr()));
  tile_->CreateDecorativeDrillInArrow();
  tile_->SetVectorIcon(kFocusModeLampIcon);
  tile_->SetToggled(FocusModeController::Get()->in_focus_session());
  UpdateUI();
  return tile;
}

QsFeatureCatalogName FocusModeFeaturePodController::GetCatalogName() {
  return QsFeatureCatalogName::kFocusMode;
}

void FocusModeFeaturePodController::OnIconPressed() {
  auto* controller = FocusModeController::Get();

  // As part of the first time user flow, if the user has never started a
  // session before, we want to direct them to the focus panel so they can get
  // more context and change focus settings instead of starting a session
  // immediately.
  if (!controller->HasStartedSessionBefore()) {
    OnLabelPressed();
    return;
  }

  TrackToggleUMA(/*target_toggle_state=*/!controller->in_focus_session());
  controller->ToggleFocusMode();
}

void FocusModeFeaturePodController::OnLabelPressed() {
  TrackDiveInUMA();
  tray_controller_->ShowFocusModeDetailedView();
}

void FocusModeFeaturePodController::OnFocusModeChanged(bool in_focus_session) {
  UpdateUI();
  tile_->SetToggled(in_focus_session);
}

void FocusModeFeaturePodController::OnTimerTick() {
  UpdateUI();
}

void FocusModeFeaturePodController::OnSessionDurationChanged() {
  UpdateUI();
}

void FocusModeFeaturePodController::UpdateUI() {
  auto* controller = FocusModeController::Get();
  CHECK(controller);
  CHECK(tile_);

  const bool in_focus_session = controller->in_focus_session();
  const std::u16string label_text = l10n_util::GetStringUTF16(
      in_focus_session ? IDS_ASH_STATUS_TRAY_FOCUS_MODE_TOGGLE_ACTIVE_LABEL
                       : IDS_ASH_STATUS_TRAY_FOCUS_MODE);
  tile_->SetLabel(label_text);
  tile_->SetIconButtonTooltipText(label_text);
  tile_->SetTooltipText(label_text);

  // As part of the first time user flow, if the user has never started a
  // session before, we hide the session duration sublabel since they are not
  // able to start a focus session immediately anyway.
  if (!controller->HasStartedSessionBefore()) {
    tile_->SetSubLabelVisibility(false);
    return;
  }

  const base::TimeDelta session_duration_remaining =
      in_focus_session ? controller->end_time() - base::Time::Now()
                       : controller->session_duration();

  // We only show seconds if there is less than a minute remaining. This can
  // only happen during a focus session.
  // TODO(b/302044981): `UpdateUI` shouldn't have to know about internal
  // implementation details of `focus_mode_util` (i.e. needing to round up the
  // seconds, which time format to use, etc.) in order to function correctly. We
  // should clean this up to provide a better API.
  const bool should_show_seconds =
      base::ClampRound<int64_t>(session_duration_remaining.InSecondsF()) <
      base::Time::kSecondsPerMinute;
  const std::u16string duration_string = focus_mode_util::GetDurationString(
      session_duration_remaining,
      should_show_seconds ? focus_mode_util::TimeFormatType::kFull
                          : focus_mode_util::TimeFormatType::kMinutesOnly);
  tile_->SetSubLabel(
      in_focus_session
          ? l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_FOCUS_MODE_TIME_SUBLABEL, duration_string)
          : duration_string);
}

}  // namespace ash
