// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/profile_report_generator_delegate_base.h"

#include <memory>

#include "base/files/file_path.h"
#include "build/branding_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/enterprise/browser/reporting/profile_report_generator.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/browser/policy_conversions_client.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/profile_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"

namespace em = enterprise_management;

namespace enterprise_reporting {

ProfileReportGeneratorDelegateBase::ProfileReportGeneratorDelegateBase() =
    default;

ProfileReportGeneratorDelegateBase::~ProfileReportGeneratorDelegateBase() =
    default;

bool ProfileReportGeneratorDelegateBase::Init(const base::FilePath& path) {
  profile_ = g_browser_process->profile_manager()->GetProfileByPath(path);

  return profile_ != nullptr;
}

void ProfileReportGeneratorDelegateBase::GetSigninUserInfo(
    em::ChromeUserProfileInfo* report) {
  signin::ConsentLevel consent_level =
      base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos)
          ? signin::ConsentLevel::kSignin
          : signin::ConsentLevel::kSync;
  auto account_info =
      IdentityManagerFactory::GetForProfile(profile_)->GetPrimaryAccountInfo(
          consent_level);
  if (account_info.IsEmpty())
    return;
  auto* signed_in_user_info = report->mutable_chrome_signed_in_user();
  signed_in_user_info->set_email(account_info.email);
  signed_in_user_info->set_obfuscated_gaia_id(account_info.gaia);
}

std::unique_ptr<policy::PolicyConversionsClient>
ProfileReportGeneratorDelegateBase::MakePolicyConversionsClient(
    bool is_machine_scope) {
  auto client =
      std::make_unique<policy::ChromePolicyConversionsClient>(profile_);

  // For profile reporting, if user is not affiliated, we need to hide machine
  // policy value.
  client->EnableShowMachineValues(
      is_machine_scope || enterprise_util::IsProfileAffiliated(profile_));

  return client;
}

void ProfileReportGeneratorDelegateBase::GetAffiliationInfo(
    em::ChromeUserProfileInfo* report) {
  auto* affiliation_state = report->mutable_affiliation();
  if (enterprise_util::IsProfileAffiliated(profile_)) {
    affiliation_state->set_is_affiliated(true);
    return;
  }
  affiliation_state->set_is_affiliated(false);
  switch (enterprise_util::GetUnaffiliatedReason(profile_)) {
    case enterprise_util::ProfileUnaffiliatedReason::kUserUnmanaged:
      affiliation_state->set_unaffiliation_reason(
          em::AffiliationState_UnaffiliationReason_USER_UNMANAGED);
      break;
    case enterprise_util::ProfileUnaffiliatedReason::
        kUserByCloudAndDeviceUnmanaged:
      affiliation_state->set_unaffiliation_reason(
          em::AffiliationState_UnaffiliationReason_DEVICE_UNMANAGED);
      break;
    case enterprise_util::ProfileUnaffiliatedReason::
        kUserByCloudAndDeviceByPlatform:
      affiliation_state->set_unaffiliation_reason(
          em::AffiliationState_UnaffiliationReason_DEVICE_MANAGED_BY_PLATFORM);
      break;
    case enterprise_util::ProfileUnaffiliatedReason::
        kUserAndDeviceByCloudUnaffiliated:
      affiliation_state->set_unaffiliation_reason(
          em::AffiliationState_UnaffiliationReason_DEVICE_MANANGED_DIFFERENT_DOMAIN);
      break;
  }
  return;
}

policy::CloudPolicyManager*
ProfileReportGeneratorDelegateBase::GetCloudPolicyManager(
    bool is_machine_scope) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return nullptr;
#else
  // CBCM report will include CBCM policy fetch information.
  if (is_machine_scope) {
    return g_browser_process->browser_policy_connector()
        ->machine_level_user_cloud_policy_manager();
  }

  // Profile report will include user cloud policy information by default.
  // Or ProfileCloudPolicyManager when it's not managed by gaia account.
  return profile_->GetCloudPolicyManager();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace enterprise_reporting
