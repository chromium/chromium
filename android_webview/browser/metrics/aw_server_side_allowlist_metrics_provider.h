// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_METRICS_AW_SERVER_SIDE_ALLOWLIST_METRICS_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_METRICS_AW_SERVER_SIDE_ALLOWLIST_METRICS_PROVIDER_H_

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "base/memory/raw_ptr.h"
#include "components/metrics/metrics_provider.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace android_webview {

class AwServerSideAllowlistMetricsProvider : public ::metrics::MetricsProvider {
 public:
  AwServerSideAllowlistMetricsProvider();
  explicit AwServerSideAllowlistMetricsProvider(AwMetricsServiceClient* client);

  AwServerSideAllowlistMetricsProvider(
      const AwServerSideAllowlistMetricsProvider&) = delete;
  AwServerSideAllowlistMetricsProvider& operator=(
      const AwServerSideAllowlistMetricsProvider&) = delete;

  ~AwServerSideAllowlistMetricsProvider() override = default;

  void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile) override;

 private:
  metrics::AndroidMetricsServiceClient::InstallerPackageType
  GetInstallerPackageType();

  bool IsAppPackageNameSystemApp();

  raw_ptr<AwMetricsServiceClient> client_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_METRICS_AW_SERVER_SIDE_ALLOWLIST_METRICS_PROVIDER_H_