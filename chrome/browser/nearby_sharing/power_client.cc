// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/power_client.h"

PowerClient::PowerClient() {
  if (chromeos::PowerManagerClient::Get())
    chromeos::PowerManagerClient::Get()->AddObserver(this);
}

PowerClient::~PowerClient() {
  if (chromeos::PowerManagerClient::Get())
    chromeos::PowerManagerClient::Get()->RemoveObserver(this);
}

void PowerClient::AddObserver(PowerClient::Observer* observer) {
  observers_.AddObserver(observer);
}

void PowerClient::RemoveObserver(PowerClient::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PowerClient::IsSuspended() {
  return is_suspended_;
}

bool PowerClient::IsScreenOn() {
  return is_screen_on_;
}

void PowerClient::SetSuspended(bool is_suspended) {
  is_suspended_ = is_suspended;
  for (auto& observer : observers_) {
    if (is_suspended)
      observer.SuspendImminent();
    else
      observer.SuspendDone();
  }
}

void PowerClient::SetScreenOn(bool is_screen_on) {
  is_screen_on_ = is_screen_on;
  screen_state_notify_timer_.Stop();
  for (auto& observer : observers_) {
    observer.ScreenStateChanged(is_screen_on_);
  }
}

void PowerClient::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  SetSuspended(true);
}

void PowerClient::SuspendDone(base::TimeDelta sleep_duration) {
  SetSuspended(false);
}

void PowerClient::ScreenIdleStateChanged(
    const power_manager::ScreenIdleState& state) {
  bool new_state_on = !state.off();
  if (is_screen_on_ && !new_state_on) {
    screen_state_notify_timer_.Start(
        FROM_HERE, base::TimeDelta::FromMinutes(1),
        base::BindOnce(&PowerClient::SetScreenOn, base::Unretained(this),
                       new_state_on));
  } else if (!is_screen_on_ && new_state_on) {
    SetScreenOn(new_state_on);
  }
}
