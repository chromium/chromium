// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limits/app_time_limits_allowlist_policy_test_utils.h"

#include "chrome/browser/ash/child_accounts/time_limits/app_time_limits_allowlist_policy_wrapper.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_policy_helpers.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_types.h"

namespace ash::app_time {

AppTimeLimitsAllowlistPolicyBuilder::AppTimeLimitsAllowlistPolicyBuilder() =
    default;

AppTimeLimitsAllowlistPolicyBuilder::~AppTimeLimitsAllowlistPolicyBuilder() =
    default;

void AppTimeLimitsAllowlistPolicyBuilder::SetUp() {
  dict_ = base::DictValue();
  dict_.Set(policy::kUrlList, base::ListValue());
  dict_.Set(policy::kAppList, base::ListValue());
}

void AppTimeLimitsAllowlistPolicyBuilder::Clear() {
  dict_.clear();
}

void AppTimeLimitsAllowlistPolicyBuilder::AppendToAllowlistAppList(
    const AppId& app_id) {
  base::DictValue dict_to_append;
  dict_to_append.Set(policy::kAppId, base::Value(app_id.app_id()));
  dict_to_append.Set(
      policy::kAppType,
      base::Value(policy::AppTypeToPolicyString(app_id.app_type())));
  AppendToList(policy::kAppList, std::move(dict_to_append));
}

void AppTimeLimitsAllowlistPolicyBuilder::AppendToList(const std::string& key,
                                                       base::DictValue dict) {
  base::ListValue* list = dict_.FindList(key);
  DCHECK(list);
  list->Append(std::move(dict));
}

}  // namespace ash::app_time
