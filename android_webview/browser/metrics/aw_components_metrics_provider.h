// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_METRICS_AW_COMPONENTS_METRICS_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_METRICS_AW_COMPONENTS_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

namespace metrics {
class SystemProfileProto;
}  // namespace metrics

namespace android_webview {

class AwMetricsServiceClient;

// Add info about the components loaded in the system to the system profile
// logs.
class AwComponentsMetricsProvider : public metrics::MetricsProvider {
 public:
  explicit AwComponentsMetricsProvider(AwMetricsServiceClient* client);
  ~AwComponentsMetricsProvider() override = default;

  // MetricsProvider:
  void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;

 private:
  AwMetricsServiceClient* client_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_METRICS_AW_COMPONENTS_METRICS_PROVIDER_H_
