// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limits/app_time_limits_allowlist_policy_wrapper.h"

#include "base/logging.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_policy_helpers.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace app_time {

AppTimeLimitsAllowlistPolicyWrapper::AppTimeLimitsAllowlistPolicyWrapper(
    const base::Value* value)
    : value_(value) {}

AppTimeLimitsAllowlistPolicyWrapper::~AppTimeLimitsAllowlistPolicyWrapper() =
    default;

std::vector<std::string>
AppTimeLimitsAllowlistPolicyWrapper::GetAllowlistURLList() const {
  std::vector<std::string> return_value;

  const base::Value* list = value_->FindListKey(policy::kUrlList);
  if (!list) {
    VLOG(1) << "Invalid allowlist URL list provided.";
    return return_value;
  }

  base::Value::ConstListView list_view = list->GetListDeprecated();
  for (const base::Value& value : list_view) {
    if (!value.is_string()) {
      VLOG(1) << "Allowlist URL is not a string.";
      continue;
    }
    return_value.push_back(value.GetString());
  }
  return return_value;
}

std::vector<AppId> AppTimeLimitsAllowlistPolicyWrapper::GetAllowlistAppList()
    const {
  std::vector<AppId> return_value;

  const base::Value* app_list = value_->FindListKey(policy::kAppList);
  if (!app_list) {
    VLOG(1) << "Invalid allowlist application list.";
    return return_value;
  }

  base::Value::ConstListView list_view = app_list->GetListDeprecated();
  for (const base::Value& value : list_view) {
    absl::optional<AppId> app_id = policy::AppIdFromDict(value);
    if (app_id)
      return_value.push_back(*app_id);
  }

  return return_value;
}

}  // namespace app_time
}  // namespace ash
