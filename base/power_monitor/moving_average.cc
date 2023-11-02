// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/moving_average.h"

#include <algorithm>
#include <limits>

#include "base/check_op.h"
#include "base/numerics/clamped_math.h"

namespace {
constexpr int kIntMax = std::numeric_limits<int>::max();
constexpr int64_t kInt64Max = std::numeric_limits<int64_t>::max();
}  // namespace

namespace base {

MovingAverage::MovingAverage(uint8_t window_size)
    : window_size_(window_size), buffer_(window_size, 0) {
  DCHECK_LE(kIntMax * window_size, kInt64Max);
}

MovingAverage::~MovingAverage() = default;

void MovingAverage::AddSample(int sample) {
  sum_ -= buffer_[index_];
  buffer_[index_++] = sample;
  sum_ += sample;
  if (index_ == window_size_) {
    full_ = true;
    index_ = 0;
  }
}

int MovingAverage::GetAverageRoundedDown() const {
  if (Size() == 0 || uint64_t{Size()} > static_cast<uint64_t>(kInt64Max)) {
    return 0;
  }
  return static_cast<int>(sum_ / static_cast<int64_t>(Size()));
}

int MovingAverage::GetAverageRoundedToClosest() const {
  if (Size() == 0 || uint64_t{Size()} > static_cast<uint64_t>(kInt64Max))
    return 0;
  return static_cast<int>((base::ClampedNumeric<int64_t>(sum_) + Size() / 2) /
                          static_cast<int64_t>(Size()));
}

double MovingAverage::GetUnroundedAverage() const {
  if (Size() == 0)
    return 0;
  return sum_ / static_cast<double>(Size());
}

void MovingAverage::Reset() {
  std::fill(buffer_.begin(), buffer_.end(), 0);
  sum_ = 0;
  index_ = 0;
  full_ = false;
}

size_t MovingAverage::Size() const {
  return full_ ? window_size_ : index_;
}
}  // namespace base
