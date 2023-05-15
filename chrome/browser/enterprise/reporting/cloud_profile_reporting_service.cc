// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/cloud_profile_reporting_service.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/enterprise/browser/reporting/chrome_profile_request_generator.h"
#include "components/enterprise/browser/reporting/report_scheduler.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_android.h"
#else
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_desktop.h"
#endif

namespace enterprise_reporting {

namespace {

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

}  // namespace

CloudProfileReportingService::CloudProfileReportingService(
    Profile* profile,
    policy::DeviceManagementService* device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory) {
  cloud_policy_client_ = std::make_unique<policy::CloudPolicyClient>(
      device_management_service, system_url_loader_factory,
      policy::CloudPolicyClient::DeviceDMTokenCallback());

#if BUILDFLAG(IS_ANDROID)
  ReportingDelegateFactoryAndroid delegate_factory;
#else
  ReportingDelegateFactoryDesktop delegate_factory;
#endif  // !BUILDFLAG(IS_ANDROID)
  ReportScheduler::CreateParams params;
  params.client = cloud_policy_client_.get();
  params.delegate = delegate_factory.GetReportSchedulerDelegate(profile);
  params.profile_request_generator =
      std::make_unique<ChromeProfileRequestGenerator>(
          profile->GetPath(), GetProfileName(profile), &delegate_factory);
  report_scheduler_ = std::make_unique<ReportScheduler>(std::move(params));
}

CloudProfileReportingService::~CloudProfileReportingService() = default;

}  // namespace enterprise_reporting
