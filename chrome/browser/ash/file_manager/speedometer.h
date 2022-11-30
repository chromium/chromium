// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_SPEEDOMETER_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_SPEEDOMETER_H_

#include "base/containers/ring_buffer.h"
#include "base/time/time.h"

namespace {
struct SpeedSample {
  // Total bytes processed up to this point in time.
  int64_t bytes_count;

  // Time when the sample was created.
  base::TimeTicks time;
};
}  // namespace

namespace file_manager {
namespace io_task {

// Calculates the remaining time for an operation based on the initial total
// bytes and the amount of bytes transferred on each `sample`.
//
// It estimates when the total bytes will be reached and exposes the "remaining
// time" from now until the projected end time.
class Speedometer {
 public:
  Speedometer();

  Speedometer(const Speedometer& other) = delete;
  Speedometer operator=(const Speedometer& other) = delete;
  ~Speedometer() = default;

  // Set the total bytes for the operation.
  void SetTotalBytes(int64_t total_bytes);

  // Number of samples currently maintained.
  size_t GetSampleCount() const;

  // Projected remaining time, it can be negative or inifity.
  double GetRemainingSeconds() const;

  // Adds a sample with the current timestamp and the given number of |bytes|
  // Does nothing if the previous sample was received less than a second ago.
  // `total_processed_bytes`: Total bytes processed by the task so far.
  void Update(int64_t total_processed_bytes);

 private:
  // Computes a linear interpolation of the samples stored in |samples_|.
  // It doesn't calculate if there isn't enough samples.
  // The calculated speed is the slope of the linear interpolation in
  // bytes per millisecond.
  // The linear interpolation goes through the point
  // (average_time, average_bytes).
  void Interpolate();

  // Stores the `sample` and enforces the `max_sample_` constraint.
  void AppendSample(SpeedSample sample);

  // Maintains the 20 most recent samples.
  base::RingBuffer<SpeedSample, 20> samples_;

  // The total number of bytes, this is the 100% value for an operation/task.
  int64_t total_bytes_;

  // The projected time to finish the operation, in milliseconds from the
  // `start_time_`.
  double projected_end_time_ = 0;

  // Time the Speedometer started. Used to calculate the delta from here to each
  // sample time.
  const base::TimeTicks start_time_;
};

}  // namespace io_task
}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_SPEEDOMETER_H_
