// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/tracing/arc_graphics_jank_detector.h"

namespace arc {

// static
constexpr base::TimeDelta ArcGraphicsJankDetector::kPauseDetectionThreshold;

ArcGraphicsJankDetector::ArcGraphicsJankDetector(const JankCallback& callback)
    : callback_(callback) {
  Reset();
}

ArcGraphicsJankDetector::~ArcGraphicsJankDetector() = default;

void ArcGraphicsJankDetector::Reset() {
  stage_ = Stage::kWarmUp;
  last_sample_time_ = base::Time::Now();
  warm_up_sample_cnt_ = kWarmUpSamples;
  period_fixed_ = false;
}

void ArcGraphicsJankDetector::SetPeriodFixed(const base::TimeDelta& period) {
  period_ = period;
  period_fixed_ = true;
  stage_ = Stage::kActive;
}

void ArcGraphicsJankDetector::OnSample() {
  OnSample(base::Time::Now());
}

void ArcGraphicsJankDetector::OnSample(const base::Time& timestamp) {
  const base::TimeDelta delta = timestamp - last_sample_time_;
  last_sample_time_ = timestamp;

  // Try to detect pause and switch to warm-up stage.
  if (delta >= kPauseDetectionThreshold) {
    if (period_fixed_)
      return;
    warm_up_sample_cnt_ = kWarmUpSamples;
    stage_ = Stage::kWarmUp;
    return;
  }

  if (stage_ == Stage::kWarmUp) {
    DCHECK(warm_up_sample_cnt_);
    if (--warm_up_sample_cnt_)
      return;
    // Switch to rate detection.
    intervals_.clear();
    stage_ = Stage::kRateDetection;
    return;
  }

  if (stage_ == Stage::kRateDetection) {
    intervals_.emplace_back(std::move(delta));
    if (intervals_.size() < kSamplesForRateDetection)
      return;
    std::sort(intervals_.begin(), intervals_.end());
    period_ = intervals_[intervals_.size() / 3];
    stage_ = Stage::kActive;
    return;
  }

  DCHECK_EQ(Stage::kActive, stage_);
  if (delta >= period_ * kJankDetectionThresholdPercent / 100)
    callback_.Run(timestamp - delta + period_);
}

}  // namespace arc
