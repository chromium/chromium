// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/android_metrics_provider.h"

#include "android_webview/browser/metrics/system_state_util.h"
#include "base/metrics/histogram_functions.h"
#include "components/metrics/android_metrics_helper.h"

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
      /*current_session=*/false);
}

void AndroidMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  EmitMultipleUserProfilesHistogram();
  metrics::AndroidMetricsHelper::GetInstance()->EmitHistograms(
      /*current_session=*/true);
}

}  // namespace android_webview
