// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/cloud_profile_reporting_service.h"

#include <utility>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/browser/reporting/chrome_profile_request_generator.h"
#include "components/enterprise/browser/reporting/report_scheduler.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_android.h"
#else
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_desktop.h"
#endif

namespace enterprise_reporting {

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)
BASE_FEATURE(kAlwaysUploadExtensionInfo,
             "AlwaysUploadExtensionInfo",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

std::string GetProfileName(Profile* profile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // profile manager may not be available in test.
  if (!profile_manager)
    return std::string();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(profile->GetPath());
  if (!entry)
    return std::string();
  return base::UTF16ToUTF8(entry->GetName());
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
bool CanUploadExtensionInfo(Profile* profile) {
  if (base::FeatureList::IsEnabled(kAlwaysUploadExtensionInfo)) {
    return true;
  }

  if (enterprise_util::IsProfileAffiliated(profile)) {
    return true;
  }

  const policy::PolicyMap& policies =
      g_browser_process->policy_service()->GetPolicies(
          policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
  return !policies.GetValue(policy::key::kExtensionSettings,
                            base::Value::Type::DICT) &&
         !policies.GetValue(policy::key::kExtensionInstallForcelist,
                            base::Value::Type::LIST) &&
         !policies.GetValue(policy::key::kExtensionInstallSources,
                            base::Value::Type::LIST);
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace

CloudProfileReportingService::CloudProfileReportingService(Profile* profile)
    : profile_(profile) {
  Init();
}

CloudProfileReportingService::~CloudProfileReportingService() = default;

void CloudProfileReportingService::CreateReportScheduler() {
  std::string profile_id = "";
  if (enterprise::ProfileIdServiceFactory::GetForProfile(profile_)) {
    profile_id = enterprise::ProfileIdServiceFactory::GetForProfile(profile_)
                     ->GetProfileId()
                     .value_or("");
  }
  cloud_policy_client_ = std::make_unique<policy::CloudPolicyClient>(
      profile_id,
      g_browser_process->browser_policy_connector()
          ->device_management_service(),
      profile_->GetURLLoaderFactory(),
      policy::CloudPolicyClient::DeviceDMTokenCallback());

#if BUILDFLAG(IS_ANDROID)
  ReportingDelegateFactoryAndroid delegate_factory;
#else
  ReportingDelegateFactoryDesktop delegate_factory;
#endif  // !BUILDFLAG(IS_ANDROID)
  ReportScheduler::CreateParams params;
  params.client = cloud_policy_client_.get();
  params.delegate = delegate_factory.GetReportSchedulerDelegate(profile_);
  params.profile_request_generator =
      std::make_unique<ChromeProfileRequestGenerator>(
          profile_->GetPath(), GetProfileName(profile_), &delegate_factory);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  params.profile_request_generator->ToggleExtensionReport(
      base::BindRepeating(&CanUploadExtensionInfo, profile_));
#endif
  report_scheduler_ = std::make_unique<ReportScheduler>(std::move(params));
}

void CloudProfileReportingService::OnCoreConnected(
    policy::CloudPolicyCore* core) {
  CreateReportScheduler();
  core_observation_.Reset();
}
void CloudProfileReportingService::OnRefreshSchedulerStarted(
    policy::CloudPolicyCore* core) {}
void CloudProfileReportingService::OnCoreDisconnecting(
    policy::CloudPolicyCore* core) {}

void CloudProfileReportingService::InitForTesting() {
  Init();
}

void CloudProfileReportingService::Init() {
  // Only create ReportScheduler when profile is managed.
  if (policy::ManagementServiceFactory::GetForProfile(profile_)
          ->HasManagementAuthority(
              policy::EnterpriseManagementAuthority::CLOUD)) {
    content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
        ->PostTask(
            FROM_HERE,
            base::BindOnce(&CloudProfileReportingService::CreateReportScheduler,
                           weak_factory_.GetWeakPtr()));
    return;
  }
  // Otherwise, wait for profile being signed in.
  if (profile_->GetCloudPolicyManager()) {
    core_observation_.Observe(profile_->GetCloudPolicyManager()->core());
  }
}

}  // namespace enterprise_reporting
