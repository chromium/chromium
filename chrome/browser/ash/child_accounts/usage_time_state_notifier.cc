// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/usage_time_state_notifier.h"

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "components/session_manager/core/session_manager.h"

namespace ash {

namespace {

UsageTimeStateNotifier::UsageTimeState GetCurrentState() {
  if (session_manager::SessionManager::Get()->session_state() ==
      session_manager::SessionState::ACTIVE) {
    return UsageTimeStateNotifier::UsageTimeState::ACTIVE;
  } else {
    return UsageTimeStateNotifier::UsageTimeState::INACTIVE;
  }
}

}  // namespace

UsageTimeStateNotifier::UsageTimeStateNotifier()
    : last_state_(GetCurrentState()) {}

UsageTimeStateNotifier::~UsageTimeStateNotifier() {}

// static
UsageTimeStateNotifier* UsageTimeStateNotifier::GetInstance() {
  static base::NoDestructor<UsageTimeStateNotifier> instance;
  return instance.get();
}

void UsageTimeStateNotifier::AddObserver(
    UsageTimeStateNotifier::Observer* observer) {
  DCHECK(observer);
  if (observers_.empty()) {
    session_manager::SessionManager::Get()->AddObserver(this);
    chromeos::PowerManagerClient::Get()->AddObserver(this);
    last_state_ = GetCurrentState();
  }
  observers_.AddObserver(observer);
}

void UsageTimeStateNotifier::RemoveObserver(
    UsageTimeStateNotifier::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
  if (observers_.empty()) {
    session_manager::SessionManager::Get()->RemoveObserver(this);
    chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  }
}

UsageTimeStateNotifier::UsageTimeState UsageTimeStateNotifier::GetState()
    const {
  return last_state_;
}

void UsageTimeStateNotifier::ChangeUsageTimeState(
    UsageTimeStateNotifier::UsageTimeState state) {
  if (state == last_state_)
    return;
  last_state_ = state;
  for (auto& observer : observers_) {
    observer.OnUsageTimeStateChange(last_state_);
  }
}

void UsageTimeStateNotifier::OnSessionStateChanged() {
  TRACE_EVENT0("ui", "UsageTimeStateNotifier::OnSessionStateChanged");
  ChangeUsageTimeState(GetCurrentState());
}

void UsageTimeStateNotifier::ScreenIdleStateChanged(
    const power_manager::ScreenIdleState& state) {
  // State should be inactive only if the screen is turned off.
  if (!state.off() && session_manager::SessionManager::Get()->session_state() ==
                          session_manager::SessionState::ACTIVE) {
    ChangeUsageTimeState(UsageTimeState::ACTIVE);
  } else {
    ChangeUsageTimeState(UsageTimeState::INACTIVE);
  }
}

void UsageTimeStateNotifier::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  ChangeUsageTimeState(UsageTimeState::INACTIVE);
}

void UsageTimeStateNotifier::SuspendDone(base::TimeDelta sleep_duration) {
  ChangeUsageTimeState(GetCurrentState());
}

}  // namespace ash
