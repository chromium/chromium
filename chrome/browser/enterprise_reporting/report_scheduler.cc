// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/report_scheduler.h"

#include <utility>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/syslog_logging.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise_reporting/prefs.h"
#include "chrome/browser/enterprise_reporting/request_timer.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/browser_dm_token_storage.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {
const int kDefaultUploadIntervalHours =
    24;  // Default upload interval is 24 hours.
const int kMaximumRetry = 10;  // Retry 10 times takes about 15 to 19 hours.
const int kMaximumTrackedProfiles = 21;

// Reads DM token and client id. Returns true if boths are non empty.
bool GetDMTokenAndDeviceId(std::string* dm_token, std::string* client_id) {
  DCHECK(dm_token && client_id);
  auto browser_dm_token =
      policy::BrowserDMTokenStorage::Get()->RetrieveBrowserDMToken();
  *client_id = policy::BrowserDMTokenStorage::Get()->RetrieveClientId();

  if (!browser_dm_token.is_valid() || client_id->empty()) {
    VLOG(1)
        << "Enterprise reporting is disabled because device is not enrolled.";
    return false;
  }
  *dm_token = browser_dm_token.value();
  return true;
}

// Returns true if cloud reporting is enabled.
bool IsReportingEnabled() {
  return g_browser_process->local_state()->GetBoolean(
      prefs::kCloudReportingEnabled);
}

}  // namespace

ReportScheduler::ReportScheduler(
    std::unique_ptr<policy::CloudPolicyClient> client,
    std::unique_ptr<RequestTimer> request_timer,
    std::unique_ptr<ReportGenerator> report_generator)
    : cloud_policy_client_(std::move(client)),
      request_timer_(std::move(request_timer)),
      report_generator_(std::move(report_generator)) {
  RegisterPrefObserver();
}

ReportScheduler::~ReportScheduler() {
  if (IsReportingEnabled() && stale_profiles_) {
    base::UmaHistogramExactLinear("Enterprise.CloudReportingStaleProfileCount",
                                  stale_profiles_->size(),
                                  kMaximumTrackedProfiles);
  }
}

void ReportScheduler::SetReportUploaderForTesting(
    std::unique_ptr<ReportUploader> uploader) {
  report_uploader_ = std::move(uploader);
}

void ReportScheduler::OnDMTokenUpdated() {
  OnReportEnabledPrefChanged();
}

void ReportScheduler::RegisterPrefObserver() {
  pref_change_registrar_.Init(g_browser_process->local_state());
  pref_change_registrar_.Add(
      prefs::kCloudReportingEnabled,
      base::BindRepeating(&ReportScheduler::OnReportEnabledPrefChanged,
                          base::Unretained(this)));
  // Trigger first pref check during launch process.
  OnReportEnabledPrefChanged();
}

void ReportScheduler::OnReportEnabledPrefChanged() {
  std::string dm_token;
  std::string client_id;
  if (!IsReportingEnabled() || !GetDMTokenAndDeviceId(&dm_token, &client_id)) {
    if (request_timer_)
      request_timer_->Stop();
    return;
  }

  if (!cloud_policy_client_->is_registered()) {
    cloud_policy_client_->SetupRegistration(dm_token, client_id,
                                            std::vector<std::string>());
  }

  Start();
}

void ReportScheduler::Start() {
  base::TimeDelta upload_interval =
      base::TimeDelta::FromHours(kDefaultUploadIntervalHours);
  base::TimeDelta first_request_delay =
      upload_interval -
      (base::Time::Now() -
       g_browser_process->local_state()->GetTime(kLastUploadTimestamp));
  // The first report delay is based on the |lastUploadTimestamp| in the
  // |local_state|, after that, it's 24 hours for each succeeded upload.
  VLOG(1) << "Schedule the first report in about "
          << first_request_delay.InHours() << " hour(s) and "
          << first_request_delay.InMinutes() % 60 << " minute(s).";
  request_timer_->Start(
      FROM_HERE, first_request_delay, upload_interval,
      base::BindRepeating(&ReportScheduler::GenerateAndUploadReport,
                          base::Unretained(this)));
}

void ReportScheduler::GenerateAndUploadReport() {
  VLOG(1) << "Generating enterprise report.";
  report_generator_->Generate(base::BindOnce(
      &ReportScheduler::OnReportGenerated, base::Unretained(this)));
}

void ReportScheduler::OnReportGenerated(ReportGenerator::Requests requests) {
  if (requests.empty()) {
    SYSLOG(ERROR)
        << "No cloud report can be generated. Likely the report is too large.";
    // We can't generate any report, stop the reporting.
    return;
  }
  VLOG(1) << "Uploading enterprise report.";
  if (!report_uploader_) {
    report_uploader_ = std::make_unique<ReportUploader>(
        cloud_policy_client_.get(), kMaximumRetry);
  }
  report_uploader_->SetRequestAndUpload(
      std::move(requests), base::BindOnce(&ReportScheduler::OnReportUploaded,
                                          base::Unretained(this)));
}

void ReportScheduler::OnReportUploaded(ReportUploader::ReportStatus status) {
  VLOG(1) << "The enterprise report upload result " << status << ".";
  switch (status) {
    case ReportUploader::kSuccess:
      // Schedule the next report for success. Reset uploader to reset failure
      // count.
      report_uploader_.reset();
      if (IsReportingEnabled())
        TrackStaleProfiles();
      FALLTHROUGH;
    case ReportUploader::kTransientError:
      // Stop retrying and schedule the next report to avoid stale report.
      // Failure count is not reset so retry delay remains.
      g_browser_process->local_state()->SetTime(kLastUploadTimestamp,
                                                base::Time::Now());
      if (IsReportingEnabled())
        request_timer_->Reset();
      break;
    case ReportUploader::kPersistentError:
      // No future upload until Chrome relaunch or pref change event.
      break;
  }
}

void ReportScheduler::TrackStaleProfiles() {
  if (!stale_profiles_) {
    // If we haven't, start the tracking.
    stale_profiles_ = std::make_unique<base::flat_set<base::FilePath>>();
    g_browser_process->profile_manager()->AddObserver(this);
  } else {
    stale_profiles_->clear();
  }
}

void ReportScheduler::OnProfileAdded(Profile* profile) {
  if (profile->IsSystemProfile() || profile->IsGuestSession() ||
      profile->IsIncognitoProfile()) {
    return;
  }
  DCHECK(stale_profiles_);
  stale_profiles_->insert(profile->GetPath());
}

void ReportScheduler::OnProfileMarkedForPermanentDeletion(Profile* profile) {
  DCHECK(stale_profiles_);
  stale_profiles_->erase(profile->GetPath());
}

}  // namespace enterprise_reporting
