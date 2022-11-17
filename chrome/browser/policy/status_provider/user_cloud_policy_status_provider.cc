// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/user_cloud_policy_status_provider.h"

#include "base/values.h"
#include "chrome/browser/policy/status_provider/status_provider_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

UserCloudPolicyStatusProvider::UserCloudPolicyStatusProvider(
    policy::CloudPolicyCore* core,
    Profile* profile)
    : CloudPolicyCoreStatusProvider(core), profile_(profile) {}

UserCloudPolicyStatusProvider::~UserCloudPolicyStatusProvider() = default;

base::Value::Dict UserCloudPolicyStatusProvider::GetStatus() {
  if (!core_->store()->is_managed())
    return {};
  base::Value::Dict dict =
      policy::PolicyStatusProvider::GetStatusFromCore(core_);
  ExtractDomainFromUsername(&dict);
  GetUserAffiliationStatus(&dict, profile_);
  dict.Set(policy::kPolicyDescriptionKey, kUserPolicyStatusDescription);
  SetDomainInUserStatus(dict);
  SetProfileId(&dict, profile_);
  return dict;
}
