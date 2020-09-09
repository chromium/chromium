// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/family_user_metrics_provider.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/components/account_manager/account_manager_factory.h"

// static
const char FamilyUserMetricsProvider::kFamilyUserLogSegmentHistogramName[] =
    "ChromeOS.FamilyUser.LogSegment";

FamilyUserMetricsProvider::FamilyUserMetricsProvider() = default;

FamilyUserMetricsProvider::~FamilyUserMetricsProvider() = default;

void FamilyUserMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto_unused) {
  Profile* profile = cached_profile_.GetMetricsProfile();
  DCHECK(profile);
  chromeos::AccountManager* account_manager =
      g_browser_process->platform_part()
          ->GetAccountManagerFactory()
          ->GetAccountManager(profile->GetPath().value());
  // Calls the callback immediately and not asynchronously.
  account_manager->GetAccounts(base::BindOnce(
      &FamilyUserMetricsProvider::CheckSecondaryAccountsAndLogSegment,
      weak_factory_.GetWeakPtr()));
}

void FamilyUserMetricsProvider::CheckSecondaryAccountsAndLogSegment(
    const std::vector<chromeos::AccountManager::Account>& accounts) {
  DCHECK(!accounts.empty());
  LogSegment log_segment = LogSegment::kOther;

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  DCHECK(connector);
  Profile* profile = cached_profile_.GetMetricsProfile();
  DCHECK(profile);

  if (profile->IsChild() && accounts.size() == 1) {
    log_segment = LogSegment::kSupervisedUser;
  } else if (profile->IsChild() && accounts.size() > 1) {
    // If a supervised user has a secondary account, then the secondary account
    // must be K-12 EDU.
    log_segment = LogSegment::kSupervisedStudent;
  } else if (!connector->IsEnterpriseManaged() &&
             GetMetricsLogSegment() == enterprise_management::PolicyData::K12) {
    DCHECK(profile->GetProfilePolicyConnector()->IsManaged());
    // This is a K-12 EDU user on an unmanaged ChromeOS device.
    log_segment = LogSegment::kStudentAtHome;
  }

  base::UmaHistogramEnumeration(kFamilyUserLogSegmentHistogramName,
                                log_segment);
}

enterprise_management::PolicyData::MetricsLogSegment
FamilyUserMetricsProvider::GetMetricsLogSegment() {
  Profile* profile = cached_profile_.GetMetricsProfile();
  DCHECK(profile);
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
