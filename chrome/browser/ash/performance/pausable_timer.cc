// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/performance/pausable_timer.h"

#include "base/time/time.h"

namespace ash {

PausableTimer::PausableTimer() = default;
PausableTimer::~PausableTimer() = default;

void PausableTimer::Start(base::OnceClosure callback) {
  DCHECK(!timer_.IsRunning());
  if (!remaining_duration_.is_positive()) {
    return;
  }
  timer_.Start(FROM_HERE, remaining_duration_, std::move(callback));
  last_started_ = base::TimeTicks::Now();
}

void PausableTimer::Pause() {
  DCHECK(timer_.IsRunning());
  timer_.Stop();
  const base::TimeDelta passed = base::TimeTicks::Now() - last_started_;
  remaining_duration_ =
      std::max(remaining_duration_ - passed, base::TimeDelta());
}

void PausableTimer::Stop() {
  timer_.Stop();
  remaining_duration_ = base::TimeDelta();
}

void PausableTimer::set_remaining_duration(base::TimeDelta duration) {
  DCHECK(!timer_.IsRunning());
  remaining_duration_ = duration;
}

}  // namespace ash
