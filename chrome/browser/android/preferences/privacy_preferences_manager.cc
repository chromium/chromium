// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "chrome/android/chrome_jni_headers/PrivacyPreferencesManager_jni.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"

namespace {

PrefService* GetPrefService() {
  return ProfileManager::GetActiveUserProfile()
      ->GetOriginalProfile()
      ->GetPrefs();
}

}  // namespace

static jboolean JNI_PrivacyPreferencesManager_GetNetworkPredictionEnabled(
    JNIEnv* env) {
  return GetPrefService()->GetInteger(prefs::kNetworkPredictionOptions) !=
         chrome_browser_net::NETWORK_PREDICTION_NEVER;
}

static jboolean JNI_PrivacyPreferencesManager_GetNetworkPredictionManaged(
    JNIEnv* env) {
  return GetPrefService()->IsManagedPreference(
      prefs::kNetworkPredictionOptions);
}

static jboolean JNI_PrivacyPreferencesManager_IsMetricsReportingEnabled(
    JNIEnv* env) {
  PrefService* local_state = g_browser_process->local_state();
  return local_state->GetBoolean(metrics::prefs::kMetricsReportingEnabled);
}

static void JNI_PrivacyPreferencesManager_SetMetricsReportingEnabled(
    JNIEnv* env,
    jboolean enabled) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(metrics::prefs::kMetricsReportingEnabled, enabled);
}

static jboolean JNI_PrivacyPreferencesManager_IsMetricsReportingManaged(
    JNIEnv* env) {
  return GetPrefService()->IsManagedPreference(
      metrics::prefs::kMetricsReportingEnabled);
}

static jboolean JNI_PrivacyPreferencesManager_CanPrefetchAndPrerender(
    JNIEnv* env) {
  return chrome_browser_net::CanPrefetchAndPrerenderUI(GetPrefService()) ==
         chrome_browser_net::NetworkPredictionStatus::ENABLED;
}

static void JNI_PrivacyPreferencesManager_SetNetworkPredictionEnabled(
    JNIEnv* env,
    jboolean enabled) {
  GetPrefService()->SetInteger(
      prefs::kNetworkPredictionOptions,
      enabled ? chrome_browser_net::NETWORK_PREDICTION_WIFI_ONLY
              : chrome_browser_net::NETWORK_PREDICTION_NEVER);
}

static jboolean
JNI_PrivacyPreferencesManager_ObsoleteNetworkPredictionOptionsHasUserSetting(
    JNIEnv* env) {
  return GetPrefService()->GetUserPrefValue(prefs::kNetworkPredictionOptions) !=
         nullptr;
}
