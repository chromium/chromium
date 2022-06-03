// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMITS_ALLOWLIST_POLICY_WRAPPER_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMITS_ALLOWLIST_POLICY_WRAPPER_H_

#include <string>
#include <vector>

#include "base/values.h"

namespace ash {
namespace app_time {

class AppId;

class AppTimeLimitsAllowlistPolicyWrapper {
 public:
  explicit AppTimeLimitsAllowlistPolicyWrapper(const base::Value* value);
  ~AppTimeLimitsAllowlistPolicyWrapper();

  // Delete copy constructor and copy assign operator.
  AppTimeLimitsAllowlistPolicyWrapper(
      const AppTimeLimitsAllowlistPolicyWrapper&) = delete;
  AppTimeLimitsAllowlistPolicyWrapper& operator=(
      const AppTimeLimitsAllowlistPolicyWrapper&) = delete;

  std::vector<std::string> GetAllowlistURLList() const;
  std::vector<AppId> GetAllowlistAppList() const;

 private:
  const base::Value* value_;
};

}  // namespace app_time
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMITS_ALLOWLIST_POLICY_WRAPPER_H_
