// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_reporting_state.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_runner_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/metrics/cloned_install_detector.h"
#include "components/metrics/entropy_state.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace {

enum MetricsReportingChangeHistogramValue {
  METRICS_REPORTING_ERROR,
  METRICS_REPORTING_DISABLED,
  METRICS_REPORTING_ENABLED,
  METRICS_REPORTING_MAX
};

void RecordMetricsReportingHistogramValue(
    MetricsReportingChangeHistogramValue value) {
  UMA_HISTOGRAM_ENUMERATION(
      "UMA.MetricsReporting.Toggle", value, METRICS_REPORTING_MAX);
}

// Tries to set metrics reporting status to |enabled| and returns whatever is
// the result of the update.
bool SetGoogleUpdateSettings(bool enabled) {
  GoogleUpdateSettings::SetCollectStatsConsent(enabled);
  bool updated_pref = GoogleUpdateSettings::GetCollectStatsConsent();
  if (enabled != updated_pref)
    DVLOG(1) << "Unable to set metrics reporting status to " << enabled;

  return updated_pref;
}

// Does the necessary changes for MetricsReportingEnabled changes which needs
// to be done in the main thread.
// As arguments this function gets:
//  |to_update_pref| which indicates what the desired update should be,
//  |callback_fn| is the callback function to be called in the end
//  |updated_pref| is the result of attempted update.
// Update considers to be successful if |to_update_pref| and |updated_pref| are
// the same.
void SetMetricsReporting(bool to_update_pref,
                         OnMetricsReportingCallbackType callback_fn,
                         bool updated_pref) {
  g_browser_process->local_state()->SetBoolean(
      metrics::prefs::kMetricsReportingEnabled, updated_pref);

  UpdateMetricsPrefsOnPermissionChange(updated_pref);

  // Uses the current state of whether reporting is enabled to enable services.
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(true);

  if (to_update_pref == updated_pref) {
    RecordMetricsReportingHistogramValue(updated_pref ?
        METRICS_REPORTING_ENABLED : METRICS_REPORTING_DISABLED);
  } else {
    RecordMetricsReportingHistogramValue(METRICS_REPORTING_ERROR);
  }
  if (!callback_fn.is_null())
    std::move(callback_fn).Run(updated_pref);
}

}  // namespace

void ChangeMetricsReportingState(bool enabled) {
  ChangeMetricsReportingStateWithReply(enabled,
                                       OnMetricsReportingCallbackType());
}

// TODO(gayane): Instead of checking policy before setting the metrics pref set
// the pref and register for notifications for the rest of the changes.
void ChangeMetricsReportingStateWithReply(
    bool enabled,
    OnMetricsReportingCallbackType callback_fn) {
#if !defined(OS_ANDROID)
  if (IsMetricsReportingPolicyManaged()) {
    if (!callback_fn.is_null()) {
      const bool metrics_enabled =
          ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
      std::move(callback_fn).Run(metrics_enabled);
    }
    return;
  }
#endif
  base::PostTaskAndReplyWithResult(
      GoogleUpdateSettings::CollectStatsConsentTaskRunner(), FROM_HERE,
      base::BindOnce(&SetGoogleUpdateSettings, enabled),
      base::BindOnce(&SetMetricsReporting, enabled, std::move(callback_fn)));
}

void UpdateMetricsPrefsOnPermissionChange(bool metrics_enabled) {
  if (metrics_enabled) {
    // When a user opts in to the metrics reporting service, the previously
    // collected data should be cleared to ensure that nothing is reported
    // before a user opts in and all reported data is accurate.
    g_browser_process->metrics_service()->ClearSavedStabilityMetrics();
  } else {
    // Clear the client id and low entropy sources pref when opting out.
    // Note: This will not affect the running state (e.g. field trial
    // randomization), as the pref is only read on startup.
    UMA_HISTOGRAM_BOOLEAN("UMA.ClientIdCleared", true);
    g_browser_process->local_state()->ClearPref(
        metrics::prefs::kMetricsClientID);
    metrics::EntropyState::ClearPrefs(g_browser_process->local_state());
    metrics::ClonedInstallDetector::ClearClonedInstallInfo(
        g_browser_process->local_state());
    g_browser_process->local_state()->ClearPref(
        metrics::prefs::kMetricsReportingEnabledTimestamp);
    crash_keys::ClearMetricsClientId();
  }
}

#if !defined(OS_ANDROID)
void ApplyMetricsReportingPolicy() {
  GoogleUpdateSettings::CollectStatsConsentTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&GoogleUpdateSettings::SetCollectStatsConsent),
          ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled()));
}
#endif

bool IsMetricsReportingPolicyManaged() {
  const PrefService* pref_service = g_browser_process->local_state();
  const PrefService::Preference* pref =
      pref_service->FindPreference(metrics::prefs::kMetricsReportingEnabled);
  return pref && pref->IsManaged();
}
