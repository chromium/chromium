// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_BROWSER_CRASH_EVENT_ROUTER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_BROWSER_CRASH_EVENT_ROUTER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"

#if !BUILDFLAG(IS_FUCHSIA)
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#endif  // !BUILDFLAG(IS_FUCHSIA)
namespace enterprise_connectors {
// This class collects crash reports from the crashpad database
// and then sends generated crash events to the reporting server.
class BrowserCrashEventRouter
    : public policy::ChromeBrowserCloudManagementController::Observer,
      public base::SupportsWeakPtr<BrowserCrashEventRouter> {
 public:
  // BrowserCrashEventRouter adds itself to
  // ChromeBrowserCloudManagementController::observers in the constructor, so
  // that once the browser launches, OnCloudReportingLaunched() will be called,
  // where we can call ReportCrashes() to report crashes.
  explicit BrowserCrashEventRouter(content::BrowserContext* context);

  BrowserCrashEventRouter(const BrowserCrashEventRouter&) = delete;
  BrowserCrashEventRouter& operator=(const BrowserCrashEventRouter&) = delete;
  BrowserCrashEventRouter(BrowserCrashEventRouter&&) = delete;
  BrowserCrashEventRouter& operator=(BrowserCrashEventRouter&&) = delete;
  ~BrowserCrashEventRouter() override;

#if !BUILDFLAG(IS_FUCHSIA)
  void OnCloudReportingLaunched(
      enterprise_reporting::ReportScheduler* report_scheduler) override;
  void UploadToReportingServer(
      RealtimeReportingClient* reporting_client,
      ReportingSettings settings,
      std::vector<crashpad::CrashReportDatabase::Report> reports);
#endif  // !BUILDFLAG(IS_FUCHSIA)

 private:
  raw_ptr<enterprise_connectors::RealtimeReportingClient, DanglingUntriaged>
      reporting_client_ = nullptr;
  raw_ptr<policy::ChromeBrowserCloudManagementController, DanglingUntriaged>
      controller_ = nullptr;

#if !BUILDFLAG(IS_FUCHSIA)
  // ReportCrashes() checks the enterprise policy settings, retrieves crash
  // reports from the crashpad local database and sends reports that have not
  // been sent to the reporting server.
  // TODO(b/238427661): Add a background thread that periodically report
  // crashes.
  void ReportCrashes();
#endif  // !BUILDFLAG(IS_FUCHSIA)
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_BROWSER_CRASH_EVENT_ROUTER_H_
