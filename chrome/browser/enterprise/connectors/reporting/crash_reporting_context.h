// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_CRASH_REPORTING_CONTEXT_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_CRASH_REPORTING_CONTEXT_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/timer/timer.h"
#include "chrome/browser/enterprise/connectors/reporting/browser_crash_event_router.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#endif

namespace enterprise_connectors {

class RealtimeReportingClient;

class CrashReportingContext
    : public policy::ChromeBrowserCloudManagementController::Observer {
 public:
  friend struct base::DefaultSingletonTraits<CrashReportingContext>;

  CrashReportingContext(const CrashReportingContext&) = delete;
  CrashReportingContext(CrashReportingContext&&) = delete;
  CrashReportingContext operator=(const CrashReportingContext&) = delete;
  CrashReportingContext operator=(CrashReportingContext&&) = delete;
  ~CrashReportingContext() override;

#if !BUILDFLAG(IS_CHROMEOS)
  static CrashReportingContext* GetInstance();

  void AddProfile(BrowserCrashEventRouter* router, Profile* profile);
  RealtimeReportingClient* GetCrashReportingClient() const;
  bool HasActiveProfile() const;
  void RemoveProfile(BrowserCrashEventRouter* router);

  // policy::Chromepolicy::ChromeBrowserCloudManagementController::Observer
  void OnBrowserUnenrolled(bool succeeded) override;
  void OnCloudReportingLaunched(
      enterprise_reporting::ReportScheduler* report_scheduler) override;
  void OnShutdown() override;

 private:
  CrashReportingContext();

  base::RepeatingTimer repeating_crash_report_;
  std::unordered_map<BrowserCrashEventRouter*,
                     raw_ptr<Profile, CtnExperimental>>
      active_profiles_;
#endif
};

#if !BUILDFLAG(IS_CHROMEOS)

// Utility function to parse reports from a crash database that were
// created past a given timestamp. Included in header for testing.
std::vector<crashpad::CrashReportDatabase::Report> GetNewReportsFromDatabase(
    time_t latest_creation_time,
    crashpad::CrashReportDatabase* database);

// Update the local_state pref with the latest crash upload timestamp.
// Included in header for testing.
void SetLatestCrashReportTime(PrefService* local_state, time_t timestamp);

// Get the current value of the latest crash upload timestamp. Included in
// header for testing.
time_t GetLatestCrashReportTime(PrefService* local_state);

// Upload a crash event reports to the reporting server if available and
// update latest crash upload timestamp if one or more such events are uploaded.
// Included in header for testing.
void UploadToReportingServer(
    base::WeakPtr<RealtimeReportingClient> reporting_client,
    PrefService* local_state,
    std::vector<crashpad::CrashReportDatabase::Report> reports);

// Get the time interval with which to query the crashpad database. Included
// in header for testing.
base::TimeDelta GetCrashpadPollingInterval();

#endif

}  // namespace enterprise_connectors
#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_CRASH_REPORTING_CONTEXT_H_
