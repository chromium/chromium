// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_performance_mode_controller.h"

namespace ash {

using ModeState = DisplayPerformanceModeController::ModeState;

DisplayPerformanceModeController::DisplayPerformanceModeController()
    : power_status_(PowerStatus::Get()->GetWeakPtr()) {
  power_status_->AddObserver(this);
}

DisplayPerformanceModeController::~DisplayPerformanceModeController() {
  if (power_status_) {
    power_status_->RemoveObserver(this);
  }
}

ModeState DisplayPerformanceModeController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  return current_state_;
}

void DisplayPerformanceModeController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DisplayPerformanceModeController::OnPowerStatusChanged() {
  UpdateCurrentStateAndNotifyIfChanged();
}

void DisplayPerformanceModeController::SetHighPerformanceModeByUser(
    bool is_high_performance_enabled) {
  is_high_performance_enabled_ = is_high_performance_enabled;
  UpdateCurrentStateAndNotifyIfChanged();
}

void DisplayPerformanceModeController::UpdateCurrentStateAndNotifyIfChanged() {
  //   Implementation Logic:
  //   1. If the user has enabled the high performance mode in the UI, then the
  //      display features should be in the high performance mode regardless of
  //      the power status.
  //   2. If the user has not enabled the high performance mode in the UI, then
  //      the display features should be in the power saver mode if the power
  //      status is in battery saver mode.
  //   3. If the user has not enabled the high performance mode in the UI, then
  //      the display features should be in the intelligent mode if the power
  //      status is not in battery saver mode.
  ModeState new_state = ModeState::kIntelligent;
  if (is_high_performance_enabled_) {
    new_state = ModeState::kHighPerformance;
  } else if (PowerStatus::Get()->IsBatterySaverActive()) {
    new_state = ModeState::kPowerSaver;
  }

  if (new_state != current_state_) {
    current_state_ = new_state;
    NotifyObservers();
  }
}

void DisplayPerformanceModeController::NotifyObservers() {
  for (Observer& observer : observers_) {
    observer.OnDisplayPerformanceModeChanged(current_state_);
  }
}

}  // namespace ash
