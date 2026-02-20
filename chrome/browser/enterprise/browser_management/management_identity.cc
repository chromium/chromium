// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/browser_management/management_identity.h"

#include <optional>
#include <string>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_data_utils.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/browser_process_platform_part.h"
#else
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#endif

namespace {

const char* g_device_manager_for_testing = nullptr;

}  // namespace

ScopedDeviceManagerForTesting::ScopedDeviceManagerForTesting(
    const char* manager) {
  previous_manager_ = g_device_manager_for_testing;
  g_device_manager_for_testing = manager;
}

ScopedDeviceManagerForTesting::~ScopedDeviceManagerForTesting() {
  g_device_manager_for_testing = previous_manager_;
}

std::optional<std::string> GetEnterpriseAccountDomain(const Profile& profile) {
  if (g_browser_process->profile_manager()) {
    ProfileAttributesEntry* entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile.GetPath());
    if (std::optional<std::string> hosted_domain =
            entry ? entry->GetHostedDomain() : std::nullopt;
        hosted_domain.has_value() && !hosted_domain->empty()) {
      return hosted_domain;
    }
  }

  const std::string domain =
      enterprise_util::GetDomainFromEmail(profile.GetProfileUserName());
  if (!signin::AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(
          profile.GetProfileUserName())) {
    return std::nullopt;
  }
  return domain;
}

std::optional<std::string> GetDeviceManagerIdentity() {
  if (g_device_manager_for_testing) {
    return g_device_manager_for_testing;
  }

  if (!policy::ManagementServiceFactory::GetForPlatform()->IsManaged()) {
    return std::nullopt;
  }

#if BUILDFLAG(IS_CHROMEOS)
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return connector->GetEnterpriseDomainManager();
#else
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (base::FeatureList::IsEnabled(
          features::kEnterpriseManagementDisclaimerUsesCustomLabel)) {
    std::string custom_management_label =
        g_browser_process->local_state()
            ? g_browser_process->local_state()->GetString(
                  prefs::kEnterpriseCustomLabelForBrowser)
            : std::string();
    if (!custom_management_label.empty()) {
      return custom_management_label;
    }
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // The device is managed as
  // `policy::ManagementServiceFactory::GetForPlatform()->IsManaged()` returned
  // true. `policy::GetManagedBy` might return `std::nullopt` if
  // `policy::CloudPolicyStore` hasn't fully initialized yet.
  return policy::GetManagedBy(g_browser_process->browser_policy_connector()
                                  ->machine_level_user_cloud_policy_manager())
      .value_or(std::string());
#endif  // BUILDFLAG(IS_CHROMEOS)
}

std::optional<std::string> GetAccountManagerIdentity(Profile* profile) {
  if (!policy::ManagementServiceFactory::GetForProfile(profile)
           ->HasManagementAuthority(
               policy::EnterpriseManagementAuthority::CLOUD)) {
    return std::nullopt;
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (base::FeatureList::IsEnabled(
          features::kEnterpriseManagementDisclaimerUsesCustomLabel)) {
    std::string custom_management_label =
        profile->GetPrefs()->GetString(prefs::kEnterpriseCustomLabelForProfile);
    if (!custom_management_label.empty()) {
      return custom_management_label;
    }
  }
#endif

  const std::optional<std::string> managed_by =
      policy::GetManagedBy(profile->GetCloudPolicyManager());
  if (managed_by) {
    return *managed_by;
  }

  if (profile->GetProfilePolicyConnector()->IsUsingLocalTestPolicyProvider()) {
    return "Local Test Policies";
  }

  return GetEnterpriseAccountDomain(*profile);
}
