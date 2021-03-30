// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/profile_report_generator_desktop.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/util/values/values_util.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/reporting/extension_info.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/enterprise/browser/reporting/policy_info.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "extensions/common/extension_urls.h"

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {

const int kMaxNumberOfExtensionRequest = 1000;

// Extension request are moved out of the pending list once user confirm the
// notification. However, there is no need to upload these requests anymore as
// long as admin made a decision.
bool ShouldUploadExtensionRequest(
    const std::string& extension_id,
    const std::string& webstore_update_url,
    extensions::ExtensionManagement* extension_management) {
  auto mode = extension_management->GetInstallationMode(extension_id,
                                                        webstore_update_url);
  return (mode == extensions::ExtensionManagement::INSTALLATION_BLOCKED ||
          mode == extensions::ExtensionManagement::INSTALLATION_REMOVED) &&
         !extension_management->IsInstallationExplicitlyBlocked(extension_id);
}

}  // namespace

ProfileReportGeneratorDesktop::ProfileReportGeneratorDesktop() = default;

ProfileReportGeneratorDesktop::~ProfileReportGeneratorDesktop() = default;

bool ProfileReportGeneratorDesktop::Init(const base::FilePath& path) {
  profile_ = g_browser_process->profile_manager()->GetProfileByPath(path);

  if (!profile_) {
    return false;
  }

  return true;
}

void ProfileReportGeneratorDesktop::GetSigninUserInfo(
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

void ProfileReportGeneratorDesktop::GetExtensionInfo(
    enterprise_management::ChromeUserProfileInfo* report) {
  AppendExtensionInfoIntoProfileReport(profile_, report);
}

void ProfileReportGeneratorDesktop::GetExtensionRequest(
    enterprise_management::ChromeUserProfileInfo* report) {
  if (!profile_->GetPrefs()->GetBoolean(prefs::kCloudExtensionRequestEnabled))
    return;
  const base::DictionaryValue* pending_requests =
      profile_->GetPrefs()->GetDictionary(prefs::kCloudExtensionRequestIds);

  // In case a corrupted profile prefs causing |pending_requests| to be null.
  if (!pending_requests)
    return;

  extensions::ExtensionManagement* extension_management =
      extensions::ExtensionManagementFactory::GetForBrowserContext(profile_);
  std::string webstore_update_url =
      extension_urls::GetDefaultWebstoreUpdateUrl().spec();

  int number_of_requests = 0;
  for (const auto& it : *pending_requests) {
    if (!ShouldUploadExtensionRequest(it.first, webstore_update_url,
                                      extension_management)) {
      continue;
    }

    // Use a hard limitation to prevent users adding too many requests. 1000
    // requests should use less than 50 kb report space.
    number_of_requests += 1;
    if (number_of_requests > kMaxNumberOfExtensionRequest)
      break;

    auto* request = report->add_extension_requests();
    request->set_id(it.first);
    base::Optional<base::Time> timestamp = ::util::ValueToTime(
        it.second->FindKey(extension_misc::kExtensionRequestTimestamp));
    if (timestamp)
      request->set_request_timestamp(timestamp->ToJavaTime());
  }
}

std::unique_ptr<policy::PolicyConversionsClient>
ProfileReportGeneratorDesktop::MakePolicyConversionsClient() {
  return std::make_unique<policy::ChromePolicyConversionsClient>(profile_);
}

policy::MachineLevelUserCloudPolicyManager*
ProfileReportGeneratorDesktop::GetCloudPolicyManager() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return nullptr;
#else
  return g_browser_process->browser_policy_connector()
      ->machine_level_user_cloud_policy_manager();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace enterprise_reporting
