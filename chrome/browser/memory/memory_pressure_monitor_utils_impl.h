// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_MEMORY_PRESSURE_MONITOR_UTILS_IMPL_H_
#define CHROME_BROWSER_MEMORY_MEMORY_PRESSURE_MONITOR_UTILS_IMPL_H_

#ifndef CHROME_BROWSER_MEMORY_MEMORY_PRESSURE_MONITOR_UTILS_H_
#error This file is meant to be included by memory_pressure_monitor_utils.h.
#endif

#include <utility>

#include "base/time/default_tick_clock.h"

namespace memory {
namespace internal {

template <typename T>
ObservationWindow<T>::ObservationWindow(const base::TimeDelta window_length)
    : window_length_(window_length),
      clock_(base::DefaultTickClock::GetInstance()) {}

template <typename T>
ObservationWindow<T>::~ObservationWindow() = default;

template <typename T>
void ObservationWindow<T>::OnSample(const T sample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::TimeTicks now = clock_->NowTicks();
  // Trim some samples if needed.
  while (observations_.size() &&
         (observations_.begin()->first < (now - window_length_))) {
    OnSampleRemoved(observations_.begin()->second);
    observations_.pop_front();
  }

  // Add the new sample to the observations.
  OnSampleAdded(sample);
  observations_.emplace_back(std::make_pair(now, std::move(sample)));
}

}  // namespace internal
}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_MEMORY_PRESSURE_MONITOR_UTILS_IMPL_H_
