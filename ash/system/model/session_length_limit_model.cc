// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/model/session_length_limit_model.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"

namespace ash {

namespace {

// If the remaining session time falls below this threshold, the user should be
// informed that the session is about to expire.
const int kExpiringSoonThresholdInMinutes = 5;

// Use 500ms interval for updates to notification and tray bubble to reduce the
// likelihood of a user-visible skip in high load situations (as might happen
// with 1000ms).
const int kTimerIntervalInMilliseconds = 500;

}  // namespace

SessionLengthLimitModel::SessionLengthLimitModel() {
  Shell::Get()->session_controller()->AddObserver(this);
}

SessionLengthLimitModel::~SessionLengthLimitModel() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void SessionLengthLimitModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SessionLengthLimitModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SessionLengthLimitModel::OnSessionStateChanged(
    session_manager::SessionState state) {
  Update();
}

void SessionLengthLimitModel::OnSessionLengthLimitChanged() {
  Update();
}

void SessionLengthLimitModel::Update() {
  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted())
    return;

  SessionControllerImpl* session = Shell::Get()->session_controller();
  base::TimeDelta time_limit = session->session_length_limit();
  base::TimeTicks session_start_time = session->session_start_time();
  if (!time_limit.is_zero() && !session_start_time.is_null()) {
    const base::TimeDelta expiring_soon_threshold(
        base::TimeDelta::FromMinutes(kExpiringSoonThresholdInMinutes));
    remaining_session_time_ =
        std::max(time_limit - (base::TimeTicks::Now() - session_start_time),
                 base::TimeDelta());
    limit_state_ = remaining_session_time_ <= expiring_soon_threshold
                       ? LIMIT_EXPIRING_SOON
                       : LIMIT_SET;
    if (!timer_)
      timer_ = std::make_unique<base::RepeatingTimer>();
    if (!timer_->IsRunning()) {
      timer_->Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kTimerIntervalInMilliseconds), this,
          &SessionLengthLimitModel::Update);
    }
  } else {
    remaining_session_time_ = base::TimeDelta();
    limit_state_ = LIMIT_NONE;
    timer_.reset();
  }

  for (auto& observer : observers_)
    observer.OnSessionLengthLimitUpdated();
}

}  // namespace ash
