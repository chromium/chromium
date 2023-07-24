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

void EmitPrimaryCpuAbiBitness(PrimaryCpuAbiBitness primary_cpu_abi_bitness) {
  if (primary_cpu_abi_bitness != PrimaryCpuAbiBitness::kUnknown) {
    base::UmaHistogramEnumeration("Android.WebView.PrimaryCpuAbiBitness",
                                  primary_cpu_abi_bitness);
  }
}

}  // namespace

void AndroidMetricsProvider::ProvidePreviousSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  EmitMultipleUserProfilesHistogram();

  // Make sure we didn't overwrite the stored state yet.
  CHECK(!local_state_saved_);
  auto primary_cpu_abi_bitness = static_cast<PrimaryCpuAbiBitness>(
      local_state_->GetInteger(prefs::kPrimaryCpuAbiBitnessPref));
  EmitPrimaryCpuAbiBitness(primary_cpu_abi_bitness);

  metrics::AndroidMetricsHelper::GetInstance()->EmitHistograms(
      local_state_,
      /*on_did_create_metrics_log=*/false);
}

void AndroidMetricsProvider::OnDidCreateMetricsLog() {
  EmitMultipleUserProfilesHistogram();

  PrimaryCpuAbiBitness primary_cpu_abi_bitness = GetPrimaryCpuAbiBitness();
  // This value may change across sessions, even though unlikely, so save  in
  // case this session dies prematurely.
  // The value won't change within the session, so save only once.
  if (!local_state_saved_) {
    local_state_->SetInteger(prefs::kPrimaryCpuAbiBitnessPref,
                             static_cast<int>(primary_cpu_abi_bitness));
    local_state_saved_ = true;
  }
  EmitPrimaryCpuAbiBitness(primary_cpu_abi_bitness);

  metrics::AndroidMetricsHelper::GetInstance()->EmitHistograms(
      local_state_,
      /*on_did_create_metrics_log=*/true);
}

// static
void AndroidMetricsProvider::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kPrimaryCpuAbiBitnessPref, 0);
  metrics::AndroidMetricsHelper::RegisterPrefs(registry);
}

// static
void AndroidMetricsProvider::ResetGlobalStateForTesting() {
  metrics::AndroidMetricsHelper::ResetGlobalStateForTesting();
  local_state_saved_ = false;
}

}  // namespace android_webview
