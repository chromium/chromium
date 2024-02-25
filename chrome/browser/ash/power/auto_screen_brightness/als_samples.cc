// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/auto_screen_brightness/als_samples.h"

#include <numeric>

#include "base/check.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

AmbientLightSampleBuffer::AmbientLightSampleBuffer(base::TimeDelta horizon)
    : horizon_(horizon) {
  DCHECK(!horizon_.is_zero());
}

AmbientLightSampleBuffer::~AmbientLightSampleBuffer() = default;

void AmbientLightSampleBuffer::SaveToBuffer(
    const AmbientLightSampleBuffer::Sample& sample) {
  samples_.push_back(sample);
  Prune(sample.sample_time);
}

void AmbientLightSampleBuffer::ClearBuffer() {
  samples_.clear();
}

std::optional<AlsAvgStdDev> AmbientLightSampleBuffer::AverageAmbientWithStdDev(
    base::TimeTicks now) {
  Prune(now);
  if (samples_.empty())
    return std::nullopt;

  const size_t count = samples_.size();
  double avg = 0;
  double stddev = 0;
  for (const auto& sample : samples_) {
    avg += sample.lux;
    stddev += sample.lux * sample.lux;
  }

  avg = avg / count;
  return std::optional<AlsAvgStdDev>(
      {avg, std::sqrt(stddev / count - avg * avg)});
}

size_t AmbientLightSampleBuffer::NumberOfSamples(base::TimeTicks now) {
  Prune(now);
  return samples_.size();
}

size_t AmbientLightSampleBuffer::NumberOfSamplesForTesting() const {
  return samples_.size();
}

void AmbientLightSampleBuffer::Prune(base::TimeTicks now) {
  while (!samples_.empty()) {
    if (now - samples_.front().sample_time < horizon_)
      return;

    samples_.pop_front();
  }
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash
