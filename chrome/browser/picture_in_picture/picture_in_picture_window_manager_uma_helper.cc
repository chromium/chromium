// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager_uma_helper.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"

namespace {

using HistogramBase = base::HistogramBase;

}  // anonymous namespace

PictureInPictureWindowManagerUmaHelper::PictureInPictureWindowManagerUmaHelper()
    : clock_(base::DefaultTickClock::GetInstance()) {}

void PictureInPictureWindowManagerUmaHelper::MaybeRecordPictureInPictureChanged(
    bool is_picture_in_picture) {
  if (is_picture_in_picture) {
    current_enter_pip_time_ = clock_->NowTicks();
    return;
  }

  if (!current_enter_pip_time_) {
    return;
  }

  const base::TimeDelta total_pip_time =
      clock_->NowTicks() - current_enter_pip_time_.value();
  current_enter_pip_time_ = std::nullopt;

  UMA_HISTOGRAM_CUSTOM_TIMES("Media.PictureInPicture.Window.TotalTime",
                             total_pip_time, base::Milliseconds(1),
                             base::Minutes(2), 50);
  UMA_HISTOGRAM_CUSTOM_TIMES("Media.PictureInPicture.Window.TotalTimeV2",
                             total_pip_time, base::Milliseconds(1),
                             base::Hours(10), 100);
}

void PictureInPictureWindowManagerUmaHelper::SetClockForTest(
    const base::TickClock* testing_clock) {
  clock_ = testing_clock;
}
