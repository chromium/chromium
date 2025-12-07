// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_session.h"

#include <algorithm>

#include "ash/system/focus_mode/focus_mode_util.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/time/time.h"

namespace ash {

FocusModeSession::FocusModeSession(const base::TimeDelta& session_duration,
                                   const base::Time& end_time)
    : session_duration_(session_duration), end_time_(end_time) {}

FocusModeSession::State FocusModeSession::GetState(
    const base::Time& now) const {
  if (now < end_time_) {
    return State::kOn;
  }

  if (persistent_ending_ ||
      now < (end_time_ + focus_mode_util::kInitialEndingMomentDuration)) {
    return State::kEnding;
  }

  return State::kOff;
}

FocusModeSession::Snapshot FocusModeSession::GetSnapshot(
    const base::Time& now) const {
  State state = GetState(now);
  base::TimeDelta remaining_time = GetTimeRemaining(now);
  base::TimeDelta time_elapsed = session_duration_ - remaining_time;
  double progress = time_elapsed / session_duration_;
  return {state, session_duration_, time_elapsed, remaining_time, progress};
}

void FocusModeSession::ExtendSession(const base::Time& now) {
  if (session_duration_ >= focus_mode_util::kMaximumDuration) {
    return;
  }

  const base::TimeDelta valid_new_session_duration =
      std::min(session_duration_ + focus_mode_util::kExtendDuration,
               focus_mode_util::kMaximumDuration);

  // Update `end_time_` only during the ending moment or if a focus session is
  // active.
  const base::TimeDelta session_duration_increase =
      valid_new_session_duration - session_duration_;
  CHECK(!session_duration_increase.is_negative());
  switch (GetState(now)) {
    case State::kOn:
      end_time_ += session_duration_increase;
      break;
    case State::kEnding:
      end_time_ = now + session_duration_increase;
      break;
    case State::kOff:
      break;
  }

  session_duration_ = valid_new_session_duration;
  persistent_ending_ = false;
}

base::TimeDelta FocusModeSession::GetTimeRemaining(
    const base::Time& now) const {
  base::TimeDelta remaining = end_time_ - now;
  return remaining.is_negative() ? base::TimeDelta() : remaining;
}

}  // namespace ash
