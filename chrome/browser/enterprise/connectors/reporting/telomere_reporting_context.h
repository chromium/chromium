// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_TELOMERE_REPORTING_CONTEXT_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_TELOMERE_REPORTING_CONTEXT_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/timer/timer.h"
#include "chrome/browser/enterprise/connectors/reporting/telomere_event_router.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"

namespace enterprise_connectors {

class RealtimeReportingClient;

// TODO(crbug.com/407535337): Refactor to share code with CrashReportingContext.
class TelomereReportingContext
    : public policy::ChromeBrowserCloudManagementController::Observer {
 public:
  friend struct base::DefaultSingletonTraits<TelomereReportingContext>;

  TelomereReportingContext(const TelomereReportingContext&) = delete;
  TelomereReportingContext(TelomereReportingContext&&) = delete;
  TelomereReportingContext operator=(const TelomereReportingContext&) = delete;
  TelomereReportingContext operator=(TelomereReportingContext&&) = delete;
  ~TelomereReportingContext() override;

  static TelomereReportingContext* GetInstance();

  void AddProfile(TelomereEventRouter* router, Profile* profile);
  RealtimeReportingClient* GetReportingClient() const;
  bool HasActiveProfile() const;
  void RemoveProfile(TelomereEventRouter* router);

  // policy::Chromepolicy::ChromeBrowserCloudManagementController::Observer
  void OnBrowserUnenrolled(bool succeeded) override;
  void OnCloudReportingLaunched(
      enterprise_reporting::ReportScheduler* report_scheduler) override;
  void OnShutdown() override;

 private:
  TelomereReportingContext();

  base::RepeatingTimer repeating_telomere_log_;
  std::unordered_map<TelomereEventRouter*, raw_ptr<Profile, CtnExperimental>>
      active_profiles_;
};

// Utility function to list all the log dumps updated after
// `latest_creation_time` and return them as a <file contents, timestamp> pair.
std::vector<std::pair<std::string, time_t>> GetLogsFromPath(
    time_t latest_creation_time,
    base::FilePath sysmon_logs_path);

// Update the local_state pref with the latest telomere upload timestamp.
// Included in header for testing.
void SetLatestTelomereReportTime(PrefService* local_state, time_t timestamp);

// Get the current value of the latest telomere upload timestamp. Included in
// header for testing.
time_t GetLatestTelomereReportTime(PrefService* local_state);

// Upload a telomere event to the reporting server if available and update
// latest telomere upload timestamp if one or more such events are uploaded.
// Included in header for testing.
void UploadToReportingServer(
    base::WeakPtr<RealtimeReportingClient> reporting_client,
    PrefService* local_state,
    std::vector<std::pair<std::string, time_t>> reports);

// Get the time interval with which to query the filesystem location containing
// telomere dumps. Included in header for testing.
base::TimeDelta GetTelomerePollingInterval();

}  // namespace enterprise_connectors
#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_TELOMERE_REPORTING_CONTEXT_H_
