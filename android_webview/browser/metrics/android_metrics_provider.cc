// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/android_metrics_provider.h"

#include "components/metrics/android_metrics_helper.h"

namespace android_webview {

void AndroidMetricsProvider::ProvidePreviousSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  metrics::AndroidMetricsHelper::GetInstance()->EmitHistograms(
      /*current_session=*/false);
}

void AndroidMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  metrics::AndroidMetricsHelper::GetInstance()->EmitHistograms(
      /*current_session=*/true);
}

}  // namespace android_webview
