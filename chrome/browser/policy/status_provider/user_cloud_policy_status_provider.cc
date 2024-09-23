// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/user_cloud_policy_status_provider.h"

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/status_provider/status_provider_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"

UserCloudPolicyStatusProvider::UserCloudPolicyStatusProvider(
    policy::CloudPolicyCore* core,
    Profile* profile)
    : CloudPolicyCoreStatusProvider(core), profile_(profile) {}

UserCloudPolicyStatusProvider::~UserCloudPolicyStatusProvider() = default;

base::Value::Dict UserCloudPolicyStatusProvider::GetStatus() {
#if BUILDFLAG(IS_CHROMEOS)
  const bool show_flex_org_warning = false;
#else
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  const bool show_flex_org_warning =
      identity_manager && identity_manager
                              ->FindExtendedAccountInfoByEmailAddress(
                                  profile_->GetProfileUserName())
                              .IsMemberOfFlexOrg();
  if (!show_flex_org_warning && !core_->store()->is_managed()) {
    return {};
  }
#endif

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_->GetPath());
  auto enrollment_token =
      entry ? entry->GetProfileManagementEnrollmentToken() : std::string();

  base::Value::Dict dict =
      policy::PolicyStatusProvider::GetStatusFromCore(core_);

  if (enrollment_token.empty()) {
    SetDomainExtractedFromUsername(dict);
    GetUserAffiliationStatus(&dict, profile_);
    dict.Set(policy::kFlexOrgWarningKey, show_flex_org_warning);
  } else {
    dict.Set(policy::kEnrollmentTokenKey, enrollment_token);
    dict.Set(policy::kDomainKey,
             gaia::ExtractDomainName(core_->store()->policy()->username()));
    dict.Remove(policy::kUsernameKey);
    dict.Remove(policy::kGaiaIdKey);
  }
  UpdateLastReportTimestamp(
      dict, profile_->GetPrefs(),
      enterprise_reporting::kLastUploadSucceededTimestamp);
  dict.Set(policy::kPolicyDescriptionKey, kUserPolicyStatusDescription);
  SetProfileId(&dict, profile_);
  return dict;
}
