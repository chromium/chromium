// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limits/app_time_limits_allowlist_policy_wrapper.h"

#include <optional>

#include "base/logging.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_policy_helpers.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_types.h"

namespace ash {
namespace app_time {

AppTimeLimitsAllowlistPolicyWrapper::AppTimeLimitsAllowlistPolicyWrapper(
    const base::Value::Dict* dict)
    : dict_(dict) {}

AppTimeLimitsAllowlistPolicyWrapper::~AppTimeLimitsAllowlistPolicyWrapper() =
    default;

std::vector<AppId> AppTimeLimitsAllowlistPolicyWrapper::GetAllowlistAppList()
    const {
  std::vector<AppId> return_value;

  const base::Value::List* app_list = dict_->FindList(policy::kAppList);
  if (!app_list) {
    VLOG(1) << "Invalid allowlist application list.";
    return return_value;
  }

  for (const base::Value& value : *app_list) {
    std::optional<AppId> app_id = policy::AppIdFromDict(value.GetIfDict());
    if (app_id)
      return_value.push_back(*app_id);
  }

  return return_value;
}

}  // namespace app_time
}  // namespace ash
