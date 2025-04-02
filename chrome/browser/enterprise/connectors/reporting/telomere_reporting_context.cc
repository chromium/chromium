// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/telomere_reporting_context.h"

#include <ctime>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/connectors/core/reporting_service_settings.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

namespace enterprise_connectors {

namespace {

constexpr char kTelomerePollingIntervalFlag[] = "telomere-polling-interval";
constexpr char kTelomereLogsPathFlag[] = "telomere-logs-path";
constexpr int kDefaultTelomerePollingIntervalSeconds = 1;

policy::ChromeBrowserCloudManagementController* GetCBCMController() {
  return g_browser_process->browser_policy_connector()
      ->chrome_browser_cloud_management_controller();
}

std::vector<std::pair<std::string, time_t>> GetNewReports(
    time_t latest_creation_time) {
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  base::FilePath::StringType sysmon_logs_path =
#if BUILDFLAG(IS_WIN)
      base::UTF8ToWide(cmd->GetSwitchValueASCII(kTelomereLogsPathFlag));
#else
      cmd->GetSwitchValueASCII(kTelomereLogsPathFlag);
#endif
  base::FilePath sysmon_logs_path_file(sysmon_logs_path);
  return GetLogsFromPath(latest_creation_time, sysmon_logs_path_file);
}

void ReportTelomereLogs() {
  TelomereReportingContext* context = TelomereReportingContext::GetInstance();
  if (!context->HasActiveProfile()) {
    return;
  }
  RealtimeReportingClient* reporting_client = context->GetReportingClient();
  VLOG(2) << "enterprise.telomere_reporting: (reporting client != nullptr): "
          << (reporting_client != nullptr) << ", kTelomereReporting: "
          << base::FeatureList::IsEnabled(kTelomereReporting);
  if (!reporting_client) {
    g_browser_process->local_state()->ClearPref(
        kLatestTelomereReportCreationTime);
    return;
  }
  VLOG(2) << "enterprise.telomere_reporting: checking for unreported logs";
  time_t latest_creation_time =
      GetLatestTelomereReportTime(g_browser_process->local_state());
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&GetNewReports, latest_creation_time),
      base::BindOnce(&UploadToReportingServer,
                     reporting_client->AsWeakPtrImpl(),
                     g_browser_process->local_state()));
}

}  // namespace

base::TimeDelta GetTelomerePollingInterval() {
  base::TimeDelta result =
      base::Seconds(kDefaultTelomerePollingIntervalSeconds);
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(kTelomerePollingIntervalFlag)) {
    int polling_interval_seconds = 0;
    if (base::StringToInt(
            cmd->GetSwitchValueASCII(kTelomerePollingIntervalFlag),
            &polling_interval_seconds) &&
        polling_interval_seconds > 0) {
      result = base::Seconds(polling_interval_seconds);
    }
  }
  VLOG(2) << "enterprise.telomere_reporting: crashpad polling interval set to "
          << result;
  return result;
}

std::vector<std::pair<std::string, time_t>> GetLogsFromPath(
    time_t latest_creation_time,
    base::FilePath sysmon_logs_path) {
  base::FileEnumerator input_files(
      sysmon_logs_path, /*recursive=*/true, base::FileEnumerator::FILES,
      FILE_PATH_LITERAL("*.json"),
      base::FileEnumerator::FolderSearchPolicy::ALL);
  std::vector<std::pair<std::string, time_t>> reports;
  input_files.ForEach([&reports,
                       &latest_creation_time](const base::FilePath& file_path) {
    base::File::Info info;
    base::GetFileInfo(file_path, &info);
    if (info.creation_time.ToTimeT() <= latest_creation_time) {
      VLOG(2) << "enterprise.telomere_reporting: skipping file: "
              << file_path.value() << "modified at "
              << info.creation_time.ToTimeT() << " < " << latest_creation_time;
      return;
    }

    std::string upload_payload;
    base::ReadFileToString(file_path, &upload_payload);
    reports.emplace_back(upload_payload, info.creation_time.ToTimeT());
  });

  VLOG(2) << "enterprise.telomere_reporting: " << reports.size()
          << " new files to upload";

  return reports;
}

time_t GetLatestTelomereReportTime(PrefService* local_state) {
  time_t timestamp = local_state->GetInt64(kLatestTelomereReportCreationTime);
  return timestamp;
}

void SetLatestTelomereReportTime(PrefService* local_state, time_t timestamp) {
  VLOG(2) << "enterprise.telomere_reporting: updating latest telomere "
             "report time to "
          << timestamp;
  local_state->SetInt64(kLatestTelomereReportCreationTime, timestamp);
}

void UploadToReportingServer(
    base::WeakPtr<RealtimeReportingClient> reporting_client,
    PrefService* local_state,
    std::vector<std::pair<std::string, time_t>> reports) {
  if (reports.empty() || !reporting_client) {
    return;
  }
  std::optional<ReportingSettings> settings =
      reporting_client->GetReportingSettings();
  for (const auto& report : reports) {
    VLOG(2) << "enterprise.telomere_reporting: reporting: "
            << report.first.substr(0, 50);
    ::chrome::cros::reporting::proto::Event event;
    event.set_prototype_raw_event(report.first);
    reporting_client->ReportEvent(std::move(event),
                                  std::move(settings.value()));

    if (report.second > GetLatestTelomereReportTime(local_state)) {
      SetLatestTelomereReportTime(local_state, report.second);
    }
  }
}

TelomereReportingContext::TelomereReportingContext() {
  GetCBCMController()->AddObserver(this);
}

void TelomereReportingContext::AddProfile(TelomereEventRouter* router,
                                          Profile* profile) {
  if (!profile->IsRegularProfile()) {
    return;
  }
  active_profiles_[router] = profile;
}

TelomereReportingContext* TelomereReportingContext::GetInstance() {
  return base::Singleton<TelomereReportingContext>::get();
}

RealtimeReportingClient* TelomereReportingContext::GetReportingClient() const {
  for (auto& it : active_profiles_) {
    Profile* profile = it.second;
    RealtimeReportingClient* reporting_client =
        RealtimeReportingClientFactory::GetForProfile(profile);
    if (!reporting_client) {
      continue;
    }
    std::optional<ReportingSettings> settings =
        reporting_client->GetReportingSettings();
    if (settings.has_value() && !settings->per_profile) {
      return reporting_client;
    }
  }
  return nullptr;
}

bool TelomereReportingContext::HasActiveProfile() const {
  return active_profiles_.size() != 0;
}

void TelomereReportingContext::OnBrowserUnenrolled(bool succeeded) {
  if (succeeded && repeating_telomere_log_.IsRunning()) {
    VLOG(1) << "enterprise.telomere_reporting: browser unenrolled";
    repeating_telomere_log_.Stop();
  }
}

void TelomereReportingContext::OnCloudReportingLaunched(
    enterprise_reporting::ReportScheduler* report_scheduler) {
  VLOG(1) << "enterprise.telomere_reporting: telomere reporting initializing";
  // An initial call to ReportTelomereLogs() is required because the first call
  // in the repeating callback happens after the delay.
  ReportTelomereLogs();
  if (!repeating_telomere_log_.IsRunning()) {
    repeating_telomere_log_.Start(FROM_HERE, GetTelomerePollingInterval(),
                                  base::BindRepeating(&ReportTelomereLogs));
  }
}

void TelomereReportingContext::OnShutdown() {
  GetCBCMController()->RemoveObserver(this);
}

void TelomereReportingContext::RemoveProfile(TelomereEventRouter* router) {
  auto it = active_profiles_.find(router);
  if (it != active_profiles_.end()) {
    active_profiles_.erase(it);
  }
}


TelomereReportingContext::~TelomereReportingContext() = default;

}  // namespace enterprise_connectors
