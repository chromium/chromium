// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limits_policy_builder.h"

#include "chrome/browser/ash/child_accounts/time_limits/app_time_policy_helpers.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_types.h"

namespace ash {
namespace app_time {

AppTimeLimitsPolicyBuilder::AppTimeLimitsPolicyBuilder() {
  value_.Set(policy::kAppLimitsArray, base::Value::List());
  value_.Set(policy::kResetAtDict, base::Value::Dict());
  value_.Set(policy::kActivityReportingEnabled, true);
}

AppTimeLimitsPolicyBuilder::~AppTimeLimitsPolicyBuilder() = default;

void AppTimeLimitsPolicyBuilder::AddAppLimit(const AppId& app_id,
                                             const AppLimit& app_limit) {
  base::Value::Dict new_entry;
  new_entry.Set(policy::kAppInfoDict, policy::AppIdToDict(app_id));
  base::Value::Dict app_limit_value = policy::AppLimitToDict(app_limit);
  new_entry.Merge(std::move(app_limit_value));

  base::Value::List* list = value_.FindList(policy::kAppLimitsArray);
  DCHECK(list);
  list->Append(std::move(new_entry));
}

void AppTimeLimitsPolicyBuilder::SetResetTime(int hour, int minutes) {
  value_.Set(policy::kResetAtDict, policy::ResetTimeToDict(hour, minutes));
}

void AppTimeLimitsPolicyBuilder::SetAppActivityReportingEnabled(bool enabled) {
  value_.Set(policy::kActivityReportingEnabled, enabled);
}

}  // namespace app_time
}  // namespace ash
