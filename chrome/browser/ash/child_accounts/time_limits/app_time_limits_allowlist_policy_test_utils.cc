// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limits_allowlist_policy_test_utils.h"

#include "chrome/browser/ash/child_accounts/time_limits/app_time_limits_allowlist_policy_wrapper.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_policy_helpers.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_types.h"

namespace ash {
namespace app_time {

AppTimeLimitsAllowlistPolicyBuilder::AppTimeLimitsAllowlistPolicyBuilder() =
    default;

AppTimeLimitsAllowlistPolicyBuilder::~AppTimeLimitsAllowlistPolicyBuilder() =
    default;

void AppTimeLimitsAllowlistPolicyBuilder::SetUp() {
  dict_ = base::Value::Dict();
  dict_.Set(policy::kUrlList, base::Value::List());
  dict_.Set(policy::kAppList, base::Value::List());
}

void AppTimeLimitsAllowlistPolicyBuilder::AppendToAllowlistAppList(
    const AppId& app_id) {
  base::Value::Dict dict_to_append;
  dict_to_append.Set(policy::kAppId, base::Value(app_id.app_id()));
  dict_to_append.Set(
      policy::kAppType,
      base::Value(policy::AppTypeToPolicyString(app_id.app_type())));
  AppendToList(policy::kAppList, std::move(dict_to_append));
}

void AppTimeLimitsAllowlistPolicyBuilder::AppendToList(const std::string& key,
                                                       base::Value::Dict dict) {
  base::Value::List* list = dict_.FindList(key);
  DCHECK(list);
  list->Append(std::move(dict));
}

}  // namespace app_time
}  // namespace ash
