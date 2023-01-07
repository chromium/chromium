// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/version_updater/update_time_estimator.h"

#include <utility>

#include "base/time/default_tick_clock.h"
#include "base/time/time.h"

namespace ash {

namespace {

// Estimation time needed for each stage to complete in seconds.
constexpr base::TimeDelta kDownloadTime = base::Minutes(50);
constexpr base::TimeDelta kVerifyingTime = base::Minutes(5);
constexpr base::TimeDelta kFinalizingTime = base::Minutes(5);

// Progress in percent falls on a stage. Should be 100 in total.
const int kDownloadProgress = 90;
const int kVerifyingProgress = 5;
const int kFinalizingProgress = 5;

const int kTotalProgress = 100;

struct StageTimeExpectationProgress {
  update_engine::Operation stage_;
  base::TimeDelta time_estimation_;
  int progress_;
};

// Stages for which `estimated_time_left` and progress should be calculated.
constexpr StageTimeExpectationProgress kStages[] = {
    {update_engine::Operation::DOWNLOADING, kDownloadTime, kDownloadProgress},
    {update_engine::Operation::VERIFYING, kVerifyingTime, kVerifyingProgress},
    {update_engine::Operation::FINALIZING, kFinalizingTime,
     kFinalizingProgress},
};

// Minimum timestep between two consecutive measurements for the download rates.
constexpr const base::TimeDelta kMinTimeStep = base::Seconds(1);

// Smooth factor that is used for the average downloading speed
// estimation.
// avg_speed = smooth_factor * cur_speed + (1.0 - smooth_factor) *
// avg_speed.
const double kDownloadSpeedSmoothFactor = 0.1;

// Minimum allowed value for the average downloading speed.
const double kDownloadAverageSpeedDropBound = 1e-8;

// An upper bound for possible downloading time left estimations.
constexpr const base::TimeDelta kMaxTimeLeft = base::Days(1);

}  // anonymous namespace

UpdateTimeEstimator::UpdateTimeEstimator()
    : tick_clock_(base::DefaultTickClock::GetInstance()) {}

void UpdateTimeEstimator::Update(const update_engine::StatusResult& status) {
  if (status.current_operation() == update_engine::Operation::DOWNLOADING) {
    UpdateForDownloadingTimeLeftEstimation(status);
  }
  UpdateForTotalTimeLeftEstimation(status.current_operation());
}

bool UpdateTimeEstimator::HasDownloadTime() const {
  return has_download_time_estimation_;
}

bool UpdateTimeEstimator::HasTotalTime(update_engine::Operation stage) const {
  for (const auto& el : kStages) {
    if (el.stage_ == stage) {
      return true;
    }
  }
  return false;
}

base::TimeDelta UpdateTimeEstimator::GetDownloadTimeLeft() const {
  return download_time_left_;
}

UpdateTimeEstimator::UpdateStatus UpdateTimeEstimator::GetUpdateStatus() const {
  base::TimeDelta time_left_estimation;
  int progress_left = 0;
  bool stage_found = false;
  for (const auto& el : kStages) {
    if (stage_found) {
      time_left_estimation += el.time_estimation_;
      progress_left += el.progress_;
    }
    if (el.stage_ != current_stage_)
      continue;

    stage_found = true;
    // Time spent and left in the current stage.
    const base::TimeDelta time_spent =
        tick_clock_->NowTicks() - stage_started_time_;
    const base::TimeDelta time_left =
        std::max(el.time_estimation_ - time_spent, base::TimeDelta());
    // Time left calculation.
    if (current_stage_ == update_engine::Operation::DOWNLOADING &&
        has_download_time_estimation_) {
      const base::TimeDelta time_left_speed_estimation =
          download_time_left_ - (tick_clock_->NowTicks() - download_last_time_);
      time_left_estimation += time_left_speed_estimation;
    } else {
      time_left_estimation += time_left;
    }
    // Progress left calculation.
    if (current_stage_ == update_engine::Operation::DOWNLOADING) {
      const double download_progress_left = 1.0 - download_last_progress_;
      const int current_stage_progress_left =
          base::ClampRound(download_progress_left * el.progress_);
      progress_left += current_stage_progress_left;
    } else {
      progress_left +=
          base::ClampRound((time_left / el.time_estimation_) * el.progress_);
    }
  }
  return {time_left_estimation, kTotalProgress - progress_left};
}

void UpdateTimeEstimator::UpdateForTotalTimeLeftEstimation(
    update_engine::Operation stage) {
  if (current_stage_ != stage) {
    current_stage_ = stage;
    stage_started_time_ = tick_clock_->NowTicks();
  }
}

void UpdateTimeEstimator::UpdateForDownloadingTimeLeftEstimation(
    const update_engine::StatusResult& status) {
  if (!is_downloading_update_) {
    is_downloading_update_ = true;

    download_start_time_ = download_last_time_ = tick_clock_->NowTicks();
    download_start_progress_ = status.progress();
    download_last_progress_ = status.progress();
    is_download_average_speed_computed_ = false;
    download_average_speed_ = 0.0;
  }

  base::TimeTicks download_current_time = tick_clock_->NowTicks();
  if (download_current_time < download_last_time_ + kMinTimeStep)
    return;

  // Estimate downloading rate.
  double progress_delta =
      std::max(status.progress() - download_last_progress_, 0.0);
  double time_delta =
      (download_current_time - download_last_time_).InSecondsF();
  double download_rate = status.new_size() * progress_delta / time_delta;

  download_last_time_ = download_current_time;
  download_last_progress_ = status.progress();

  // Estimate time left.
  double progress_left = std::max(1.0 - status.progress(), 0.0);
  if (!is_download_average_speed_computed_) {
    download_average_speed_ = download_rate;
    is_download_average_speed_computed_ = true;
  }
  download_average_speed_ =
      kDownloadSpeedSmoothFactor * download_rate +
      (1.0 - kDownloadSpeedSmoothFactor) * download_average_speed_;
  if (download_average_speed_ < kDownloadAverageSpeedDropBound) {
    time_delta = (download_current_time - download_start_time_).InSecondsF();
    download_average_speed_ = status.new_size() * progress_delta / time_delta;
  }
  double work_left = progress_left * status.new_size();
  // time_left is in seconds.
  double time_left = work_left / download_average_speed_;
  // If `download_average_speed_` is 0.
  if (isnan(time_left)) {
    has_download_time_estimation_ = false;
    return;
  }
  // `time_left` may be large enough or even +infinity. So we must
  // |bound possible estimations.
  time_left = std::min(time_left, kMaxTimeLeft.InSecondsF());

  has_download_time_estimation_ = true;
  download_time_left_ = base::Seconds(static_cast<int>(round(time_left)));
}

}  // namespace ash
