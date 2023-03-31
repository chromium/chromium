// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_client_side_sampling_status_metrics_provider.h"

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/common/aw_features.h"
#include "components/embedder_support/android/metrics/android_metrics_service_client.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace android_webview {

AwClientSideSamplingStatusMetricsProvider::
    AwClientSideSamplingStatusMetricsProvider(AwMetricsServiceClient* client)
    : client_(client) {
  CHECK(client != nullptr);
}

bool AwClientSideSamplingStatusMetricsProvider::IsClientSideSamplingDisabled() {
  return client_->GetSampleRatePerMille() == 1000;
}

void AwClientSideSamplingStatusMetricsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile) {
  if (IsClientSideSamplingDisabled()) {
    system_profile->set_client_side_sampling_status(
        metrics::SystemProfileProto::SAMPLING_NOT_APPLIED);
  } else {
    system_profile->set_client_side_sampling_status(
        metrics::SystemProfileProto::SAMPLING_APPLIED);
  }
}

}  // namespace android_webview
