// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_VERSION_UPDATER_UPDATE_TIME_ESTIMATOR_H_
#define CHROME_BROWSER_ASH_LOGIN_VERSION_UPDATER_UPDATE_TIME_ESTIMATOR_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"

namespace base {
class TickClock;
}

namespace ash {

// Helper class that gives time left expectations.
class UpdateTimeEstimator {
 public:
  struct UpdateStatus {
    base::TimeDelta time_left;
    int progress;
  };
  UpdateTimeEstimator();

  // Updates data needed for estimation.
  void Update(const update_engine::StatusResult& status);

  bool HasDownloadTime() const;

  bool HasTotalTime(update_engine::Operation stage) const;

  // Estimate time left for a downloading stage to complete.
  base::TimeDelta GetDownloadTimeLeft() const;

  // Estimate time left for an update to complete and current update progress in
  // procents.
  UpdateStatus GetUpdateStatus() const;

  void set_tick_clock_for_testing(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

 private:
  // Starts timer that refreshes time left estimation for non-error stages.
  void UpdateForTotalTimeLeftEstimation(update_engine::Operation stage);

  // Updates downloading stats (remaining time and downloading
  // progress), which are stored in update_info_.
  void UpdateForDownloadingTimeLeftEstimation(
      const update_engine::StatusResult& status);

  bool has_download_time_estimation_ = false;
  base::TimeDelta download_time_left_;

  // Time of the first notification from the downloading stage.
  base::TimeTicks download_start_time_;
  double download_start_progress_ = 0;

  // Time of the last notification from the downloading stage.
  base::TimeTicks download_last_time_;
  double download_last_progress_ = 0;

  bool is_download_average_speed_computed_ = false;
  // Average speed in bytes per second.
  double download_average_speed_ = 0;

  // Flag that is used to detect when update download has just started.
  bool is_downloading_update_ = false;

  // Time when stage for which total time estimation is calculated started.
  base::TimeTicks stage_started_time_;
  update_engine::Operation current_stage_ = update_engine::Operation::IDLE;

  raw_ptr<const base::TickClock> tick_clock_ = nullptr;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_VERSION_UPDATER_UPDATE_TIME_ESTIMATOR_H_
