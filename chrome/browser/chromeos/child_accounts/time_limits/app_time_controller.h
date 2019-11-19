// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_CONTROLLER_H_

#include <memory>

namespace chromeos {

class WebTimeLimitEnforcer;

// Coordinates per-app time limit for child user.
class AppTimeController {
 public:
  static bool ArePerAppTimeLimitsEnabled();

  AppTimeController();
  AppTimeController(const AppTimeController&) = delete;
  AppTimeController& operator=(const AppTimeController&) = delete;
  ~AppTimeController();

  const WebTimeLimitEnforcer* web_time_enforcer() const {
    return web_time_enforcer_.get();
  }

  WebTimeLimitEnforcer* web_time_enforcer() { return web_time_enforcer_.get(); }

 private:
  std::unique_ptr<WebTimeLimitEnforcer> web_time_enforcer_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_CONTROLLER_H_
