// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/report_scheduler_desktop.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_report_throttler.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

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

bool ShouldReportExtensionRequestRealtime() {
  return base::FeatureList::IsEnabled(
      features::kEnterpriseRealtimeExtensionRequest);
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
  ExtensionRequestReportThrottler::Get()->Disable();
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
  if (!ShouldReportExtensionRequestRealtime())
    return;

  // On CrOS, the function may be called twice during startup.
  if (ExtensionRequestReportThrottler::Get()->IsEnabled())
    return;

  ExtensionRequestReportThrottler::Get()->Enable(
      features::kEnterpiseRealtimeExtensionRequestThrottleDelay.Get(),
      base::BindRepeating(&ReportSchedulerDesktop::TriggerExtensionRequest,
                          base::Unretained(this)));
}

void ReportSchedulerDesktop::StopWatchingExtensionRequest() {
  ExtensionRequestReportThrottler::Get()->Disable();
}

void ReportSchedulerDesktop::OnExtensionRequestUploaded() {
  auto* extension_request_report_throttler =
      ExtensionRequestReportThrottler::Get();
  if (extension_request_report_throttler->IsEnabled())
    extension_request_report_throttler->OnExtensionRequestUploaded();
}

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

void ReportSchedulerDesktop::TriggerExtensionRequest() {
  if (!trigger_report_callback_.is_null()) {
    trigger_report_callback_.Run(
        ReportScheduler::ReportTrigger::kTriggerExtensionRequest);
  }
}

}  // namespace enterprise_reporting
