// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_metrics_filtering_status_metrics_provider.h"

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/common/aw_features.h"
#include "components/embedder_support/android/metrics/android_metrics_service_client.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace android_webview {

AwMetricsFilteringStatusMetricsProvider::
    AwMetricsFilteringStatusMetricsProvider(AwMetricsServiceClient* client)
    : client_(client) {
  CHECK(client);
}

void AwMetricsFilteringStatusMetricsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile) {
  if (client_->ShouldApplyMetricsFiltering()) {
    system_profile->set_metrics_filtering_status(
        metrics::SystemProfileProto::METRICS_ONLY_CRITICAL);
  } else {
    system_profile->set_metrics_filtering_status(
        metrics::SystemProfileProto::METRICS_ALL);
  }
}

}  // namespace android_webview
