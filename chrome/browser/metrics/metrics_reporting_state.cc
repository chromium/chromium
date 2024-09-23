// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_reporting_state.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process_platform_part.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_ANDROID)
#include "components/policy/core/common/features.h"
#endif  // BUILDFLAG(IS_ANDROID)

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
//  |callback_fn| is the callback function to be called in the end,
//  |called_from| is from where the call was made,
//  |updated_pref| is the result of attempted update.
// Update considers to be successful if |to_update_pref| and |updated_pref| are
// the same.
void SetMetricsReporting(bool to_update_pref,
                         OnMetricsReportingCallbackType callback_fn,
                         ChangeMetricsReportingStateCalledFrom called_from,
                         bool updated_pref) {
  g_browser_process->local_state()->SetBoolean(
      metrics::prefs::kMetricsReportingEnabled, updated_pref);

  UpdateMetricsPrefsOnPermissionChange(updated_pref, called_from);

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

void ChangeMetricsReportingState(
    bool enabled,
    ChangeMetricsReportingStateCalledFrom called_from) {
  ChangeMetricsReportingStateWithReply(
      enabled, OnMetricsReportingCallbackType(), called_from);
}

// TODO(gayane): Instead of checking policy before setting the metrics pref set
// the pref and register for notifications for the rest of the changes.
void ChangeMetricsReportingStateWithReply(
    bool enabled,
    OnMetricsReportingCallbackType callback_fn,
    ChangeMetricsReportingStateCalledFrom called_from) {
#if !BUILDFLAG(IS_ANDROID)
  // Chrome OS manages metrics settings externally and changes to reporting
  // should be propagated to metrics service regardless if the policy is managed
  // or not.
  // TODO(crbug.com/40232452): Possibly change |is_chrome_os| to use
  // BUILDFLAG(IS_CHROMEOS_ASH).
  bool is_chrome_os =
      (called_from ==
       ChangeMetricsReportingStateCalledFrom::kCrosMetricsSettingsChange) ||
      (called_from ==
       ChangeMetricsReportingStateCalledFrom::kCrosMetricsSettingsCreated);
  if (IsMetricsReportingPolicyManaged() && !is_chrome_os) {
    if (!callback_fn.is_null()) {
      const bool metrics_enabled =
          ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
      std::move(callback_fn).Run(metrics_enabled);
    }
    return;
  }
#endif
  GoogleUpdateSettings::CollectStatsConsentTaskRunner()
      ->PostTaskAndReplyWithResult(
          FROM_HERE, base::BindOnce(&SetGoogleUpdateSettings, enabled),
          base::BindOnce(&SetMetricsReporting, enabled, std::move(callback_fn),
                         called_from));
}

void UpdateMetricsPrefsOnPermissionChange(
    bool metrics_enabled,
    ChangeMetricsReportingStateCalledFrom called_from) {
  // On Chrome OS settings creation, nothing should be performed (the metrics
  // service is simply being initialized). Otherwise, for users who have
  // metrics reporting disabled, their client ID and low entropy sources would
  // be cleared on each log in. For users who have metrics reporting enabled,
  // their stability metrics and histogram data would be cleared.
  if (called_from ==
      ChangeMetricsReportingStateCalledFrom::kCrosMetricsSettingsCreated) {
    return;
  }
  if (metrics_enabled) {
    // When a user opts in to the metrics reporting service, the previously
    // collected data should be cleared to ensure that nothing is reported
    // before a user opts in and all reported data is accurate.
    g_browser_process->metrics_service()->ClearSavedStabilityMetrics();
    if (called_from == ChangeMetricsReportingStateCalledFrom::kUiSettings) {
      ClearPreviouslyCollectedMetricsData();
    }
    return;
  }
#if BUILDFLAG(IS_ANDROID)
  // When a user disables metrics reporting on Android Chrome, the new
  // sampling trial should be used to determine whether the client is sampled
  // in or out (if the user ever re-enables metrics reporting).
  //
  // Existing metrics-reporting-enabled clients (i.e. the users without this
  // pref set) do not use the new sampling trial; they continue to use
  // MetricsAndCrashSampling. However, if such a user disables metrics
  // reporting and later re-enables it, they will start using the new trial.
  //
  // See crbug/1306481 and the comment above |kUsePostFREFixSamplingTrial| in
  // components/metrics/metrics_pref_names.cc for more details.
  g_browser_process->local_state()->SetBoolean(
      metrics::prefs::kUsePostFREFixSamplingTrial, true);
#endif  // BUILDFLAG(IS_ANDROID)

  // Clear the client id and low entropy sources pref when the user opts out
  // from a non-FRE source. In the FRE flow the entropy source is not cleared
  // since no data has been uploaded yet.
  // Note: This will not affect the running state (e.g. field trial
  // randomization), as the pref is only read on startup.

  UMA_HISTOGRAM_BOOLEAN("UMA.ClientIdCleared", true);

  PrefService* local_state = g_browser_process->local_state();

  local_state->ClearPref(metrics::prefs::kMetricsClientID);
  local_state->ClearPref(metrics::prefs::kMetricsProvisionalClientID);
  local_state->ClearPref(metrics::prefs::kMetricsLogRecordId);

  // Don't clear the entropy state if the user opted out in the FRE. This is to
  // prevent experiments that have been randomized based on the low-entropy
  // source from having their state re-rolled on a subsequent session.
  if (called_from != ChangeMetricsReportingStateCalledFrom::kUiFirstRun) {
    metrics::EntropyState::ClearPrefs(local_state);
  }
  metrics::ClonedInstallDetector::ClearClonedInstallInfo(local_state);
  local_state->ClearPref(metrics::prefs::kMetricsReportingEnabledTimestamp);
  crash_keys::ClearMetricsClientId();
}

void ApplyMetricsReportingPolicy() {
  GoogleUpdateSettings::CollectStatsConsentTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&GoogleUpdateSettings::SetCollectStatsConsent),
          ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled()));
}

bool IsMetricsReportingPolicyManaged() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy::BrowserPolicyConnectorAsh* policy_connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return policy_connector->IsDeviceEnterpriseManaged();
#else
  const PrefService* pref_service = g_browser_process->local_state();
  const PrefService::Preference* pref =
      pref_service->FindPreference(metrics::prefs::kMetricsReportingEnabled);
  return pref && pref->IsManaged();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ClearPreviouslyCollectedMetricsData() {
  // Mark histogram data that was collected during the current session up until
  // now as reported so that they are not included in the next log.
  g_browser_process->metrics_service()->MarkCurrentHistogramsAsReported();
  // Note: There is no need to clear User Actions as they do not get recorded
  // when metrics reporting is disabled.
}
