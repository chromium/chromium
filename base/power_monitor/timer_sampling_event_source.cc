// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/timer_sampling_event_source.h"

#include "base/check.h"

namespace base {

TimerSamplingEventSource::TimerSamplingEventSource(TimeDelta interval)
    : interval_(interval) {}

TimerSamplingEventSource::~TimerSamplingEventSource() = default;

bool TimerSamplingEventSource::Start(SamplingEventCallback callback) {
  DCHECK(callback);
  timer_.Start(FROM_HERE, interval_, std::move(callback));
  return true;
}

}  // namespace base
