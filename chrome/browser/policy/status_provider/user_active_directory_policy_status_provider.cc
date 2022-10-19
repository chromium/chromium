// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/user_active_directory_policy_status_provider.h"

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/active_directory/active_directory_policy_manager.h"
#include "chrome/browser/policy/status_provider/status_provider_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/browser/cloud/message_util.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "ui/base/l10n/time_format.h"

UserActiveDirectoryPolicyStatusProvider::
    UserActiveDirectoryPolicyStatusProvider(
        policy::ActiveDirectoryPolicyManager* policy_manager,
        Profile* profile)
    : policy_manager_(policy_manager) {
  policy_manager_->store()->AddObserver(this);
  profile_ = profile;
}

UserActiveDirectoryPolicyStatusProvider::
    ~UserActiveDirectoryPolicyStatusProvider() {
  policy_manager_->store()->RemoveObserver(this);
}

base::Value::Dict UserActiveDirectoryPolicyStatusProvider::GetStatus() {
  const enterprise_management::PolicyData* policy =
      policy_manager_->store()->policy();
  const std::string client_id = policy ? policy->device_id() : std::string();
  const std::string username = policy ? policy->username() : std::string();
  const std::u16string status =
      policy::FormatStoreStatus(policy_manager_->store()->status(),
                                policy_manager_->store()->validation_status());
  base::Value::Dict dict;
  dict.Set("status", status);
  dict.Set(policy::kUsernameKey, username);
  dict.Set(policy::kClientIdKey, client_id);

  if (!policy && policy_manager_->store()->is_managed())
    dict.Set("error", true);

  const base::TimeDelta refresh_interval =
      policy_manager_->scheduler()->interval();
  dict.Set(
      "refreshInterval",
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_SHORT, refresh_interval));

  const base::Time last_refresh_time =
      (policy && policy->has_timestamp())
          ? base::Time::FromJavaTime(policy->timestamp())
          : base::Time();
  dict.Set("timeSinceLastRefresh",
           GetTimeSinceLastActionString(last_refresh_time));

  const base::Time last_refresh_attempt_time =
      policy_manager_->scheduler()->last_refresh_attempt();
  dict.Set("timeSinceLastFetchAttempt",
           GetTimeSinceLastActionString(last_refresh_attempt_time));

  // Check if profile is present. Note that profile is not present if object is
  // an instance of DeviceActiveDirectoryPolicyStatusProvider that inherits from
  // UserActiveDirectoryPolicyStatusProvider.
  // TODO(b/182585903): Extend browser test to cover Active Directory case.
  if (profile_) {
    GetUserAffiliationStatus(&dict, profile_);
    GetUserManager(&dict, profile_);
  }
  dict.Set(policy::kPolicyDescriptionKey, kUserPolicyStatusDescription);
  SetDomainInUserStatus(dict);
  return dict;
}

void UserActiveDirectoryPolicyStatusProvider::OnStoreLoaded(
    policy::CloudPolicyStore* store) {
  NotifyStatusChange();
}

void UserActiveDirectoryPolicyStatusProvider::OnStoreError(
    policy::CloudPolicyStore* store) {
  NotifyStatusChange();
}
