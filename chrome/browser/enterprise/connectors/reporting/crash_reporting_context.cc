// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/crash_reporting_context.h"

#include "base/command_line.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "components/crash/core/app/crashpad.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/connectors/core/reporting_service_settings.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

namespace enterprise_connectors {

#if !BUILDFLAG(IS_CHROMEOS)

namespace {

constexpr char kKeyChannel[] = "channel";
constexpr char kKeyVersion[] = "version";
constexpr char kKeyReportId[] = "reportId";
constexpr char kKeyPlatform[] = "platform";
constexpr char kCrashpadPollingIntervalFlag[] = "crashpad-polling-interval";
constexpr int kDefaultCrashpadPollingIntervalSeconds = 3600;

policy::ChromeBrowserCloudManagementController* GetCBCMController() {
  return g_browser_process->browser_policy_connector()
      ->chrome_browser_cloud_management_controller();
}

// Copy new reports (i.e. reports that have not been sent to the
// reporting server) from `reports_to_be_copied` to `reports`
// based on the `latest_creation_time`.
void CopyNewReports(
    const std::vector<crashpad::CrashReportDatabase::Report>&
        reports_to_be_copied,
    int64_t latest_creation_time,
    std::vector<crashpad::CrashReportDatabase::Report>& reports) {
  for (const crashpad::CrashReportDatabase::Report& report :
       reports_to_be_copied) {
    if (report.creation_time <= latest_creation_time) {
      continue;
    }
    reports.push_back(report);
  }
}

std::vector<crashpad::CrashReportDatabase::Report> GetNewReports(
    time_t latest_creation_time) {
  auto crashpad_path = crash_reporter::GetCrashpadDatabasePath();
  if (!crashpad_path) {
    VLOG(1) << "enterprise.crash_reporting: no valid crashpad path";
    return {};
  }
  std::unique_ptr<crashpad::CrashReportDatabase> database =
      crashpad::CrashReportDatabase::InitializeWithoutCreating(*crashpad_path);
  if (!database) {
    VLOG(1) << "enterprise.crash_reporting: failed to fetch crashpad db";
    return {};
  }
  return GetNewReportsFromDatabase(latest_creation_time, database.get());
}

void ReportCrashes() {
  CrashReportingContext* context = CrashReportingContext::GetInstance();
  if (!context->HasActiveProfile()) {
    return;
  }
  RealtimeReportingClient* reporting_client =
      context->GetCrashReportingClient();
  VLOG(1) << "enterprise.crash_reporting: crash reporting enabled: "
          << (reporting_client != nullptr);
  if (!reporting_client) {
    g_browser_process->local_state()->ClearPref(kLatestCrashReportCreationTime);
    return;
  }
  VLOG(1) << "enterprise.crash_reporting: checking for unreported crashes";
  time_t latest_creation_time =
      GetLatestCrashReportTime(g_browser_process->local_state());
  if (latest_creation_time == 0) {
    latest_creation_time = base::Time::Now().ToTimeT();
    SetLatestCrashReportTime(g_browser_process->local_state(),
                             latest_creation_time);
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&GetNewReports, latest_creation_time),
      base::BindOnce(&UploadToReportingServer, reporting_client->GetWeakPtr(),
                     g_browser_process->local_state()));
}

}  // namespace

base::TimeDelta GetCrashpadPollingInterval() {
  base::TimeDelta result =
      base::Seconds(kDefaultCrashpadPollingIntervalSeconds);
  if (chrome::GetChannel() != version_info::Channel::STABLE) {
    base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
    if (cmd->HasSwitch(kCrashpadPollingIntervalFlag)) {
      int crashpad_polling_interval_seconds = 0;
      if (base::StringToInt(
              cmd->GetSwitchValueASCII(kCrashpadPollingIntervalFlag),
              &crashpad_polling_interval_seconds) &&
          crashpad_polling_interval_seconds > 0 &&
          crashpad_polling_interval_seconds <
              kDefaultCrashpadPollingIntervalSeconds) {
        result = base::Seconds(crashpad_polling_interval_seconds);
      }
    }
  }
  VLOG(1) << "enterprise.crash_reporting: crashpad polling interval set to "
          << result;
  return result;
}

std::vector<crashpad::CrashReportDatabase::Report> GetNewReportsFromDatabase(
    time_t latest_creation_time,
    crashpad::CrashReportDatabase* database) {
  std::vector<crashpad::CrashReportDatabase::Report> reports;
  std::vector<crashpad::CrashReportDatabase::Report> from_db;
  crashpad::CrashReportDatabase::OperationStatus status =
      database->GetPendingReports(&from_db);
  if (status == crashpad::CrashReportDatabase::kNoError) {
    CopyNewReports(from_db, latest_creation_time, reports);
    from_db.clear();
  }
  status = database->GetCompletedReports(&from_db);
  if (status == crashpad::CrashReportDatabase::kNoError) {
    CopyNewReports(from_db, latest_creation_time, reports);
  }
  return reports;
}

time_t GetLatestCrashReportTime(PrefService* local_state) {
  time_t timestamp = local_state->GetInt64(kLatestCrashReportCreationTime);
  VLOG(1) << "enterprise.crash_reporting: latest crash report time: "
          << base::Time::FromTimeT(timestamp);
  return timestamp;
}

void SetLatestCrashReportTime(PrefService* local_state, time_t timestamp) {
  local_state->SetInt64(kLatestCrashReportCreationTime, timestamp);
}

void UploadToReportingServer(
    base::WeakPtr<RealtimeReportingClient> reporting_client,
    PrefService* local_state,
    std::vector<crashpad::CrashReportDatabase::Report> reports) {
  VLOG(1) << "enterprise.crash_reporting: " << reports.size()
          << " crashes to report";
  if (reports.empty() || !reporting_client) {
    return;
  }
  std::optional<ReportingSettings> settings =
      reporting_client->GetReportingSettings();
  const std::string version(version_info::GetVersionNumber());
  const std::string channel(
      version_info::GetChannelString(chrome::GetChannel()));
  const std::string platform(version_info::GetOSType());

  int64_t latest_creation_time = -1;

  for (const auto& report : reports) {
    base::Value::Dict event;
    event.Set(kKeyChannel, channel);
    event.Set(kKeyVersion, version);
    event.Set(kKeyReportId, report.id);
    event.Set(kKeyPlatform, platform);
    reporting_client->ReportPastEvent(
        kBrowserCrashEvent, settings.value(), std::move(event),
        base::Time::FromTimeT(report.creation_time));
    if (report.creation_time > latest_creation_time) {
      latest_creation_time = report.creation_time;
    }
  }
  SetLatestCrashReportTime(local_state, latest_creation_time);
}

CrashReportingContext::CrashReportingContext() {
  GetCBCMController()->AddObserver(this);
}

void CrashReportingContext::AddProfile(BrowserCrashEventRouter* router,
                                       Profile* profile) {
  if (!profile->IsRegularProfile()) {
    return;
  }
  active_profiles_[router] = profile;
}

CrashReportingContext* CrashReportingContext::GetInstance() {
  return base::Singleton<CrashReportingContext>::get();
}

RealtimeReportingClient* CrashReportingContext::GetCrashReportingClient()
    const {
  for (auto& it : active_profiles_) {
    Profile* profile = it.second;
    RealtimeReportingClient* reporting_client =
        RealtimeReportingClientFactory::GetForProfile(profile);
    if (!reporting_client) {
      continue;
    }
    std::optional<ReportingSettings> settings =
        reporting_client->GetReportingSettings();
    if (settings.has_value() &&
        settings->enabled_event_names.count(kBrowserCrashEvent) != 0 &&
        !settings->per_profile) {
      return reporting_client;
    }
  }
  return nullptr;
}

bool CrashReportingContext::HasActiveProfile() const {
  return active_profiles_.size() != 0;
}

void CrashReportingContext::OnBrowserUnenrolled(bool succeeded) {
  if (succeeded && repeating_crash_report_.IsRunning()) {
    VLOG(1) << "enterprise.crash_reporting: browser unenrolled";
    repeating_crash_report_.Stop();
  }
}

void CrashReportingContext::OnCloudReportingLaunched(
    enterprise_reporting::ReportScheduler* report_scheduler) {
  VLOG(1) << "enterprise.crash_reporting: crash event reporting initializing";
  // An initial call to ReportCrashes() is required because the first call
  // in the repeating callback happens after the delay.
  ReportCrashes();
  if (!repeating_crash_report_.IsRunning()) {
    repeating_crash_report_.Start(FROM_HERE, GetCrashpadPollingInterval(),
                                  base::BindRepeating(&ReportCrashes));
  }
}

void CrashReportingContext::OnShutdown() {
  GetCBCMController()->RemoveObserver(this);
}

void CrashReportingContext::RemoveProfile(BrowserCrashEventRouter* router) {
  auto it = active_profiles_.find(router);
  if (it != active_profiles_.end()) {
    active_profiles_.erase(it);
  }
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

CrashReportingContext::~CrashReportingContext() = default;

}  // namespace enterprise_connectors
