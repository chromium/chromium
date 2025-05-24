// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PERFORMANCE_PAUSABLE_TIMER_H_
#define CHROME_BROWSER_ASH_PERFORMANCE_PAUSABLE_TIMER_H_

#include "base/time/time.h"
#include "base/timer/timer.h"

namespace ash {

// A pausable timer that tracks an elapsed duration before calling a
// given callback.
class PausableTimer {
 public:
  PausableTimer();
  PausableTimer(const PausableTimer&) = delete;
  PausableTimer& operator=(const PausableTimer&) = delete;
  ~PausableTimer();

  void Start(base::OnceClosure callback);

  void Pause();

  void Stop();

  bool IsRunning() const { return timer_.IsRunning(); }

  void set_remaining_duration(base::TimeDelta duration);

  base::TimeDelta get_remaining_duration() const { return remaining_duration_; }

 private:
  base::OneShotTimer timer_;
  base::TimeTicks last_started_;
  base::TimeDelta remaining_duration_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PERFORMANCE_PAUSABLE_TIMER_H_
