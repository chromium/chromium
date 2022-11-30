// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMITS_ALLOWLIST_POLICY_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMITS_ALLOWLIST_POLICY_TEST_UTILS_H_

#include <string>

#include "base/values.h"

namespace ash {
namespace app_time {

class AppId;

class AppTimeLimitsAllowlistPolicyBuilder {
 public:
  AppTimeLimitsAllowlistPolicyBuilder();
  ~AppTimeLimitsAllowlistPolicyBuilder();

  AppTimeLimitsAllowlistPolicyBuilder(
      const AppTimeLimitsAllowlistPolicyBuilder&) = delete;
  AppTimeLimitsAllowlistPolicyBuilder& operator=(
      const AppTimeLimitsAllowlistPolicyBuilder&) = delete;

  void SetUp();
  void AppendToAllowlistAppList(const AppId& app_id);

  const base::Value::Dict& dict() const { return dict_; }

 private:
  void AppendToList(const std::string& key, base::Value::Dict dict);

  base::Value::Dict dict_;
};

}  // namespace app_time
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMITS_ALLOWLIST_POLICY_TEST_UTILS_H_
