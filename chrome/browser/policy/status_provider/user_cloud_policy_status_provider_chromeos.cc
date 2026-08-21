// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/user_cloud_policy_status_provider_chromeos.h"

#include "base/values.h"
#include "chrome/browser/enterprise/browser_management/management_identity.h"
#include "chrome/browser/policy/status_provider/status_provider_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/resources/webui/mojom/policy.mojom.h"

UserCloudPolicyStatusProviderChromeOS::UserCloudPolicyStatusProviderChromeOS(
    policy::CloudPolicyManager* cloud_policy_manager,
    Profile* profile)
    // TODO(b/486888143): ChromeOS only supports user policies, so there is no
    // extension install core.
    : UserCloudPolicyStatusProvider(cloud_policy_manager, profile) {
  profile_ = profile;
}

UserCloudPolicyStatusProviderChromeOS::
    ~UserCloudPolicyStatusProviderChromeOS() = default;

base::DictValue UserCloudPolicyStatusProviderChromeOS::GetStatus() {
  if (!core()->store()->is_managed()) {
    return {};
  }
  base::DictValue dict = UserCloudPolicyStatusProvider::GetStatus();
  if (auto user_affiliation_status = GetUserAffiliationStatus(profile_)) {
    dict.Set("isAffiliated", user_affiliation_status.value());
  }
  GetUserManager(&dict, profile_);
  dict.Set(policy::kPolicyDescriptionKey, kUserPolicyStatusDescription);
  SetDomainExtractedFromUsername(dict);
  if (auto profile_id = GetProfileId(profile_)) {
    dict.Set("profileId", profile_id.value());
  }
  return dict;
}

policy::mojom::StatusPtr
UserCloudPolicyStatusProviderChromeOS::GetStatusMojo() {
  if (!core()->store()->is_managed()) {
    return policy::mojom::Status::New();
  }
  auto status = UserCloudPolicyStatusProvider::GetStatusMojo();
  status->enterprise_domain_manager = GetAccountManagerIdentity(profile_);
  return status;
}
