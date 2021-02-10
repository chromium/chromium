// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/affiliation.h"

#include <set>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"

namespace chrome {
namespace enterprise_util {

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !defined(OS_ANDROID)

namespace {

const enterprise_management::PolicyData* GetPolicyData(
    policy::CloudPolicyManager* policy_manager) {
  if (!policy_manager || !policy_manager->IsClientRegistered() ||
      !policy_manager->core() || !policy_manager->core()->store()) {
    return nullptr;
  }

  return policy_manager->core()->store()->policy();
}

}  // namespace

const enterprise_management::PolicyData* GetProfilePolicyData(
    Profile* profile) {
  DCHECK(profile);
  return GetPolicyData(profile->GetUserCloudPolicyManager());
}

const enterprise_management::PolicyData* GetBrowserPolicyData() {
  if (!g_browser_process->browser_policy_connector())
    return nullptr;

  policy::MachineLevelUserCloudPolicyManager* policy_manager =
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager();

  return GetPolicyData(policy_manager);
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !defined(OS_ANDROID)

bool IsProfileAffiliated(
    const enterprise_management::PolicyData& profile_policy,
    const enterprise_management::PolicyData& browser_policy) {
  std::set<std::string> profile_affiliation_ids;
  profile_affiliation_ids.insert(profile_policy.user_affiliation_ids().begin(),
                                 profile_policy.user_affiliation_ids().end());
  for (const std::string& browser_affiliation_id :
       browser_policy.device_affiliation_ids()) {
    if (profile_affiliation_ids.count(browser_affiliation_id))
      return true;
  }
  return false;
}

}  // namespace enterprise_util
}  // namespace chrome
