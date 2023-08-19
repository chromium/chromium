// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace {

FocusModeController* g_instance = nullptr;

// The default Focus Mode session duration, in minutes.
constexpr base::TimeDelta kDefaultSessionDuration = base::Minutes(30);

}  // namespace

FocusModeController::FocusModeController()
    : session_duration_(kDefaultSessionDuration) {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;

  // TODO(b/286932458): Get the Focus Mode session duration and DND setting from
  // user prefs.
}

FocusModeController::~FocusModeController() {
  if (in_focus_session_) {
    ToggleFocusMode();
  }

  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
FocusModeController* FocusModeController::Get() {
  CHECK(g_instance);
  return g_instance;
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
    previous_do_not_disturb_state_ = message_center->IsQuietMode();
    message_center->SetQuietMode(turn_on_do_not_disturb_);

    // Start timer for the specified `session_duration_`.
    end_time_ = base::Time::Now() + session_duration_;
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
