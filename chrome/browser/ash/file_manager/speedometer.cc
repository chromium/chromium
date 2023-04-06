// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/file_manager/speedometer.h"

#include <algorithm>

#include "base/time/time.h"

namespace file_manager {
namespace io_task {

Speedometer::Speedometer() : start_time_(base::TimeTicks::Now()) {}

void Speedometer::SetTotalBytes(int64_t total_bytes) {
  total_bytes_ = total_bytes;
}

size_t Speedometer::GetSampleCount() const {
  // While the buffer isn't full, we want the CurrentIndex().
  return std::min(samples_.CurrentIndex(), samples_.BufferSize());
}

double Speedometer::GetRemainingSeconds() const {
  // Not interpolated yet or not enough samples.
  if (projected_end_time_ == 0) {
    return 0;
  }

  return (projected_end_time_ -
          (base::TimeTicks::Now() - start_time_).InMillisecondsF()) /
         1000;
}

void Speedometer::Update(int64_t total_processed_bytes) {
  SpeedSample sample;

  // Is this the first sample?
  sample.time = base::TimeTicks::Now();
  sample.bytes_count = total_processed_bytes;
  if (GetSampleCount() == 0) {
    AppendSample(sample);
    return;
  }

  const auto* last = *samples_.End();
  // Drop this sample if we received the previous samples less than 1 second
  // ago.
  if (sample.time - last->time < base::Seconds(1)) {
    return;
  }

  AppendSample(sample);
}

void Speedometer::Interpolate() {
  // Don't try to compute the linear interpolation unless we have enough
  // samples.
  if (GetSampleCount() < 2) {
    return;
  }

  double average_bytes = 0;
  double average_time = 0;
  for (auto iter = samples_.Begin(); iter; ++iter) {
    const auto* sample = *iter;
    // TODO(lucmult): sample.bytes_count is already the total bytes, so we
    // probably shouldn't aggregate this. Keeping the aggregation to match with
    // the JS implementation.
    average_bytes += sample->bytes_count;
    average_time += (sample->time - start_time_).InMillisecondsF();
  }

  average_bytes = average_bytes / GetSampleCount();
  average_time = average_time / GetSampleCount();

  double variance_time = 0;
  double covariance_time_bytes = 0;
  for (auto iter = samples_.Begin(); iter; ++iter) {
    const auto* sample = *iter;
    const double time_diff =
        ((sample->time - start_time_).InMillisecondsF()) - average_time;
    variance_time += time_diff * time_diff;
    covariance_time_bytes += time_diff * (sample->bytes_count - average_bytes);
  }

  // Current speed in bytes per millisecond.
  double current_speed = covariance_time_bytes / variance_time;

  projected_end_time_ =
      (total_bytes_ - average_bytes) / current_speed + average_time;
}

void Speedometer::AppendSample(SpeedSample sample) {
  samples_.SaveToBuffer(std::move(sample));

  Interpolate();
}

}  // namespace io_task
}  // namespace file_manager
