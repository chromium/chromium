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

void DisplayPerformanceModeController::UpdateCurrentStateAndNotifyIfChanged() {
  ModeState new_state = PowerStatus::Get()->IsBatterySaverActive()
                            ? ModeState::kPowerSaver
                            : ModeState::kIntelligent;

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
