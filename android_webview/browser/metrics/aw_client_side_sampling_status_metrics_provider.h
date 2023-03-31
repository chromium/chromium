// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_METRICS_AW_CLIENT_SIDE_SAMPLING_STATUS_METRICS_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_METRICS_AW_CLIENT_SIDE_SAMPLING_STATUS_METRICS_PROVIDER_H_

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "base/memory/raw_ptr.h"
#include "components/metrics/metrics_provider.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace android_webview {

class AwClientSideSamplingStatusMetricsProvider
    : public ::metrics::MetricsProvider {
 public:
  AwClientSideSamplingStatusMetricsProvider();
  explicit AwClientSideSamplingStatusMetricsProvider(
      AwMetricsServiceClient* client);

  AwClientSideSamplingStatusMetricsProvider(
      const AwClientSideSamplingStatusMetricsProvider&) = delete;
  AwClientSideSamplingStatusMetricsProvider& operator=(
      const AwClientSideSamplingStatusMetricsProvider&) = delete;

  ~AwClientSideSamplingStatusMetricsProvider() override = default;

  void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile) override;

 private:
  bool IsClientSideSamplingDisabled();
  raw_ptr<AwMetricsServiceClient> client_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_METRICS_AW_CLIENT_SIDE_SAMPLING_STATUS_METRICS_PROVIDER_H_
