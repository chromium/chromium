// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_reporting_delegate_factory_desktop.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_report_factory_desktop.h"
#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_report_scheduler_delegate_desktop.h"
#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_report_uploader_desktop.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_factory.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_scheduler.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_uploader.h"
#include "components/prefs/pref_service.h"

namespace enterprise_reporting {

// static
std::unique_ptr<SaasUsageReportingDelegateFactoryDesktop>
SaasUsageReportingDelegateFactoryDesktop::CreateForBrowser() {
  return base::WrapUnique(
      new SaasUsageReportingDelegateFactoryDesktop(nullptr));
}

// static
std::unique_ptr<SaasUsageReportingDelegateFactoryDesktop>
SaasUsageReportingDelegateFactoryDesktop::CreateForProfile(Profile* profile) {
  return base::WrapUnique(
      new SaasUsageReportingDelegateFactoryDesktop(profile));
}

SaasUsageReportingDelegateFactoryDesktop::
    SaasUsageReportingDelegateFactoryDesktop(Profile* profile)
    : profile_(profile) {}

PrefService* SaasUsageReportingDelegateFactoryDesktop::GetPrefService() const {
  if (profile_) {
    return profile_->GetPrefs();
  }
  return g_browser_process->local_state();
}

std::unique_ptr<SaasUsageReportFactory::Delegate>
SaasUsageReportingDelegateFactoryDesktop::GetSaasUsageReportFactoryDelegate()
    const {
  return std::make_unique<SaasUsageReportFactoryDesktop>(profile_);
}

std::unique_ptr<SaasUsageReportUploader>
SaasUsageReportingDelegateFactoryDesktop::GetSaasUsageReportUploader() const {
  if (profile_) {
    return std::make_unique<SaasUsageProfileReportUploaderDesktop>(profile_);
  }
  return std::make_unique<SaasUsageBrowserReportUploaderDesktop>();
}

std::unique_ptr<SaasUsageReportScheduler::Delegate>
SaasUsageReportingDelegateFactoryDesktop::GetSaasUsageReportSchedulerDelegate()
    const {
  if (profile_) {
    // profile-specific scheduler is not using delegate, because it is using
    // RealtimeReportingClient from profile and does not need to observe
    // profile-added/removed events to schedule/stop the report.
    return nullptr;
  }
  return std::make_unique<SaasUsageReportSchedulerDelegateDesktop>();
}

}  // namespace enterprise_reporting
