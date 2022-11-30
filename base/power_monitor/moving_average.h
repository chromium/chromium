// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_POWER_MONITOR_MOVING_AVERAGE_H_
#define BASE_POWER_MONITOR_MOVING_AVERAGE_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/base_export.h"

namespace base {

// Calculates average over a small fixed size window. If there are less than
// window size elements, calculates average of all inserted elements so far.
// This implementation support a maximum window size of 255.
// Ported from third_party/webrtc/rtc_base/numerics/moving_average.h.
class BASE_EXPORT MovingAverage {
 public:
  // Maximum supported window size is 2^8 - 1 = 255.
  explicit MovingAverage(uint8_t window_size);
  ~MovingAverage();
  // MovingAverage is neither copyable nor movable.
  MovingAverage(const MovingAverage&) = delete;
  MovingAverage& operator=(const MovingAverage&) = delete;

  // Adds new sample. If the window is full, the oldest element is pushed out.
  void AddSample(int sample);

  // Returns rounded down average of last `window_size` elements or all
  // elements if there are not enough of them.
  int GetAverageRoundedDown() const;

  // Same as above but rounded to the closest integer.
  int GetAverageRoundedToClosest() const;

  // Returns unrounded average over the window.
  double GetUnroundedAverage() const;

  // Resets to the initial state before any elements were added.
  void Reset();

  // Returns number of elements in the window.
  size_t Size() const;

 private:
  // Stores `window_size` used in the constructor.
  uint8_t window_size_ = 0;
  // New samples are added at this index. Counts modulo `window_size`.
  uint8_t index_ = 0;
  // Set to true when the `buffer_` is full. i.e, all elements contain a
  // sample added by AddSample().
  bool full_ = false;
  // Sum of the samples in the moving window.
  int64_t sum_ = 0;
  // Circular buffer for all the samples in the moving window.
  // Size is always `window_size`
  std::vector<int> buffer_;
};

}  // namespace base

#endif  // BASE_POWER_MONITOR_MOVING_AVERAGE_H_
