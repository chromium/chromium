// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/report_scheduler_desktop.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_report_generator.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/enterprise/browser/reporting/report_scheduler.h"
#include "components/prefs/pref_service.h"
#include "components/reporting/client/report_queue_provider.h"

namespace em = enterprise_management;

namespace enterprise_reporting {

namespace {

// Returns true if this build should generate basic reports when an update is
// detected.
// TODO(crbug.com/1102047): Get rid of this function after Chrome OS reporting
// logic has been split to its own delegates.
constexpr bool ShouldReportUpdates() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return false;
#else
  return true;
#endif
}

}  // namespace

ReportSchedulerDesktop::ReportSchedulerDesktop(Profile* profile)
    : extension_request_observer_factory_(profile) {}

ReportSchedulerDesktop::~ReportSchedulerDesktop() {
  // If new profiles have been added since the last report was sent, they won't
  // be reported until the next launch, since Chrome is shutting down. However,
  // the (now obsolete) Enterprise.CloudReportingStaleProfileCount metric has
  // shown that this very rarely happens, with 99.23% of samples reporting no
  // stale profiles and 0.72% reporting a single stale profile.
  if (ShouldReportUpdates())
    g_browser_process->GetBuildState()->RemoveObserver(this);
}

PrefService* ReportSchedulerDesktop::GetLocalState() {
  return g_browser_process->local_state();
}

void ReportSchedulerDesktop::StartWatchingUpdatesIfNeeded(
    base::Time last_upload,
    base::TimeDelta upload_interval) {
  if (!ShouldReportUpdates())
    return;

  auto* build_state = g_browser_process->GetBuildState();
  if (build_state->HasObserver(this))
    // Already watching browser updates.
    return;

  build_state->AddObserver(this);

  // Generate and upload a basic report immediately if the version has
  // changed since the last upload and the last upload was less than 24h
  // ago.
  if (GetLocalState()->GetString(kLastUploadVersion) !=
          chrome::kChromeVersion &&
      last_upload + upload_interval > base::Time::Now() &&
      !trigger_report_callback_.is_null()) {
    trigger_report_callback_.Run(
        ReportScheduler::ReportTrigger::kTriggerNewVersion);
  }
}

void ReportSchedulerDesktop::StopWatchingUpdates() {
  if (ShouldReportUpdates()) {
    g_browser_process->GetBuildState()->RemoveObserver(this);
  }
}

void ReportSchedulerDesktop::OnBrowserVersionUploaded() {
  if (ShouldReportUpdates()) {
    // Remember what browser version made this upload.
    GetLocalState()->SetString(kLastUploadVersion, chrome::kChromeVersion);
  }
}

void ReportSchedulerDesktop::StartWatchingExtensionRequestIfNeeded() {
  // On CrOS, the function may be called twice during startup.
  if (extension_request_observer_factory_.IsReportEnabled())
    return;

  extension_request_observer_factory_.EnableReport(
      base::BindRepeating(&ReportSchedulerDesktop::TriggerExtensionRequest,
                          base::Unretained(this)));
}

void ReportSchedulerDesktop::StopWatchingExtensionRequest() {
  extension_request_observer_factory_.DisableReport();
}

void ReportSchedulerDesktop::OnExtensionRequestUploaded() {}

void ReportSchedulerDesktop::OnUpdate(const BuildState* build_state) {
  DCHECK(ShouldReportUpdates());
  // A new version has been detected on the machine and a restart is now needed
  // for it to take effect. Send a basic report (without profile info)
  // immediately.
  if (!trigger_report_callback_.is_null()) {
    trigger_report_callback_.Run(
        ReportScheduler::ReportTrigger::kTriggerUpdate);
  }
}

void ReportSchedulerDesktop::TriggerExtensionRequest(Profile* profile) {
  if (!trigger_realtime_report_callback_.is_null()) {
    trigger_realtime_report_callback_.Run(
        ReportScheduler::ReportTrigger::kTriggerExtensionRequestRealTime,
        ExtensionRequestReportGenerator::ExtensionRequestData(profile));
  }
}

}  // namespace enterprise_reporting
