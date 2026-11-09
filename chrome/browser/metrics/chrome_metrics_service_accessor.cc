// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"

#include <string_view>

#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

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

  // Early browser startup calls this before the main thread is registered as
  // BrowserThread::UI, e.g. when the MetricsStateManager is created during
  // field trial setup in ChromeMainDelegate::PostEarlyInitialization(). Once
  // the UI thread is registered, all calls must happen on it.
  CHECK(!content::BrowserThread::IsThreadInitialized(
            content::BrowserThread::UI) ||
            content::BrowserThread::CurrentlyOn(content::BrowserThread::UI),
        base::NotFatalUntil::M161);

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
    std::string_view trial_name,
    std::string_view group_name,
    variations::SyntheticTrialAnnotationMode annotation_mode) {
  return metrics::MetricsServiceAccessor::RegisterSyntheticFieldTrial(
      g_browser_process->metrics_service(), trial_name, group_name,
      annotation_mode);
}

void ChromeMetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
    bool value) {
  metrics::MetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
      value);
}
