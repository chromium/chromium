// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_CHROMEVOX_SPOKEN_FEEDBACK_ENABLER_H_
#define ASH_ACCESSIBILITY_CHROMEVOX_SPOKEN_FEEDBACK_ENABLER_H_

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace ash {

// A class that enables ChromeVox based on a timer, providing progress
// sound feedback.
class ASH_EXPORT SpokenFeedbackEnabler {
 public:
  SpokenFeedbackEnabler();

  SpokenFeedbackEnabler(const SpokenFeedbackEnabler&) = delete;
  SpokenFeedbackEnabler& operator=(const SpokenFeedbackEnabler&) = delete;

  ~SpokenFeedbackEnabler();

 private:
  // Handles ticks of the timer.
  void OnTimer();

  // The start time.
  base::TimeTicks start_time_;

  // A timer that triggers repeatedly until either cancel or the desired time
  // elapsed.
  base::RepeatingTimer timer_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_CHROMEVOX_SPOKEN_FEEDBACK_ENABLER_H_
