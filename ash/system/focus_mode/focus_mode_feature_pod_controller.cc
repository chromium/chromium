// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_feature_pod_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_session.h"
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

  const auto* session_controller = Shell::Get()->session_controller();
  std::optional<user_manager::UserType> user_type =
      session_controller->GetUserType();
  // Only show the focus mode tile if it is a regular user or if the user
  // session is not blocked (e.g. lock screen).
  const bool should_show_tile =
      (user_type && (*user_type == user_manager::UserType::kRegular ||
                     *user_type == user_manager::UserType::kChild)) &&
      !session_controller->IsUserSessionBlocked();

  tile_->SetVisible(should_show_tile);
  if (should_show_tile) {
    TrackVisibilityUMA();
  }

  tile_->SetIconClickable(true);
  tile_->SetIconClickCallback(
      base::BindRepeating(&FocusModeFeaturePodController::OnIconPressed,
                          weak_factory_.GetWeakPtr()));
  tile_->CreateDecorativeDrillInArrow();
  tile_->SetVectorIcon(kFocusModeLampIcon);
  tile_->icon_button()->SetFlipCanvasOnPaintForRTLUI(false);
  auto* controller = FocusModeController::Get();
  tile_->SetToggled(controller->in_focus_session());
  UpdateUI(controller->GetSnapshot(base::Time::Now()));
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
  controller->ToggleFocusMode(
      focus_mode_histogram_names::ToggleSource::kFeaturePod);
}

void FocusModeFeaturePodController::OnLabelPressed() {
  TrackDiveInUMA();
  tray_controller_->ShowFocusModeDetailedView();
}

void FocusModeFeaturePodController::OnFocusModeChanged(
    FocusModeSession::State session_state) {
  UpdateUI(FocusModeController::Get()->GetSnapshot(base::Time::Now()));
  const bool in_focus_session = session_state == FocusModeSession::State::kOn;
  tile_->SetToggled(in_focus_session);
}

void FocusModeFeaturePodController::OnTimerTick(
    const FocusModeSession::Snapshot& session_snapshot) {
  UpdateUI(session_snapshot);
}

void FocusModeFeaturePodController::OnInactiveSessionDurationChanged(
    const base::TimeDelta& session_duration) {
  CHECK(tile_);

  if (!FocusModeController::Get()->HasStartedSessionBefore()) {
    return;
  }

  const std::u16string duration_string = focus_mode_util::GetDurationString(
      session_duration, /*digital_format=*/false);
  tile_->SetSubLabel(duration_string);
  tile_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TILE_INACTIVE, duration_string));
  tile_->SetIconButtonTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TOGGLE_BUTTON_INACTIVE, duration_string));
}

void FocusModeFeaturePodController::OnActiveSessionDurationChanged(
    const FocusModeSession::Snapshot& session_snapshot) {
  UpdateUI(session_snapshot);
}

void FocusModeFeaturePodController::UpdateUI(
    const FocusModeSession::Snapshot& session_snapshot) {
  auto* controller = FocusModeController::Get();
  CHECK(controller);
  CHECK(tile_);

  const bool in_focus_session =
      session_snapshot.state == FocusModeSession::State::kOn;
  tile_->SetLabel(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_FOCUS_MODE));

  // As part of the first time user flow, if the user has never started a
  // session before, we hide the session duration sublabel since they are not
  // able to start a focus session immediately anyway.
  if (!controller->HasStartedSessionBefore()) {
    const std::u16string tooltip_text =
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_FOCUS_MODE_TILE_INITIAL);
    tile_->SetTooltipText(tooltip_text);
    tile_->SetIconButtonTooltipText(tooltip_text);
    tile_->SetSubLabelVisibility(false);
    tile_->SetIconClickable(false);
    return;
  }

  const base::TimeDelta session_duration_remaining =
      in_focus_session ? session_snapshot.remaining_time
                       : controller->GetSessionDuration();
  const std::u16string duration_string = focus_mode_util::GetDurationString(
      session_duration_remaining, /*digital_format=*/false);
  tile_->SetSubLabel(
      in_focus_session
          ? l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_FOCUS_MODE_TIME_SUBLABEL, duration_string)
          : duration_string);

  const std::u16string tooltip_duration_string =
      session_duration_remaining < base::Minutes(1)
          ? l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_FOCUS_MODE_SESSION_LESS_THAN_ONE_MINUTE)
          : duration_string;
  tile_->SetTooltipText(l10n_util::GetStringFUTF16(
      in_focus_session ? IDS_ASH_STATUS_TRAY_FOCUS_MODE_TILE_ACTIVE
                       : IDS_ASH_STATUS_TRAY_FOCUS_MODE_TILE_INACTIVE,
      tooltip_duration_string));
  tile_->SetIconClickable(true);
  tile_->SetIconButtonTooltipText(l10n_util::GetStringFUTF16(
      in_focus_session ? IDS_ASH_STATUS_TRAY_FOCUS_MODE_TILE_BUTTON_ACTIVE
                       : IDS_ASH_STATUS_TRAY_FOCUS_MODE_TOGGLE_BUTTON_INACTIVE,
      tooltip_duration_string));
}

}  // namespace ash
