// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/report_scheduler_desktop.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_report_generator.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/enterprise/browser/reporting/report_scheduler.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

PrefService* LocalState() {
  return g_browser_process->local_state();
}

}  // namespace

ReportSchedulerDesktop::ReportSchedulerDesktop()
    : ReportSchedulerDesktop(nullptr, false) {}

ReportSchedulerDesktop::ReportSchedulerDesktop(Profile* profile,
                                               bool profile_reporting) {
  if (profile_reporting) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Profile reporting is on LaCrOs instead of Ash.
    NOTREACHED();
#endif
    profile_ = profile;
    prefs_ = profile->GetPrefs();
    // Extension request hasn't support profile report yet. When we do, we also
    // need to refactor the code to avoid multiple extension request observer.
  } else {
    profile_ = nullptr;
    prefs_ = LocalState();
    extension_request_observer_factory_ =
        std::make_unique<ExtensionRequestObserverFactory>(profile);
  }
}

ReportSchedulerDesktop::~ReportSchedulerDesktop() {
  // If new profiles have been added since the last report was sent, they won't
  // be reported until the next launch, since Chrome is shutting down. However,
  // the (now obsolete) Enterprise.CloudReportingStaleProfileCount metric has
  // shown that this very rarely happens, with 99.23% of samples reporting no
  // stale profiles and 0.72% reporting a single stale profile.
  if (ShouldReportUpdates())
    g_browser_process->GetBuildState()->RemoveObserver(this);
}

PrefService* ReportSchedulerDesktop::GetPrefService() {
  return prefs_;
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
  if (GetPrefService()->GetString(kLastUploadVersion) !=
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
    GetPrefService()->SetString(kLastUploadVersion, chrome::kChromeVersion);
  }
}

void ReportSchedulerDesktop::StartWatchingExtensionRequestIfNeeded() {
  if (!extension_request_observer_factory_)
    return;

  // On CrOS, the function may be called twice during startup.
  if (extension_request_observer_factory_->IsReportEnabled())
    return;

  extension_request_observer_factory_->EnableReport(
      base::BindRepeating(&ReportSchedulerDesktop::TriggerExtensionRequest,
                          base::Unretained(this)));
}

void ReportSchedulerDesktop::StopWatchingExtensionRequest() {
  if (extension_request_observer_factory_)
    extension_request_observer_factory_->DisableReport();
}

void ReportSchedulerDesktop::OnExtensionRequestUploaded() {}

policy::DMToken ReportSchedulerDesktop::GetProfileDMToken() {
  absl::optional<std::string> dm_token = reporting::GetUserDmToken(profile_);
  if (!dm_token || dm_token->empty())
    return policy::DMToken();
  return policy::DMToken(policy::DMToken::Status::kValid, *dm_token);
}

std::string ReportSchedulerDesktop::GetProfileClientId() {
  return reporting::GetUserClientId(profile_).value_or(std::string());
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

void ReportSchedulerDesktop::TriggerExtensionRequest(Profile* profile) {
  if (!trigger_realtime_report_callback_.is_null()) {
    trigger_realtime_report_callback_.Run(
        ReportScheduler::ReportTrigger::kTriggerExtensionRequestRealTime,
        ExtensionRequestReportGenerator::ExtensionRequestData(profile));
  }
}

}  // namespace enterprise_reporting
