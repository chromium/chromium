// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_METRICS_AW_METRICS_FILTERING_STATUS_METRICS_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_METRICS_AW_METRICS_FILTERING_STATUS_METRICS_PROVIDER_H_

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "base/memory/raw_ptr.h"
#include "components/metrics/metrics_provider.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace android_webview {

class AwMetricsFilteringStatusMetricsProvider
    : public ::metrics::MetricsProvider {
 public:
  AwMetricsFilteringStatusMetricsProvider();
  explicit AwMetricsFilteringStatusMetricsProvider(
      AwMetricsServiceClient* client);

  AwMetricsFilteringStatusMetricsProvider(
      const AwMetricsFilteringStatusMetricsProvider&) = delete;
  AwMetricsFilteringStatusMetricsProvider& operator=(
      const AwMetricsFilteringStatusMetricsProvider&) = delete;

  ~AwMetricsFilteringStatusMetricsProvider() override = default;

  void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile) override;

 private:
  raw_ptr<AwMetricsServiceClient> client_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_METRICS_AW_METRICS_FILTERING_STATUS_METRICS_PROVIDER_H_
