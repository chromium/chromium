// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/profile_report_generator.h"

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise_reporting/extension_info.h"
#include "chrome/browser/enterprise_reporting/policy_info.h"
#include "chrome/browser/policy/policy_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace enterprise_reporting {

ProfileReportGenerator::ProfileReportGenerator() {}

ProfileReportGenerator::~ProfileReportGenerator() = default;

void ProfileReportGenerator::set_extensions_enabled(bool enabled) {
  extensions_enabled_ = enabled;
}

void ProfileReportGenerator::set_policies_enabled(bool enabled) {
  policies_enabled_ = enabled;
}

void ProfileReportGenerator::set_extension_request_enabled(bool enabled) {
  extension_request_enabled_ = enabled;
}

std::unique_ptr<em::ChromeUserProfileInfo>
ProfileReportGenerator::MaybeGenerate(const base::FilePath& path,
                                      const std::string& name) {
  profile_ = g_browser_process->profile_manager()->GetProfileByPath(path);

  if (!profile_) {
    return nullptr;
  }

  report_ = std::make_unique<em::ChromeUserProfileInfo>();
  report_->set_id(path.AsUTF8Unsafe());
  report_->set_name(name);
  report_->set_is_full_report(true);

  GetSigninUserInfo();
  GetExtensionInfo();
  GetExtensionRequest();

  if (policies_enabled_) {
    // TODO(crbug.com/983151): Upload policy error as their IDs.
    policies_ = policy::DictionaryPolicyConversions()
                    .WithBrowserContext(profile_)
                    .EnableConvertTypes(false)
                    .EnablePrettyPrint(false)
                    .ToValue();
    GetChromePolicyInfo();
    GetExtensionPolicyInfo();
    GetPolicyFetchTimestampInfo();
  }

  return std::move(report_);
}

void ProfileReportGenerator::GetSigninUserInfo() {
  auto account_info =
      IdentityManagerFactory::GetForProfile(profile_)->GetPrimaryAccountInfo();
  if (account_info.IsEmpty())
    return;
  auto* signed_in_user_info = report_->mutable_chrome_signed_in_user();
  signed_in_user_info->set_email(account_info.email);
  signed_in_user_info->set_obfudscated_gaia_id(account_info.gaia);
}

void ProfileReportGenerator::GetExtensionInfo() {
  if (!extensions_enabled_)
    return;
  AppendExtensionInfoIntoProfileReport(profile_, report_.get());
}

void ProfileReportGenerator::GetChromePolicyInfo() {
  AppendChromePolicyInfoIntoProfileReport(policies_, report_.get());
}

void ProfileReportGenerator::GetExtensionPolicyInfo() {
  AppendExtensionPolicyInfoIntoProfileReport(policies_, report_.get());
}

void ProfileReportGenerator::GetPolicyFetchTimestampInfo() {
  AppendMachineLevelUserCloudPolicyFetchTimestamp(report_.get());
}

void ProfileReportGenerator::GetExtensionRequest() {
  if (!extension_request_enabled_)
    return;
  const auto pending_list = profile_->GetPrefs()
                                ->GetList(prefs::kCloudExtensionRequestIds)
                                ->GetList();
  for (const base::Value& pending_request : pending_list)
    report_->add_extension_requests()->set_id(pending_request.GetString());
}

}  // namespace enterprise_reporting
