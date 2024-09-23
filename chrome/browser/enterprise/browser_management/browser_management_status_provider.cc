// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/browser_management/browser_management_status_provider.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"

#if BUILDFLAG(IS_WIN)
#include "components/policy/core/common/management/platform_management_status_provider_win.h"
#elif BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "components/user_manager/user_manager.h"
#endif

namespace {

bool IsProfileManaged(Profile* profile) {
  return profile && profile->GetProfilePolicyConnector() &&
         profile->GetProfilePolicyConnector()->IsManaged();
}

}  // namespace

BrowserCloudManagementStatusProvider::BrowserCloudManagementStatusProvider() =
    default;

BrowserCloudManagementStatusProvider::~BrowserCloudManagementStatusProvider() =
    default;

EnterpriseManagementAuthority
BrowserCloudManagementStatusProvider::FetchAuthority() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return EnterpriseManagementAuthority::NONE;
#else
  // A machine level user cloud policy manager is only created if the browser is
  // managed by CBCM.
  if (g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager() != nullptr) {
    return EnterpriseManagementAuthority::CLOUD_DOMAIN;
  }
  return EnterpriseManagementAuthority::NONE;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

LocalBrowserManagementStatusProvider::LocalBrowserManagementStatusProvider() =
    default;

LocalBrowserManagementStatusProvider::~LocalBrowserManagementStatusProvider() =
    default;

EnterpriseManagementAuthority
LocalBrowserManagementStatusProvider::FetchAuthority() {
// BrowserPolicyConnector::HasMachineLevelPolicies is not supported on Chrome
// OS.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return EnterpriseManagementAuthority::NONE;
#else
  return g_browser_process && g_browser_process->browser_policy_connector() &&
                 g_browser_process->browser_policy_connector()
                     ->HasMachineLevelPolicies()
             ? EnterpriseManagementAuthority::COMPUTER_LOCAL
             : EnterpriseManagementAuthority::NONE;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

LocalDomainBrowserManagementStatusProvider::
    LocalDomainBrowserManagementStatusProvider() = default;

LocalDomainBrowserManagementStatusProvider::
    ~LocalDomainBrowserManagementStatusProvider() = default;

EnterpriseManagementAuthority
LocalDomainBrowserManagementStatusProvider::FetchAuthority() {
  auto result = EnterpriseManagementAuthority::NONE;
// BrowserPolicyConnector::HasMachineLevelPolicies is not supported on Chrome
// OS.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return result;
#else
  if (g_browser_process->browser_policy_connector()
          ->HasMachineLevelPolicies()) {
    result = EnterpriseManagementAuthority::COMPUTER_LOCAL;
#if BUILDFLAG(IS_WIN)
    if (policy::DomainEnrollmentStatusProvider::IsEnrolledToDomain())
      result = EnterpriseManagementAuthority::DOMAIN_LOCAL;
#endif  // BUILDFLAG(IS_WIN)
  }
  return result;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

ProfileCloudManagementStatusProvider::ProfileCloudManagementStatusProvider(
    Profile* profile)
    : profile_(profile) {}

ProfileCloudManagementStatusProvider::~ProfileCloudManagementStatusProvider() =
    default;

EnterpriseManagementAuthority
ProfileCloudManagementStatusProvider::FetchAuthority() {
  if (IsProfileManaged(profile_))
    return EnterpriseManagementAuthority::CLOUD;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // This session's primary user may also have policies, and those policies may
  // not have per-profile support.
  auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (primary_user &&
      IsProfileManaged(
          ash::ProfileHelper::Get()->GetProfileByUser(primary_user))) {
    return EnterpriseManagementAuthority::CLOUD;
  }
#endif
  return EnterpriseManagementAuthority::NONE;
}

LocalTestPolicyUserManagementProvider::LocalTestPolicyUserManagementProvider(
    Profile* profile)
    : profile_(profile) {}

LocalTestPolicyUserManagementProvider::
    ~LocalTestPolicyUserManagementProvider() = default;

EnterpriseManagementAuthority
LocalTestPolicyUserManagementProvider::FetchAuthority() {
  if (!profile_->GetProfilePolicyConnector()
           ->IsUsingLocalTestPolicyProvider()) {
    return EnterpriseManagementAuthority::NONE;
  }
  for (const auto& [_, entry] :
       profile_->GetProfilePolicyConnector()->policy_service()->GetPolicies(
           policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                   std::string()))) {
    if (entry.scope == policy::POLICY_SCOPE_USER &&
        entry.source == policy::POLICY_SOURCE_CLOUD) {
      return EnterpriseManagementAuthority::CLOUD;
    }
  }
  return EnterpriseManagementAuthority::NONE;
}

LocalTestPolicyBrowserManagementProvider::
    LocalTestPolicyBrowserManagementProvider(Profile* profile)
    : profile_(profile) {}

LocalTestPolicyBrowserManagementProvider::
    ~LocalTestPolicyBrowserManagementProvider() = default;

EnterpriseManagementAuthority
LocalTestPolicyBrowserManagementProvider::FetchAuthority() {
  if (!profile_->GetProfilePolicyConnector()
           ->IsUsingLocalTestPolicyProvider()) {
    return EnterpriseManagementAuthority::NONE;
  }
  for (const auto& [_, entry] :
       profile_->GetProfilePolicyConnector()->policy_service()->GetPolicies(
           policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                   std::string()))) {
    if (entry.scope == policy::POLICY_SCOPE_MACHINE &&
        entry.source == policy::POLICY_SOURCE_CLOUD) {
      return EnterpriseManagementAuthority::CLOUD_DOMAIN;
    }
    if (entry.scope == policy::POLICY_SCOPE_MACHINE &&
        entry.source == policy::POLICY_SOURCE_PLATFORM) {
      return EnterpriseManagementAuthority::DOMAIN_LOCAL;
    }
  }
  return EnterpriseManagementAuthority::NONE;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
DeviceManagementStatusProvider::DeviceManagementStatusProvider() = default;

DeviceManagementStatusProvider::~DeviceManagementStatusProvider() = default;

EnterpriseManagementAuthority DeviceManagementStatusProvider::FetchAuthority() {
  return g_browser_process && g_browser_process->platform_part() &&
                 g_browser_process->platform_part()
                     ->browser_policy_connector_ash() &&
                 g_browser_process->platform_part()
                     ->browser_policy_connector_ash()
                     ->IsDeviceEnterpriseManaged()
             ? EnterpriseManagementAuthority::CLOUD_DOMAIN
             : EnterpriseManagementAuthority::NONE;
}
#endif
