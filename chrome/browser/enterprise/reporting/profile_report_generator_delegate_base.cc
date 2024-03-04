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
    enterprise_management::ChromeUserProfileInfo* report) {
  auto account_info =
      IdentityManagerFactory::GetForProfile(profile_)->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSync);
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
      is_machine_scope ||
      chrome::enterprise_util::IsProfileAffiliated(profile_));

  return client;
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
  auto* cloud_policy_manager = profile_->GetUserCloudPolicyManager();
  if (cloud_policy_manager) {
    return cloud_policy_manager;
  }

  return profile_->GetProfileCloudPolicyManager();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace enterprise_reporting
