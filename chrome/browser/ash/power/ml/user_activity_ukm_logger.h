// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_ML_USER_ACTIVITY_UKM_LOGGER_H_
#define CHROME_BROWSER_ASH_POWER_ML_USER_ACTIVITY_UKM_LOGGER_H_

#include "services/metrics/public/cpp/ukm_source_id.h"

namespace ash {
namespace power {
namespace ml {

class UserActivityEvent;

// Interface to log UserActivityEvent to UKM.
class UserActivityUkmLogger {
 public:
  virtual ~UserActivityUkmLogger() = default;

  // Log user activity event.
  virtual void LogActivity(const UserActivityEvent& event) = 0;
};

}  // namespace ml
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_ML_USER_ACTIVITY_UKM_LOGGER_H_
