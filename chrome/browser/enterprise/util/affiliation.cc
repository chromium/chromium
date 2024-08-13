// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/affiliation.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/affiliation.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/policy/core/common/policy_loader_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace enterprise_util {

bool IsProfileAffiliated(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (profile->IsMainProfile()) {
    return policy::PolicyLoaderLacros::IsMainUserAffiliated();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  return policy::IsAffiliated(
      profile->GetProfilePolicyConnector()->user_affiliation_ids(),
      g_browser_process->browser_policy_connector()->device_affiliation_ids());
}

ProfileUnaffiliatedReason GetUnaffiliatedReason(Profile* profile) {
  CHECK(!IsProfileAffiliated(profile));
  return GetUnaffiliatedReason(profile->GetProfilePolicyConnector());
}

ProfileUnaffiliatedReason GetUnaffiliatedReason(
    policy::ProfilePolicyConnector* connector) {
  if (!connector->IsManaged()) {
    return ProfileUnaffiliatedReason::kUserUnmanaged;
  }

  if (g_browser_process->browser_policy_connector()
          ->device_affiliation_ids()
          .size() > 0) {
    return ProfileUnaffiliatedReason::kUserAndDeviceByCloudUnaffiliated;
  }

  if (policy::ManagementServiceFactory::GetForPlatform()->IsBrowserManaged()) {
    return ProfileUnaffiliatedReason::kUserByCloudAndDeviceByPlatform;
  }

  return ProfileUnaffiliatedReason::kUserByCloudAndDeviceUnmanaged;
}

}  // namespace enterprise_util
