// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_reporting_delegate_factory_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_report_factory_delegate_impl.h"
#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_report_scheduler_delegate_impl.h"
#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_report_uploader_impl.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_factory.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_scheduler.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_uploader.h"
#include "components/prefs/pref_service.h"

namespace enterprise_reporting {

// static
std::unique_ptr<SaasUsageReportingDelegateFactoryImpl>
SaasUsageReportingDelegateFactoryImpl::CreateForBrowser() {
  return base::WrapUnique(new SaasUsageReportingDelegateFactoryImpl(nullptr));
}

// static
std::unique_ptr<SaasUsageReportingDelegateFactoryImpl>
SaasUsageReportingDelegateFactoryImpl::CreateForProfile(Profile* profile) {
  return base::WrapUnique(new SaasUsageReportingDelegateFactoryImpl(profile));
}

SaasUsageReportingDelegateFactoryImpl::SaasUsageReportingDelegateFactoryImpl(
    Profile* profile)
    : profile_(profile) {}

PrefService* SaasUsageReportingDelegateFactoryImpl::GetPrefService() const {
  if (profile_) {
    return profile_->GetPrefs();
  }
  return g_browser_process->local_state();
}

std::unique_ptr<SaasUsageReportFactory::Delegate>
SaasUsageReportingDelegateFactoryImpl::GetSaasUsageReportFactoryDelegate()
    const {
  return std::make_unique<SaasUsageReportFactoryDelegateImpl>(profile_);
}

std::unique_ptr<SaasUsageReportUploader>
SaasUsageReportingDelegateFactoryImpl::GetSaasUsageReportUploader() const {
  if (profile_) {
    return std::make_unique<SaasUsageReportUploaderImpl>(profile_);
  }
  return std::make_unique<SaasUsageReportUploaderImpl>();
}

std::unique_ptr<SaasUsageReportScheduler::Delegate>
SaasUsageReportingDelegateFactoryImpl::GetSaasUsageReportSchedulerDelegate()
    const {
  if (profile_) {
    // profile-specific scheduler is not using delegate, because it is using
    // RealtimeReportingClient from profile and does not need to observe
    // profile-added/removed events to schedule/stop the report.
    return nullptr;
  }
  return std::make_unique<SaasUsageReportSchedulerDelegateImpl>();
}

}  // namespace enterprise_reporting
