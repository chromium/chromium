// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_choice_service.h"
#include "components/policy/core/common/features.h"
#include "components/prefs/pref_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/PrivacyPreferencesManagerImpl_jni.h"

static bool JNI_PrivacyPreferencesManagerImpl_IsMetricsReportingEnabled(
    JNIEnv* env) {
  PrefService* local_state = g_browser_process->local_state();
  return metrics::MetricsReportingChoiceService::IsBasicMetricsReportingEnabled(
      local_state);
}

static void JNI_PrivacyPreferencesManagerImpl_SetMetricsReportingEnabled(
    JNIEnv* env,
    bool enabled) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(metrics::prefs::kMetricsReportingEnabled, enabled);
}

static bool
JNI_PrivacyPreferencesManagerImpl_IsMetricsReportingDisabledByPolicy(
    JNIEnv* env) {
  const PrefService* local_state = g_browser_process->local_state();
  return metrics::MetricsReportingChoiceService::
      IsMetricsReportingDisabledByPolicy(local_state);
}

DEFINE_JNI(PrivacyPreferencesManagerImpl)
