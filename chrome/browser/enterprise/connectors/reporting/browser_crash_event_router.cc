// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/browser_crash_event_router.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/reporting/reporting_service_settings.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

#if !BUILDFLAG(IS_FUCHSIA)
#include "components/crash/core/app/crashpad.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#endif  // !BUILDFLAG(IS_FUCHSIA)

namespace enterprise_connectors {

#if !BUILDFLAG(IS_FUCHSIA)
namespace {
// key names used when building the dictionary to pass to the real-time
// reporting API
constexpr char kKeyChannel[] = "channel";
constexpr char kKeyVersion[] = "version";
constexpr char kKeyReportId[] = "reportId";
constexpr char kKeyPlatform[] = "platform";
constexpr char kKeyProfileUserName[] = "profileUserName";

constexpr char kCrashpadPollingIntervalFlag[] = "crashpad-polling-interval";
constexpr int kDefaultCrashpadPollingIntervalSeconds = 3600;

void SetLatestCrashReportTime(time_t timestamp) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetInt64(enterprise_connectors::kLatestCrashReportCreationTime,
                        timestamp);
}

time_t GetLatestCrashReportTime() {
  PrefService* local_state = g_browser_process->local_state();
  time_t timestamp = local_state->GetInt64(
      enterprise_connectors::kLatestCrashReportCreationTime);
  VLOG(1) << "enterprise.crash_reporting: latest crash report time: "
          << base::Time::FromTimeT(timestamp);
  return timestamp;
}

// Get polling interval for crashpad database. This is factored into a
// function to allow for a dev-only command-line option for ease of
// manual testing
base::TimeDelta GetCrashpadPollingInterval() {
  base::TimeDelta result =
      base::Seconds(kDefaultCrashpadPollingIntervalSeconds);
  if (g_browser_process && g_browser_process->browser_policy_connector()
                               ->IsCommandLineSwitchSupported()) {
    base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
    if (cmd->HasSwitch(kCrashpadPollingIntervalFlag)) {
      int crashpad_polling_interval_seconds = 0;
      if (base::StringToInt(
              cmd->GetSwitchValueASCII(kCrashpadPollingIntervalFlag),
              &crashpad_polling_interval_seconds) &&
          crashpad_polling_interval_seconds > 0) {
        result = base::Seconds(crashpad_polling_interval_seconds);
      }
    }
  }
  VLOG(1) << "enterprise.crash_reporting: crashpad polling interval set to "
          << result;
  return result;
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

bool GetReportsFromDatabase(
    std::vector<crashpad::CrashReportDatabase::Report>& pending_reports,
    std::vector<crashpad::CrashReportDatabase::Report>& completed_reports) {
  std::unique_ptr<crashpad::CrashReportDatabase> database =
      crashpad::CrashReportDatabase::InitializeWithoutCreating(
          crash_reporter::GetCrashpadDatabasePath());
  // `database` could be null if it has not been initialized yet.
  if (!database) {
    VLOG(1) << "enterprise.crash_reporting: failed to fetch crashpad db";
    return false;
  }

  crashpad::CrashReportDatabase::OperationStatus status;
  status = database->GetPendingReports(&pending_reports);
  if (status != crashpad::CrashReportDatabase::kNoError) {
    return false;
  }
  status = database->GetCompletedReports(&completed_reports);
  if (status != crashpad::CrashReportDatabase::kNoError) {
    return false;
  }
  return true;
}

// Return a list of new Reports that are ready to be to sent to the
// reporting server. It returns an empty list if any operation fails or
// there is no new report.
std::vector<crashpad::CrashReportDatabase::Report> GetNewReports(
    time_t latest_creation_time) {
  // Get the creation time of the latest report that was sent to the reporting
  // server last time.
  // latest_crash_report is the filepath where stores the latest report info
  std::vector<crashpad::CrashReportDatabase::Report> reports;
  std::vector<crashpad::CrashReportDatabase::Report> pending_reports;
  std::vector<crashpad::CrashReportDatabase::Report> completed_reports;
  if (!GetReportsFromDatabase(pending_reports, completed_reports)) {
    return reports;
  }
  // Get reports that have not been sent (<= latest_creation_time)
  CopyNewReports(pending_reports, latest_creation_time, reports);
  CopyNewReports(completed_reports, latest_creation_time, reports);

  return reports;
}

}  // namespace

// TODO(b/238427470): unit testing this function
void BrowserCrashEventRouter::UploadToReportingServer(
    RealtimeReportingClient* reporting_client,
    ReportingSettings settings,
    std::vector<crashpad::CrashReportDatabase::Report> reports) {
  DCHECK(reporting_client);
  if (reports.empty()) {
    VLOG(1) << "enterprise.crash_reporting: no new crashes";
    return;
  }
  VLOG(1) << "enterprise.crash_reporting: " << reports.size()
          << " new crashes to report";
  const std::string version = version_info::GetVersionNumber();
  const std::string channel =
      version_info::GetChannelString(chrome::GetChannel());
  const std::string platform = version_info::GetOSType();

  int64_t latest_creation_time = -1;

  for (const auto& report : reports) {
    base::Value::Dict event;
    event.Set(kKeyChannel, channel);
    event.Set(kKeyVersion, version);
    event.Set(kKeyReportId, report.id);
    event.Set(kKeyPlatform, platform);
    event.Set(kKeyProfileUserName, reporting_client->GetProfileUserName());
    reporting_client->ReportPastEvent(
        ReportingServiceSettings::kBrowserCrashEvent, settings,
        std::move(event), base::Time::FromTimeT(report.creation_time));
    if (report.creation_time > latest_creation_time) {
      latest_creation_time = report.creation_time;
    }
  }
  SetLatestCrashReportTime(latest_creation_time);
}

void BrowserCrashEventRouter::ReportCrashes() {
  VLOG(1) << "enterprise.crash_reporting: checking for unreported crashes";
  DCHECK(reporting_client_);
  const absl::optional<ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  bool isBrowserCrashReportingEnabled =
      settings.has_value() &&
      settings->enabled_event_names.count(
          ReportingServiceSettings::kBrowserCrashEvent) != 0;
  VLOG(1) << "enterprise.crash_reporting: crash reporting enabled: "
          << isBrowserCrashReportingEnabled;
  if (!isBrowserCrashReportingEnabled) {
    g_browser_process->local_state()->ClearPref(
        enterprise_connectors::kLatestCrashReportCreationTime);
    return;
  }
  time_t latest_creation_time = GetLatestCrashReportTime();
  if (latest_creation_time == 0) {
    latest_creation_time = base::Time::Now().ToTimeT();
    SetLatestCrashReportTime(latest_creation_time);
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&GetNewReports, latest_creation_time),
      base::BindOnce(&BrowserCrashEventRouter::UploadToReportingServer,
                     AsWeakPtr(), reporting_client_, std::move(*settings)));
}

void BrowserCrashEventRouter::OnCloudReportingLaunched(
    enterprise_reporting::ReportScheduler* report_scheduler) {
  VLOG(1) << "enterprise.crash_reporting: crash event reporting initializing";
  // An initial call to ReportCrashes() is required because the first call
  // in the repeating callback happens after the delay.
  ReportCrashes();
  repeating_crash_report_.Start(FROM_HERE, GetCrashpadPollingInterval(), this,
                                &BrowserCrashEventRouter::ReportCrashes);
}
#endif  // !BUILDFLAG(IS_FUCHSIA)

BrowserCrashEventRouter::BrowserCrashEventRouter(
    content::BrowserContext* context) {
  reporting_client_ = RealtimeReportingClientFactory::GetForProfile(context);
  if (base::FeatureList::IsEnabled(kBrowserCrashEventsEnabled)) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    controller_ = g_browser_process->browser_policy_connector()
                      ->chrome_browser_cloud_management_controller();
    controller_->AddObserver(this);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
  }
}

BrowserCrashEventRouter::~BrowserCrashEventRouter() {
  if (controller_) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    controller_->RemoveObserver(this);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
  }
}

}  // namespace enterprise_connectors
