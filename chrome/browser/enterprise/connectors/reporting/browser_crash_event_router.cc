// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/browser_crash_event_router.h"

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/reporting/reporting_service_settings.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
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

constexpr base::FilePath::CharType LATEST_CRASH_REPORT[] =
    FILE_PATH_LITERAL("LatestCrashReport");

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

// Read latest_creation_time from
// {User_Data_Dir}/Enterprise/ReportingConnector/LatestCrashReport,
// and return it. On error, it returns -1.
int64_t GetLatestCreationTime(base::FilePath& latest_crash_report) {
  // Get the path of the file that stores the latest crash report info
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &latest_crash_report)) {
    return -1;
  }
  latest_crash_report = latest_crash_report.Append(RC_BASE_DIR);
  // CreateDirectory() evaluates to true no matter the directory already exists
  // or not, unless there is an error.
  if (!base::CreateDirectory(latest_crash_report)) {
    return -1;
  }
  latest_crash_report = latest_crash_report.Append(LATEST_CRASH_REPORT);
  std::string latest_creation_time_str;
  // ReadFileToString() evaluates to false if the file does not exist or it
  // exceeds the max size 32 bytes.
  if (!base::ReadFileToStringWithMaxSize(latest_crash_report,
                                         &latest_creation_time_str, 32)) {
    // Then we create the file with empty contents.
    base::ImportantFileWriter::WriteFileAtomically(latest_crash_report,
                                                   latest_creation_time_str);
    // No reports have been sent since the file does not even exist.
    return 0;
  }
  base::TrimWhitespaceASCII(latest_creation_time_str,
                            base::TrimPositions::TRIM_TRAILING,
                            &latest_creation_time_str);
  int64_t latest_creation_time;
  if (!base::StringToInt64(latest_creation_time_str, &latest_creation_time) ||
      latest_creation_time < 0) {
    return -1;
  }
  return latest_creation_time;
}

bool GetReportsFromDatabase(
    std::vector<crashpad::CrashReportDatabase::Report>& pending_reports,
    std::vector<crashpad::CrashReportDatabase::Report>& completed_reports) {
  crashpad::CrashReportDatabase* database =
      crash_reporter::internal::GetCrashReportDatabase();
  // `database` could be null if it has not been initialized yet.
  if (!database) {
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
std::vector<crashpad::CrashReportDatabase::Report> GetNewReports() {
  // Get the creation time of the latest report that was sent to the reporting
  // server last time.
  // latest_crash_report is the filepath where stores the latest report info
  base::FilePath latest_crash_report;
  int64_t latest_creation_time = GetLatestCreationTime(latest_crash_report);
  std::vector<crashpad::CrashReportDatabase::Report> reports;
  if (latest_creation_time < 0) {
    return reports;
  }

  // Get all pending and completed reports from the crashpad database
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

void WriteLatestCrashReportTime(int64_t latest_creation_time) {
  // Read the latest_creation_time
  base::FilePath latest_crash_report;
  int64_t prev_latest_creation_time =
      GetLatestCreationTime(latest_crash_report);

  if (latest_creation_time < prev_latest_creation_time) {
    LOG(WARNING) << "Current latest_creation_time ("
                 << prev_latest_creation_time
                 << ") is greater than the new value (" << latest_creation_time
                 << "). Not updating " << latest_crash_report.value();
    return;
  }
  base::ImportantFileWriter::WriteFileAtomically(
      latest_crash_report, base::NumberToString(latest_creation_time));
}
}  // namespace

// TODO(b/238427470): unit testing this function
void BrowserCrashEventRouter::UploadToReportingServer(
    RealtimeReportingClient* reporting_client,
    ReportingSettings settings,
    std::vector<crashpad::CrashReportDatabase::Report> reports) {
  DCHECK(reporting_client);
  if (reports.empty()) {
    return;
  }

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
    reporting_client->ReportRealtimeEvent(
        ReportingServiceSettings::kBrowserCrashEvent, settings,
        std::move(event));
    if (report.creation_time > latest_creation_time) {
      latest_creation_time = report.creation_time;
    }
  }

  // Write the latest_creation_time back to file
  if (!reports.empty()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&WriteLatestCrashReportTime, latest_creation_time));
  }
}

void BrowserCrashEventRouter::ReportCrashes() {
  DCHECK(reporting_client_);
  const absl::optional<ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(
          ReportingServiceSettings::kBrowserCrashEvent) == 0) {
    return;
  }
  // GetNewReports() may block since it has file I/O operations
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&GetNewReports),
      base::BindOnce(&BrowserCrashEventRouter::UploadToReportingServer,
                     AsWeakPtr(), reporting_client_, std::move(*settings)));
}

void BrowserCrashEventRouter::OnCloudReportingLaunched(
    enterprise_reporting::ReportScheduler* report_scheduler) {
  ReportCrashes();
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
