// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_server_side_allowlist_metrics_provider.h"

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/common/aw_features.h"
#include "components/embedder_support/android/metrics/android_metrics_service_client.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace android_webview {
AwServerSideAllowlistMetricsProvider::AwServerSideAllowlistMetricsProvider()
    : client_(nullptr) {}

AwServerSideAllowlistMetricsProvider::AwServerSideAllowlistMetricsProvider(
    AwMetricsServiceClient* client)
    : client_(client) {}

void AwServerSideAllowlistMetricsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile) {

  if (IsAppPackageNameSystemApp()) {
    system_profile->set_app_package_name_allowlist_filter(
        metrics::SystemProfileProto::
            NO_SERVER_SIDE_FILTER_REQUIRED_FOR_SYSTEM_APPS);
  } else {
    system_profile->set_app_package_name_allowlist_filter(
        metrics::SystemProfileProto::SERVER_SIDE_FILTER_REQUIRED);
  }
}

bool AwServerSideAllowlistMetricsProvider::IsAppPackageNameSystemApp() {
  return GetInstallerPackageType() ==
         metrics::AndroidMetricsServiceClient::InstallerPackageType::SYSTEM_APP;
}

metrics::AndroidMetricsServiceClient::InstallerPackageType
AwServerSideAllowlistMetricsProvider::GetInstallerPackageType() {
  DCHECK(client_);
  return client_->GetInstallerPackageType();
}

}  // namespace android_webview
