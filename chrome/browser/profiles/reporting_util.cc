// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/reporting_util.h"

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "components/account_id/account_id.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/users/affiliation.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

// Returns policy for the given |profile|. If failed to get policy returns
// nullptr.
const enterprise_management::PolicyData* GetPolicyData(Profile* profile) {
  if (!profile)
    return nullptr;

  auto* manager = profile->GetCloudPolicyManager();
  if (!manager) {
    return nullptr;
  }

  policy::CloudPolicyStore* store = manager->core()->store();
  if (!store || !store->has_policy()) {
    return nullptr;
  }

  return store->policy();
}

#if BUILDFLAG(IS_CHROMEOS)
// A callback which fetches device dm_token based on user affiliation.
using DeviceDMTokenCallback = base::RepeatingCallback<std::string(
    const std::vector<std::string>& user_affiliation_ids)>;

// Returns the Device DMToken for the given |profile| if:
// * |profile| is NOT incognito profile
// * user corresponding to a given |profile| is affiliated.
// Otherwise returns empty string. More about DMToken:
// go/dmserver-domain-model#dmtoken.
std::string GetDeviceDmToken(Profile* profile) {
  if (!profile)
    return std::string();

  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return std::string();

  DeviceDMTokenCallback device_dm_token_callback =
      ash::GetDeviceDMTokenForUserPolicyGetter(user->GetAccountId());
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

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

namespace reporting {

base::Value::Dict GetContext(Profile* profile) {
  base::Value::Dict context;
  context.SetByDottedPath("browser.userAgent",
                          embedder_support::GetUserAgent());

  if (!profile)
    return context;

  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(profile->GetPath());
  if (entry) {
    context.SetByDottedPath("profile.profileName", entry->GetName());
    context.SetByDottedPath("profile.gaiaEmail", entry->GetUserName());
  }

  context.SetByDottedPath("profile.profilePath",
                          profile->GetPath().AsUTF8Unsafe());

  std::optional<std::string> client_id = GetUserClientId(profile);
  if (client_id)
    context.SetByDottedPath("profile.clientId", *client_id);

#if BUILDFLAG(IS_CHROMEOS)
  std::string device_dm_token = GetDeviceDmToken(profile);
  if (!device_dm_token.empty())
    context.SetByDottedPath("device.dmToken", device_dm_token);
#endif

  std::optional<std::string> user_dm_token = GetUserDmToken(profile);
  if (user_dm_token)
    context.SetByDottedPath("profile.dmToken", *user_dm_token);

  return context;
}

enterprise_connectors::ClientMetadata GetContextAsClientMetadata(
    Profile* profile) {
  enterprise_connectors::ClientMetadata metadata;
  metadata.mutable_browser()->set_user_agent(embedder_support::GetUserAgent());

  if (!profile)
    return metadata;

  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(profile->GetPath());
  if (entry) {
    metadata.mutable_profile()->set_profile_name(
        base::UTF16ToUTF8(entry->GetName()));
    metadata.mutable_profile()->set_gaia_email(
        base::UTF16ToUTF8(entry->GetUserName()));
  }

  metadata.mutable_profile()->set_profile_path(
      profile->GetPath().AsUTF8Unsafe());

  std::optional<std::string> client_id = GetUserClientId(profile);
  if (client_id)
    metadata.mutable_profile()->set_client_id(*client_id);

#if BUILDFLAG(IS_CHROMEOS)
  std::string device_dm_token = GetDeviceDmToken(profile);
  if (!device_dm_token.empty())
    metadata.mutable_device()->set_dm_token(device_dm_token);
#endif

  std::optional<std::string> user_dm_token = GetUserDmToken(profile);
  if (user_dm_token)
    metadata.mutable_profile()->set_dm_token(*user_dm_token);

  return metadata;
}

// Returns User DMToken for a given |profile| if:
// * |profile| is NOT incognito profile.
// * |profile| is NOT sign-in screen profile
// * user corresponding to a |profile| is managed.
// Otherwise returns empty string. More about DMToken:
// go/dmserver-domain-model#dmtoken.
std::optional<std::string> GetUserDmToken(Profile* profile) {
  if (!profile)
    return std::nullopt;

  const enterprise_management::PolicyData* policy_data = GetPolicyData(profile);
  if (!policy_data || !policy_data->has_request_token())
    return std::nullopt;
  return policy_data->request_token();
}

std::optional<std::string> GetUserClientId(Profile* profile) {
  if (!profile)
    return std::nullopt;

  const enterprise_management::PolicyData* policy_data = GetPolicyData(profile);
  if (!policy_data || !policy_data->has_device_id())
    return std::nullopt;
  return policy_data->device_id();
}

#if BUILDFLAG(IS_CHROMEOS)
std::optional<std::string> GetMGSUserClientId() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  policy::DeviceLocalAccountPolicyService* policy_service =
      connector->GetDeviceLocalAccountPolicyService();

  // The policy service is null if the device is managed by Active Directory.
  if (!policy_service) {
    return std::nullopt;
  }

  const policy::DeviceLocalAccountPolicyBroker* policy_broker =
      policy_service->GetBrokerForUser(
          user_manager->GetActiveUser()->GetAccountId().GetUserEmail());

  // The policy broker is null if the active user does not belong to an existing
  // device-local account, which should never be the case when calling this
  // function.
  DCHECK(policy_broker);

  const enterprise_management::PolicyData* policy_data =
      policy_broker->core()->store()->policy();

  if (policy_data && policy_data->has_device_id()) {
    return policy_data->device_id();
  } else {
    return std::nullopt;
  }
}
#endif

}  // namespace reporting
