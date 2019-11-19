// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/fps_counter.h"

#include "ui/compositor/compositor.h"

namespace ash {
namespace {
bool force_report_zero_animation_for_test = false;
}  // namespace

FpsCounter::FpsCounter(ui::Compositor* compositor) : compositor_(compositor) {
  compositor_->AddObserver(this);
  start_frame_number_ = compositor->activated_frame_count();
  start_time_ = base::TimeTicks::Now();
}

FpsCounter::~FpsCounter() {
  if (compositor_)
    compositor_->RemoveObserver(this);
}

int FpsCounter::ComputeSmoothness() {
  if (!compositor_)
    return -1;
  int end_frame_number = compositor_->activated_frame_count();

  // Don't report zero animation as 100.
  if (!force_report_zero_animation_for_test &&
      end_frame_number <= start_frame_number_)
    return -1;

  base::TimeDelta elapsed = base::TimeTicks::Now() - start_time_;
  float refresh_rate = compositor_->refresh_rate();
  int expected_frame_number =
      std::floor(refresh_rate * elapsed.InMillisecondsF() /
                 base::Time::kMillisecondsPerSecond);
  int actual_frame_number = end_frame_number - start_frame_number_;
  int smoothness = actual_frame_number < expected_frame_number
                       ? smoothness =
                             100 * actual_frame_number / expected_frame_number
                       : 100;

  VLOG(1) << "Smoothness:" << smoothness
          << ", duration=" << elapsed.InMilliseconds()
          << ", actual_frame=" << actual_frame_number
          << ", expected_frame=" << expected_frame_number;
  return smoothness;
}

void FpsCounter::OnCompositingShuttingDown(ui::Compositor* compositor) {
  DCHECK_EQ(compositor_, compositor);
  compositor->RemoveObserver(this);
  compositor_ = nullptr;
}

// static
void FpsCounter::SetForceReportZeroAnimationForTest(bool value) {
  force_report_zero_animation_for_test = value;
}

}  // namespace ash
