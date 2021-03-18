// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_TIMER_SLACK_H_
#define BASE_MESSAGE_LOOP_TIMER_SLACK_H_

#include "base/base_export.h"
#include "base/time/time.h"

namespace base {

struct Feature;

// Amount of timer slack to use for delayed timers.  Increasing timer slack
// allows the OS to coalesce timers more effectively.
enum TimerSlack {
  // Lowest value for timer slack allowed by OS.
  TIMER_SLACK_NONE,

  // Maximal value for timer slack allowed by OS.
  TIMER_SLACK_MAXIMUM
};

// Returns true if the ludicrous timer slack experiment is enabled.
BASE_EXPORT bool IsLudicrousTimerSlackEnabled();

// Returns the slack for the experiment.
BASE_EXPORT base::TimeDelta GetLudicrousTimerSlack();

namespace features {

// Exposed for testing.
BASE_EXPORT extern const base::Feature kLudicrousTimerSlack;

}  // namespace features

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_TIMER_SLACK_H_
