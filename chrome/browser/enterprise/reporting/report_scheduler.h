// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_SCHEDULER_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_SCHEDULER_H_

#include <stdint.h>
#include <memory>

#include "base/macros.h"
#include "base/util/timer/wall_clock_timer.h"
#include "chrome/browser/enterprise/reporting/notification/extension_request_observer_factory.h"
#include "chrome/browser/enterprise/reporting/report_generator.h"
#include "chrome/browser/enterprise/reporting/report_uploader.h"
#include "chrome/browser/upgrade_detector/build_state_observer.h"
#include "components/prefs/pref_change_registrar.h"

namespace policy {
class CloudPolicyClient;
}  // namespace policy

namespace enterprise_reporting {

// Schedules report generation and upload every 24 hours and upon browser update
// for desktop Chrome while cloud reporting is enabled via administrative
// policy. If either of these triggers fires while a report is being generated,
// processing is deferred until the existing processing completes.
class ReportScheduler : public BuildStateObserver {
 public:
  ReportScheduler(policy::CloudPolicyClient* client,
                  std::unique_ptr<ReportGenerator> report_generator,
                  Profile* profile = nullptr);

  ~ReportScheduler() override;

  // Returns true if next report has been scheduled. The report will be
  // scheduled only if the previous report is uploaded successfully and the
  // reporting policy is still enabled.
  bool IsNextReportScheduledForTesting() const;

  void SetReportUploaderForTesting(std::unique_ptr<ReportUploader> uploader);

  void OnDMTokenUpdated();

  // BuildStateObserver:
  void OnUpdate(const BuildState* build_state) override;

 private:
  // The trigger leading to report generation. Values are bitmasks in the
  // |pending_triggers_| bitfield.
  enum ReportTrigger : uint32_t {
    kTriggerNone = 0,              // No trigger.
    kTriggerTimer = 1U << 0,       // The periodic timer expired.
    kTriggerUpdate = 1U << 1,      // An update was detected.
    kTriggerNewVersion = 1U << 2,  // A new version is running.
  };

  // Observes CloudReportingEnabled policy.
  void RegisterPrefObserver();

  // Handles kCloudReportingEnabled policy value change, including the first
  // policy value check during startup.
  void OnReportEnabledPrefChanged();

  // Stops the periodic timer and the update observer.
  void Stop();

  // Register |cloud_policy_client_| with dm token and client id for desktop
  // browser only. (Chrome OS doesn't need this step here.)
  bool SetupBrowserPolicyClientRegistration();

  // Starts the periodic timer based on the last time a report was uploaded.
  void Start(base::Time last_upload_time);

  // Starts report generation in response to |trigger|.
  void GenerateAndUploadReport(ReportTrigger trigger);

  // Continues processing a report (contained in the |requests| collection) by
  // sending it to the uploader.
  void OnReportGenerated(ReportGenerator::ReportRequests requests);

  // Finishes processing following report upload. |status| indicates the result
  // of the attempted upload.
  void OnReportUploaded(ReportUploader::ReportStatus status);

  // Initiates report generation for any triggers that arrived during generation
  // of another report.
  void RunPendingTriggers();

  // Records that |trigger| was responsible for an upload attempt.
  static void RecordUploadTrigger(ReportTrigger trigger);

  // Policy value watcher
  PrefChangeRegistrar pref_change_registrar_;

  policy::CloudPolicyClient* cloud_policy_client_;

  util::WallClockTimer request_timer_;

  std::unique_ptr<ReportUploader> report_uploader_;

  std::unique_ptr<ReportGenerator> report_generator_;

  ExtensionRequestObserverFactory extension_request_observer_factory_;

  // The trigger responsible for initiating active report generation.
  ReportTrigger active_trigger_ = kTriggerNone;

  // The set of triggers that have fired while processing a report (a bitfield
  // of ReportTrigger values). They will be handled following completion of the
  // in-process report.
  uint32_t pending_triggers_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ReportScheduler);
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_SCHEDULER_H_
