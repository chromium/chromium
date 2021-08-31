// Copyright 2020 The Chromium Authors. All rights reserved.
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

#if defined(OS_WIN)
#include "components/policy/core/common/management/platform_management_status_provider_win.h"
#endif

BrowserCloudManagementStatusProvider::BrowserCloudManagementStatusProvider() =
    default;

BrowserCloudManagementStatusProvider::~BrowserCloudManagementStatusProvider() =
    default;

EnterpriseManagementAuthority
BrowserCloudManagementStatusProvider::GetAuthority() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return EnterpriseManagementAuthority::NONE;
#elif !defined(OS_ANDROID)
  // A machine level user cloud policy manager is only created if the browser is
  // managed by CBCM.
  if (g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager() != nullptr) {
    return EnterpriseManagementAuthority::CLOUD_DOMAIN;
  }
  return EnterpriseManagementAuthority::NONE;
#else
  return EnterpriseManagementAuthority::NONE;
#endif
}

LocalBrowserManagementStatusProvider::LocalBrowserManagementStatusProvider() =
    default;

LocalBrowserManagementStatusProvider::~LocalBrowserManagementStatusProvider() =
    default;

EnterpriseManagementAuthority
LocalBrowserManagementStatusProvider::GetAuthority() {
  EnterpriseManagementAuthority result = EnterpriseManagementAuthority::NONE;
#if defined(OS_WIN)
  if (!g_browser_process->browser_policy_connector()->HasMachineLevelPolicies())
    return result;
  if (policy::DomainEnrollmentStatusProvider::IsEnrolledToDomain()) {
    result = policy::DomainEnrollmentStatusProvider::IsEnrolledToDomain()
                 ? EnterpriseManagementAuthority::DOMAIN_LOCAL
                 : EnterpriseManagementAuthority::COMPUTER_LOCAL;
  }
#endif
  return result;
}

ProfileCloudManagementStatusProvider::ProfileCloudManagementStatusProvider(
    Profile* profile)
    : profile_(profile) {}

ProfileCloudManagementStatusProvider::~ProfileCloudManagementStatusProvider() =
    default;

EnterpriseManagementAuthority
ProfileCloudManagementStatusProvider::GetAuthority() {
  return profile_ && profile_->GetProfilePolicyConnector() &&
                 profile_->GetProfilePolicyConnector()->IsManaged()
             ? EnterpriseManagementAuthority::CLOUD
             : EnterpriseManagementAuthority::NONE;
}
