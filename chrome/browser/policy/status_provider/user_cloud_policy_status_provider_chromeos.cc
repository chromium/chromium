// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/user_cloud_policy_status_provider_chromeos.h"

#include "base/values.h"
#include "chrome/browser/policy/status_provider/status_provider_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

UserCloudPolicyStatusProviderChromeOS::UserCloudPolicyStatusProviderChromeOS(
    policy::CloudPolicyCore* core,
    Profile* profile)
    : UserCloudPolicyStatusProvider(core, profile) {
  profile_ = profile;
}

UserCloudPolicyStatusProviderChromeOS::
    ~UserCloudPolicyStatusProviderChromeOS() = default;

base::Value::Dict UserCloudPolicyStatusProviderChromeOS::GetStatus() {
  if (!core_->store()->is_managed())
    return {};
  base::Value::Dict dict = UserCloudPolicyStatusProvider::GetStatus();
  GetUserAffiliationStatus(&dict, profile_);
  GetUserManager(&dict, profile_);
  dict.Set(policy::kPolicyDescriptionKey, kUserPolicyStatusDescription);
  SetDomainExtractedFromUsername(dict);
  SetProfileId(&dict, profile_);
  return dict;
}
