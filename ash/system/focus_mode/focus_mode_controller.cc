// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ui/message_center/message_center.h"

namespace ash {

FocusModeController::FocusModeController() = default;

FocusModeController::~FocusModeController() {
  if (in_focus_session_) {
    ToggleFocusMode();
  }
}

void FocusModeController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FocusModeController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FocusModeController::ToggleFocusMode() {
  auto* message_center = message_center::MessageCenter::Get();
  CHECK(message_center);

  if (in_focus_session_) {
    timer_.Stop();

    // Restore previous DND state.
    message_center->SetQuietMode(previous_do_not_disturb_state_);
  } else {
    // Turn on DND.
    // TODO: Set DND based on user input.
    previous_do_not_disturb_state_ = message_center->IsQuietMode();
    message_center->SetQuietMode(true);

    // Start timer for 30 minutes.
    // TODO: Set `end_time_` based on user input.
    end_time_ = base::Time::Now() + base::Minutes(30);
    timer_.Start(FROM_HERE, base::Seconds(1), this,
                 &FocusModeController::OnTimerTick, base::TimeTicks::Now());
  }

  in_focus_session_ = !in_focus_session_;

  for (auto& observer : observers_) {
    observer.OnFocusModeChanged(in_focus_session_);
  }
}

void FocusModeController::OnTimerTick() {
  if (in_focus_session_ && base::Time::Now() >= end_time_) {
    ToggleFocusMode();
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTimerTick();
  }
}

}  // namespace ash
