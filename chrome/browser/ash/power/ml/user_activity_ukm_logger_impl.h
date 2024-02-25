// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_ML_USER_ACTIVITY_UKM_LOGGER_IMPL_H_
#define CHROME_BROWSER_ASH_POWER_ML_USER_ACTIVITY_UKM_LOGGER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/power/ml/user_activity_ukm_logger.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace ash {
namespace power {
namespace ml {

class UserActivityEvent;

class UserActivityUkmLoggerImpl : public UserActivityUkmLogger {
 public:
  UserActivityUkmLoggerImpl();

  UserActivityUkmLoggerImpl(const UserActivityUkmLoggerImpl&) = delete;
  UserActivityUkmLoggerImpl& operator=(const UserActivityUkmLoggerImpl&) =
      delete;

  ~UserActivityUkmLoggerImpl() override;

  // ash::power::ml::UserActivityUkmLogger overrides:
  void LogActivity(const UserActivityEvent& event) override;

 private:
  friend class UserActivityUkmLoggerTest;

  raw_ptr<ukm::UkmRecorder> ukm_recorder_;  // not owned

  // This ID is incremented each time a UserActivity is logged to UKM.
  // Event index starts from 1, and resets when a new session starts.
  int next_sequence_id_ = 1;
};

}  // namespace ml
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_ML_USER_ACTIVITY_UKM_LOGGER_IMPL_H_
