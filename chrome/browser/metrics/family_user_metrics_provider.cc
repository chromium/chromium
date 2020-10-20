// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/family_user_metrics_provider.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace {

constexpr char kHistogramName[] = "ChromeOS.FamilyUser.LogSegment";

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

bool IsEnterpriseManaged() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->IsEnterpriseManaged();
}

}  // namespace

FamilyUserMetricsProvider::FamilyUserMetricsProvider()
    : identity_manager_observer_(this) {
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
void FamilyUserMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto_unused) {
  if (!log_segment_)
    return;
  base::UmaHistogramEnumeration(kHistogramName, log_segment_.value());
}

void FamilyUserMetricsProvider::OnUserSessionStarted(bool is_primary_user) {
  if (!is_primary_user)
    return;
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  DCHECK(primary_user);
  DCHECK(primary_user->is_profile_created());
  Profile* profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
  DCHECK(profile);
  DCHECK(chromeos::ProfileHelper::IsRegularProfile(profile));

  // Check for incognito profiles.
  if (!profile->IsRegularProfile()) {
    log_segment_ = LogSegment::kOther;
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  DCHECK(identity_manager);
  if (!identity_manager_observer_.IsObserving(identity_manager))
    identity_manager_observer_.Add(identity_manager);
  auto accounts_size = identity_manager->GetAccountsWithRefreshTokens().size();

  if (profile->IsChild() && accounts_size == 1) {
    log_segment_ = LogSegment::kSupervisedUser;
  } else if (profile->IsChild() && accounts_size > 1) {
    // If a supervised user has a secondary account, then the secondary
    // account must be EDU.
    log_segment_ = LogSegment::kSupervisedStudent;
  } else if (!IsEnterpriseManaged() &&
             GetMetricsLogSegment(profile) ==
                 enterprise_management::PolicyData::K12) {
    DCHECK(profile->GetProfilePolicyConnector()->IsManaged());
    // This is a K-12 EDU user on an unmanaged ChromeOS device.
    log_segment_ = LogSegment::kStudentAtHome;
  } else {
    log_segment_ = LogSegment::kOther;
  }
}

// Called when the user adds a secondary account. We're only interested in
// detecting when a supervised user adds an EDU secondary account.
void FamilyUserMetricsProvider::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  Profile* profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
  // Check for incognito profiles.
  if (!profile->IsRegularProfile())
    return;

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  auto accounts_size = identity_manager->GetAccountsWithRefreshTokens().size();

  // If a supervised user has a secondary account, then the secondary account
  // must be EDU.
  if (profile->IsChild() && accounts_size > 1)
    log_segment_ = LogSegment::kSupervisedStudent;
}

// static
const char* FamilyUserMetricsProvider::GetHistogramNameForTesting() {
  return kHistogramName;
}
