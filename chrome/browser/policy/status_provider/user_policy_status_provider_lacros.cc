// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/user_policy_status_provider_lacros.h"

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/status_provider/status_provider_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/policy/core/browser/cloud/message_util.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/policy_loader_lacros.h"
#include "components/policy/proto/device_management_backend.pb.h"

UserPolicyStatusProviderLacros::UserPolicyStatusProviderLacros(
    policy::PolicyLoaderLacros* loader,
    Profile* profile)
    : profile_(profile), loader_(loader) {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service) {
    lacros_service->AddObserver(this);
  }
}

UserPolicyStatusProviderLacros::~UserPolicyStatusProviderLacros() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service) {
    lacros_service->RemoveObserver(this);
  }
}

base::Value::Dict UserPolicyStatusProviderLacros::GetStatus() {
  enterprise_management::PolicyData* policy = loader_->GetPolicyData();
  if (!policy) {
    base::Value::Dict error_dict;
    if (profile_->GetProfilePolicyConnector()->IsManaged()) {
      error_dict.Set("error", true);
    }
    return error_dict;
  }
  base::Value::Dict dict = GetStatusFromPolicyData(policy);
  SetDomainExtractedFromUsername(dict);
  GetUserAffiliationStatus(&dict, profile_);

  // Get last fetched time from policy, since we have no refresh scheduler here.
  base::Time last_refresh_time =
      policy && policy->has_timestamp()
          ? base::Time::FromMillisecondsSinceUnixEpoch(policy->timestamp())
          : base::Time();
  dict.Set("timeSinceLastRefresh",
           GetTimeSinceLastActionString(last_refresh_time));

  const base::Time last_refresh_attempt_time = loader_->last_fetch_timestamp();
  dict.Set("timeSinceLastFetchAttempt",
           GetTimeSinceLastActionString(last_refresh_attempt_time));

  // TODO(crbug.com/40787523): Pass this information from Ash through
  // Mojo. Assume no error for now.
  dict.Set("error", false);
  dict.Set("status",
           FormatStoreStatus(
               policy::CloudPolicyStore::STATUS_OK,
               policy::CloudPolicyValidatorBase::Status::VALIDATION_OK));
  dict.Set(policy::kPolicyDescriptionKey, kUserPolicyStatusDescription);
  SetProfileId(&dict, profile_);
  return dict;
}

void UserPolicyStatusProviderLacros::OnPolicyUpdated(
    const std::vector<uint8_t>& policy_fetch_response) {
  NotifyStatusChange();
}

void UserPolicyStatusProviderLacros::OnPolicyFetchAttempt() {
  NotifyStatusChange();
}

void UserPolicyStatusProviderLacros::OnComponentPolicyUpdated(
    const policy::ComponentPolicyMap& component_policy) {
  NotifyStatusChange();
}
