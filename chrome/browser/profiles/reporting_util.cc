// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/reporting_util.h"

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

#ifdef OS_CHROMEOS
#include "chrome/browser/chromeos/login/users/affiliation.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#endif  // OS_CHROMEOS

namespace {

#ifdef OS_CHROMEOS
// A callback which fetches device dm_token based on user affiliation.
using DeviceDMTokenCallback = base::RepeatingCallback<std::string(
    const std::vector<std::string>& user_affiliation_ids)>;

// Returns policy for the given |profile|. If failed to get policy returns
// nullptr.
const enterprise_management::PolicyData* GetPolicyData(Profile* profile) {
  if (!profile)
    return nullptr;

  policy::UserCloudPolicyManagerChromeOS* manager =
      profile->GetUserCloudPolicyManagerChromeOS();
  if (!manager)
    return nullptr;

  policy::CloudPolicyStore* store = manager->core()->store();
  if (!store || !store->has_policy())
    return nullptr;

  return store->policy();
}

// Returns the Device DMToken for the given |profile| if:
// * |profile| is NOT incognito profile
// * user corresponding to a given |profile| is affiliated.
// Otherwise returns empty string. More about DMToken:
// go/dmserver-domain-model#dmtoken.
std::string GetDeviceDmToken(Profile* profile) {
  if (!profile)
    return std::string();

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return std::string();

  DeviceDMTokenCallback device_dm_token_callback =
      chromeos::GetDeviceDMTokenForUserPolicyGetter(user->GetAccountId());
  if (!device_dm_token_callback)
    return std::string();

  const enterprise_management::PolicyData* policy = GetPolicyData(profile);
  if (!policy)
    return std::string();

  std::vector<std::string> user_affiliation_ids(
      policy->user_affiliation_ids().begin(),
      policy->user_affiliation_ids().end());
  return device_dm_token_callback.Run(user_affiliation_ids);
}

// Returns User DMToken for a given |profile| if:
// * |profile| is NOT incognito profile.
// * |profile| is NOT sign-in screen profile
// * user corresponding to a |profile| is managed.
// Otherwise returns empty string. More about DMToken:
// go/dmserver-domain-model#dmtoken.
std::string GetUserDmToken(Profile* profile) {
  if (!profile)
    return std::string();

  const enterprise_management::PolicyData* policy = GetPolicyData(profile);
  if (!policy || !policy->has_request_token())
    return std::string();

  return policy->request_token();
}
#endif  // OS_CHROMEOS

}  // namespace

namespace reporting {

base::Value GetContext(Profile* profile) {
  base::Value context(base::Value::Type::DICTIONARY);
  context.SetStringPath("browser.userAgent", GetUserAgent());

  if (!profile)
    return context;

  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry = nullptr;
  if (storage.GetProfileAttributesWithPath(profile->GetPath(), &entry)) {
    context.SetStringPath("profile.profileName", entry->GetName());
    context.SetStringPath("profile.gaiaEmail", entry->GetUserName());
  }

  context.SetStringPath("profile.profilePath", profile->GetPath().value());

#ifdef OS_CHROMEOS
  const enterprise_management::PolicyData* policy = GetPolicyData(profile);

  if (policy) {
    if (policy->has_device_id())
      context.SetStringPath("profile.clientId", policy->device_id());

    std::string device_dm_token = GetDeviceDmToken(profile);
    if (!device_dm_token.empty())
      context.SetStringPath("device.dmToken", device_dm_token);

    std::string user_dm_token = GetUserDmToken(profile);
    if (!user_dm_token.empty())
      context.SetStringPath("profile.dmToken", user_dm_token);
  }
#endif  // OS_CHROMEOS

  return context;
}

}  // namespace reporting
