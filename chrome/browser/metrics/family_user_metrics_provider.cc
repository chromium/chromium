// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/family_user_metrics_provider.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/child_accounts/family_features.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace {

// Returns user's segment for metrics logging.
enterprise_management::PolicyData::MetricsLogSegment GetMetricsLogSegment(
    Profile* profile) {
  const policy::UserCloudPolicyManagerChromeOS* user_cloud_policy_manager =
      profile->GetUserCloudPolicyManagerChromeOS();
  if (!user_cloud_policy_manager)
    return enterprise_management::PolicyData::UNSPECIFIED;
  const enterprise_management::PolicyData* policy =
      user_cloud_policy_manager->core()->store()->policy();
  if (!policy || !policy->has_metrics_log_segment())
    return enterprise_management::PolicyData::UNSPECIFIED;
  return policy->metrics_log_segment();
}

bool IsLoggedIn() {
  return user_manager::UserManager::IsInitialized() &&
         user_manager::UserManager::Get()->IsUserLoggedIn();
}

bool IsEnterpriseManaged() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->IsEnterpriseManaged();
}

}  // namespace

// static
const char FamilyUserMetricsProvider::kFamilyUserLogSegmentHistogramName[] =
    "ChromeOS.FamilyUser.LogSegment";

FamilyUserMetricsProvider::FamilyUserMetricsProvider() = default;

FamilyUserMetricsProvider::~FamilyUserMetricsProvider() = default;

void FamilyUserMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto_unused) {
  if (!base::FeatureList::IsEnabled(chromeos::kFamilyUserMetricsProvider))
    return;
  if (!IsLoggedIn())
    return;
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user || !primary_user->is_profile_created())
    return;
  Profile* profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
  DCHECK(profile);
  DCHECK(chromeos::ProfileHelper::IsRegularProfile(profile));
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  auto accounts_size = identity_manager->GetAccountsWithRefreshTokens().size();
  DCHECK_GT(accounts_size, 0);

  LogSegment log_segment = LogSegment::kOther;
  if (profile->IsChild() && accounts_size == 1) {
    log_segment = LogSegment::kSupervisedUser;
  } else if (profile->IsChild() && accounts_size > 1) {
    // If a supervised user has a secondary account, then the secondary
    // account must be EDU.
    log_segment = LogSegment::kSupervisedStudent;
  } else if (!IsEnterpriseManaged() &&
             GetMetricsLogSegment(profile) ==
                 enterprise_management::PolicyData::K12) {
    DCHECK(profile->GetProfilePolicyConnector()->IsManaged());
    // This is a K-12 EDU user on an unmanaged ChromeOS device.
    log_segment = LogSegment::kStudentAtHome;
  }
  base::UmaHistogramEnumeration(kFamilyUserLogSegmentHistogramName,
                                log_segment);
}
