// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/child_status_reporting_service.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/child_accounts/event_based_status_reporting_service_factory.h"
#include "chrome/browser/chromeos/child_accounts/usage_time_limit_processor.h"
#include "chrome/browser/chromeos/policy/status_collector/child_status_collector.h"
#include "chrome/browser/chromeos/policy/status_uploader.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/system/statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

namespace {

// Default frequency for uploading status reports.
constexpr base::TimeDelta kStatusUploadFrequency =
    base::TimeDelta::FromMinutes(10);

}  // namespace

ChildStatusReportingService::ChildStatusReportingService(
    content::BrowserContext* context)
    : context_(context) {
  Profile* profile = Profile::FromBrowserContext(context_);
  // If child user is registered with DMServer and has User Policy applied, it
  // should upload device status to the server.
  user_cloud_policy_manager_ = profile->GetUserCloudPolicyManagerChromeOS();
  if (!user_cloud_policy_manager_) {
    LOG(WARNING) << "Child user is not managed by User Policy - status reports "
                    "cannot be uploaded to the server. ";
    return;
  }

  PrefService* pref_service = profile->GetPrefs();
  DCHECK(pref_service->GetInitializationStatus() !=
         PrefService::INITIALIZATION_STATUS_WAITING);

  // We immediately upload a status report after Time Limits policy changes.
  // Make sure we listen for those events.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      prefs::kUsageTimeLimit,
      base::BindRepeating(
          &ChildStatusReportingService::OnTimeLimitsPolicyChanged,
          base::Unretained(this)));

  CreateStatusUploaderIfNeeded(user_cloud_policy_manager_->core()->client());

  if (base::FeatureList::IsEnabled(features::kEventBasedStatusReporting)) {
    EventBasedStatusReportingServiceFactory::GetForBrowserContext(context);
  }
}

ChildStatusReportingService::~ChildStatusReportingService() = default;

void ChildStatusReportingService::CreateStatusUploaderIfNeeded(
    policy::CloudPolicyClient* client) {
  const base::DictionaryValue* time_limit =
      pref_change_registrar_->prefs()->GetDictionary(prefs::kUsageTimeLimit);
  const base::TimeDelta new_day_reset_time =
      usage_time_limit::GetTimeUsageLimitResetTime(*time_limit);

  // Day reset time did not change, there is no need to re-create StatusUploader
  // if it already exists.
  if (status_uploader_ && (day_reset_time_ == new_day_reset_time))
    return;

  VLOG(1) << "Creating status uploader for child status reporting.";
  day_reset_time_ = new_day_reset_time;
  status_uploader_ = std::make_unique<policy::StatusUploader>(
      client,
      std::make_unique<policy::ChildStatusCollector>(
          pref_change_registrar_->prefs(),
          system::StatisticsProvider::GetInstance(),
          policy::ChildStatusCollector::AndroidStatusFetcher(),
          day_reset_time_),
      base::ThreadTaskRunnerHandle::Get(), kStatusUploadFrequency);
}

bool ChildStatusReportingService::RequestImmediateStatusReport() {
  return status_uploader_->ScheduleNextStatusUploadImmediately();
}

base::TimeDelta ChildStatusReportingService::GetChildScreenTime() const {
  // Notice that this cast works because we know that |status_uploader_| has a
  // ChildStatusCollector (see above).
  return static_cast<policy::ChildStatusCollector*>(
             status_uploader_->status_collector())
      ->GetActiveChildScreenTime();
}

void ChildStatusReportingService::OnTimeLimitsPolicyChanged() {
  CreateStatusUploaderIfNeeded(user_cloud_policy_manager_->core()->client());
}

}  // namespace chromeos
