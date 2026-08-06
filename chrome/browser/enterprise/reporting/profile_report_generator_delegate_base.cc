// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/profile_report_generator_delegate_base.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "base/json/values_util.h"
#include "chrome/browser/enterprise/reporting/extension_info.h"
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_report_generator.h"
#include "chrome/browser/extensions/extension_management.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/prefs/pref_service.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_urls.h"
#endif
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
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
#include "google_apis/gaia/gaia_id.h"

namespace em = enterprise_management;

namespace enterprise_reporting {

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
constexpr int kMaxNumberOfExtensionRequest = 1000;
#endif

}  // namespace

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
      syncer::IsReplaceSyncPromosWithSignInPromosEnabled()
          ? signin::ConsentLevel::kSignin
          : signin::ConsentLevel::kSync;
  auto account_info =
      IdentityManagerFactory::GetForProfile(profile_)->GetPrimaryAccountInfo(
          consent_level);
  if (account_info.IsEmpty()) {
    return;
  }
  auto* signed_in_user_info = report->mutable_chrome_signed_in_user();
  signed_in_user_info->set_email(account_info.email);
  signed_in_user_info->set_obfuscated_gaia_id(account_info.gaia.ToString());
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

void ProfileReportGeneratorDelegateBase::GetProfileId(
    em::ChromeUserProfileInfo* report) {
  auto profile_id = enterprise::ProfileIdServiceFactory::GetForProfile(profile_)
                        ->GetProfileId();
  if (profile_id) {
    report->set_profile_id(*profile_id);
  }
}

void ProfileReportGeneratorDelegateBase::GetProfileName(
    em::ChromeUserProfileInfo* report) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // profile manager may not be available in test.
  if (!profile_manager) {
    report->set_name(std::string());
    return;
  }

  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(profile_->GetPath());
  std::string name =
      entry ? base::UTF16ToUTF8(entry->GetName()) : std::string();
  report->set_name(name);
}

policy::CloudPolicyManager*
ProfileReportGeneratorDelegateBase::GetCloudPolicyManager(
    bool is_machine_scope) {
#if BUILDFLAG(IS_CHROMEOS)
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
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void ProfileReportGeneratorDelegateBase::GetExtensionInfo(
    enterprise_management::ChromeUserProfileInfo* report) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  AppendExtensionInfoIntoProfileReport(profile_, report);
#endif
}

void ProfileReportGeneratorDelegateBase::GetExtensionRequest(
    enterprise_management::ChromeUserProfileInfo* report) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (!profile_->GetPrefs()->GetBoolean(
          enterprise_reporting::kCloudExtensionRequestEnabled)) {
    return;
  }
  const base::DictValue& pending_requests = profile_->GetPrefs()->GetDict(
      enterprise_reporting::kCloudExtensionRequestIds);

  extensions::ExtensionManagement* extension_management =
      extensions::ExtensionManagementFactory::GetForBrowserContext(profile_);
  std::string webstore_update_url =
      extension_urls::GetDefaultWebstoreUpdateUrl().spec();

  int number_of_requests = 0;
  for (auto [extension_id, request_data] : pending_requests) {
    if (!ExtensionRequestReportGenerator::ShouldUploadExtensionRequest(
            extension_id, webstore_update_url, extension_management)) {
      continue;
    }

    number_of_requests += 1;
    if (number_of_requests > kMaxNumberOfExtensionRequest) {
      break;
    }

    auto* request = report->add_extension_requests();
    request->set_id(extension_id);

    const auto& request_data_dict = request_data.GetDict();
    std::optional<base::Time> timestamp = ::base::ValueToTime(
        request_data_dict.Find(extension_misc::kExtensionRequestTimestamp));
    if (timestamp) {
      request->set_request_timestamp(timestamp->InMillisecondsSinceUnixEpoch());
    }

    const std::string* justification = request_data_dict.FindString(
        extension_misc::kExtensionWorkflowJustification);
    if (justification) {
      request->set_justification(*justification);
    }
  }
#endif
}

}  // namespace enterprise_reporting
