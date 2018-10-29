// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/cros_settings.h"
#endif

namespace {

const bool* g_metrics_consent_for_testing = nullptr;

}  // namespace

// static
void ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
    const bool* value) {
  DCHECK_NE(g_metrics_consent_for_testing == nullptr, value == nullptr)
      << "Unpaired set/reset";

  g_metrics_consent_for_testing = value;
}

// static
bool ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled() {
  return IsMetricsAndCrashReportingEnabled(g_browser_process->local_state());
}

// static
bool ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled(
    PrefService* local_state) {
  if (g_metrics_consent_for_testing)
    return *g_metrics_consent_for_testing;

  // TODO(blundell): Fix the unittests that don't set up the UI thread and
  // change this to just be DCHECK_CURRENTLY_ON().
  DCHECK(!content::BrowserThread::IsThreadInitialized(
             content::BrowserThread::UI) ||
         content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // This is only possible during unit tests. If the unit test didn't set the
  // local_state then it doesn't care about pref value and therefore we return
  // false.
  if (!local_state) {
    DLOG(WARNING) << "Local state has not been set and pref cannot be read";
    return false;
  }

  return IsMetricsReportingEnabled(local_state);
}

// static
bool ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
    base::StringPiece trial_name,
    base::StringPiece group_name) {
  return metrics::MetricsServiceAccessor::RegisterSyntheticFieldTrial(
      g_browser_process->metrics_service(), trial_name, group_name);
}

// static
bool ChromeMetricsServiceAccessor::RegisterSyntheticMultiGroupFieldTrial(
    base::StringPiece trial_name,
    const std::vector<uint32_t>& group_name_hashes) {
  return metrics::MetricsServiceAccessor::RegisterSyntheticMultiGroupFieldTrial(
      g_browser_process->metrics_service(), trial_name, group_name_hashes);
}

// static
bool ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrialWithNameHash(
    uint32_t trial_name_hash,
    base::StringPiece group_name) {
  return metrics::MetricsServiceAccessor::
      RegisterSyntheticFieldTrialWithNameHash(
          g_browser_process->metrics_service(), trial_name_hash, group_name);
}

// static
void ChromeMetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
    bool value) {
  metrics::MetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
      value);
}
