// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/family_user_metrics_provider.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace {

constexpr char kFamilyUserLogSegmentHistogramName[] =
    "ChromeOS.FamilyUser.LogSegment2";
constexpr char kNumSecondaryAccountsHistogramName[] =
    "ChromeOS.FamilyUser.NumSecondaryAccounts";

// Returns managed user log segment for metrics logging.
enterprise_management::PolicyData::MetricsLogSegment GetManagedUserLogSegment(
    Profile* profile) {
  const policy::UserCloudPolicyManagerAsh* user_cloud_policy_manager =
      profile->GetUserCloudPolicyManagerAsh();
  if (!user_cloud_policy_manager)
    return enterprise_management::PolicyData::UNSPECIFIED;
  const enterprise_management::PolicyData* policy =
      user_cloud_policy_manager->core()->store()->policy();
  if (!policy || !policy->has_metrics_log_segment())
    return enterprise_management::PolicyData::UNSPECIFIED;
  return policy->metrics_log_segment();
}

// Returns if the device is managed, independent of the user.
bool IsDeviceEnterpriseEnrolled() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return connector->IsDeviceEnterpriseManaged();
}

Profile* GetPrimaryUserProfile() {
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  DCHECK(primary_user);
  DCHECK(primary_user->is_profile_created());
  Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(primary_user);
  DCHECK(profile);
  DCHECK(ash::ProfileHelper::IsUserProfile(profile));
  return profile;
}

// Can return -1 for guest users, browser tests, and other edge cases. If -1,
// then no metrics uploaded.
int GetNumSecondaryAccounts(Profile* profile) {
  // Check for incognito profiles.
  if (profile->IsOffTheRecord())
    return -1;

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  DCHECK(identity_manager);
  if (!identity_manager->AreRefreshTokensLoaded()) {
    // IdentityManager hasn't finished loading accounts, return -1 to indicate
    // that we don't know the number of secondary accounts yet.
    return -1;
  }
  int num_accounts = identity_manager->GetAccountsWithRefreshTokens().size();
  return num_accounts - 1;
}

}  // namespace

FamilyUserMetricsProvider::FamilyUserMetricsProvider() {
  session_manager::SessionManager* session_manager =
      session_manager::SessionManager::Get();
  // The |session_manager| is nullptr only for unit tests.
  if (session_manager)
    session_manager->AddObserver(this);
}

FamilyUserMetricsProvider::~FamilyUserMetricsProvider() {
  session_manager::SessionManager* session_manager =
      session_manager::SessionManager::Get();
  // The |session_manager| is nullptr only for unit tests.
  if (session_manager)
    session_manager->RemoveObserver(this);
}

// This function is called at unpredictable intervals throughout the entire
// ChromeOS session, so guarantee it will never crash.
bool FamilyUserMetricsProvider::ProvideHistograms() {
  if (!family_user_log_segment_)
    return false;

  base::UmaHistogramEnumeration(kFamilyUserLogSegmentHistogramName,
                                family_user_log_segment_.value());

  if (num_secondary_accounts_ >= 0) {
    base::UmaHistogramCounts100(kNumSecondaryAccountsHistogramName,
                                num_secondary_accounts_);
  }

  return true;
}

void FamilyUserMetricsProvider::OnUserSessionStarted(bool is_primary_user) {
  if (!is_primary_user)
    return;
  Profile* profile = GetPrimaryUserProfile();
  ObserveIdentityManager(profile);

  num_secondary_accounts_ = GetNumSecondaryAccounts(profile);

  if (IsSupervisedUser(profile)) {
    family_user_log_segment_ = FamilyUserLogSegment::kSupervisedUser;
  } else if (IsSupervisedStudent(profile)) {
    family_user_log_segment_ = FamilyUserLogSegment::kSupervisedStudent;
  } else if (!IsDeviceEnterpriseEnrolled() &&
             GetManagedUserLogSegment(profile) ==
                 enterprise_management::PolicyData::K12) {
    DCHECK(profile->GetProfilePolicyConnector()->IsManaged());
    // This is a K-12 EDU user on an unmanaged ChromeOS device.
    family_user_log_segment_ = FamilyUserLogSegment::kStudentAtHome;
  } else if (profile->IsRegularProfile() &&
             !profile->GetProfilePolicyConnector()->IsManaged()) {
    DCHECK(!profile->IsChild());
    DCHECK_EQ(GetManagedUserLogSegment(profile),
              enterprise_management::PolicyData::UNSPECIFIED);
    // This is a regular unmanaged user on any device.
    family_user_log_segment_ = FamilyUserLogSegment::kRegularUser;
  } else {
    family_user_log_segment_ = FamilyUserLogSegment::kOther;
  }
}

// Called when the user adds a secondary account. We're only interested in
// detecting when a supervised user adds an EDU secondary account.
void FamilyUserMetricsProvider::OnRefreshTokensLoaded() {
  Profile* profile = GetPrimaryUserProfile();

  num_secondary_accounts_ = GetNumSecondaryAccounts(profile);

  // If a supervised user has a secondary account, then the secondary account
  // must be EDU.
  if (IsSupervisedStudent(profile))
    family_user_log_segment_ = FamilyUserLogSegment::kSupervisedStudent;
}

void FamilyUserMetricsProvider::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  // Call OnRefreshTokensLoaded to update `num_secondary_accounts_` and
  // `family_user_log_segment_`.
  OnRefreshTokensLoaded();
}

// Called when the user removes a secondary account. We're interested in
// detecting when a supervised user removes an EDU secondary account.
void FamilyUserMetricsProvider::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  // Call OnRefreshTokensLoaded to update `num_secondary_accounts_` and
  // `family_user_log_segment_`.
  OnRefreshTokensLoaded();
}

// static
const char*
FamilyUserMetricsProvider::GetFamilyUserLogSegmentHistogramNameForTesting() {
  return kFamilyUserLogSegmentHistogramName;
}
const char*
FamilyUserMetricsProvider::GetNumSecondaryAccountsHistogramNameForTesting() {
  return kNumSecondaryAccountsHistogramName;
}

void FamilyUserMetricsProvider::ObserveIdentityManager(Profile* profile) {
  // Check for incognito profiles.
  if (profile->IsOffTheRecord())
    return;

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  DCHECK(identity_manager);
  if (!identity_manager_observations_.IsObservingSource(identity_manager))
    identity_manager_observations_.AddObservation(identity_manager);
}

bool FamilyUserMetricsProvider::IsSupervisedUser(Profile* profile) {
  if (!profile->IsChild())
    return false;
  return num_secondary_accounts_ == 0;
}

bool FamilyUserMetricsProvider::IsSupervisedStudent(Profile* profile) {
  if (!profile->IsChild())
    return false;
  // If a supervised user has a secondary account, then the secondary
  // account must be EDU.
  return num_secondary_accounts_ > 0;
}
