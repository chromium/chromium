// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/android_metrics_provider.h"

#include "android_webview/browser/metrics/system_state_util.h"
#include "base/metrics/histogram_functions.h"
#include "components/metrics/android_metrics_helper.h"
#include "components/prefs/pref_registry_simple.h"

namespace android_webview {

namespace {
void EmitMultipleUserProfilesHistogram() {
  const MultipleUserProfilesState multiple_user_profiles_state =
      GetMultipleUserProfilesState();
  base::UmaHistogramEnumeration("Android.MultipleUserProfilesState",
                                multiple_user_profiles_state);
}
}  // namespace

void AndroidMetricsProvider::ProvidePreviousSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  EmitMultipleUserProfilesHistogram();
  metrics::AndroidMetricsHelper::GetInstance()->EmitHistograms(
      local_state_,
      /*current_session=*/false);
}

void AndroidMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  EmitMultipleUserProfilesHistogram();
  metrics::AndroidMetricsHelper::GetInstance()->EmitHistograms(
      local_state_,
      /*current_session=*/true);
}

// static
void AndroidMetricsProvider::RegisterPrefs(PrefRegistrySimple* registry) {
  metrics::AndroidMetricsHelper::RegisterPrefs(registry);
}

// static
void AndroidMetricsProvider::ResetGlobalStateForTesting() {
  metrics::AndroidMetricsHelper::GetInstance()->ResetForTesting();  // IN-TEST
}

}  // namespace android_webview
