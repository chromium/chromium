// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_components_metrics_provider.h"

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/common/metrics/app_package_name_logging_rule.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace android_webview {

AwComponentsMetricsProvider::AwComponentsMetricsProvider(
    AwMetricsServiceClient* client)
    : client_(client) {
  DCHECK(client);
}

void AwComponentsMetricsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile) {
  // TODO(https://crbug.com/1227882): Add info about other components too.
  absl::optional<AppPackageNameLoggingRule> record =
      client_->GetCachedAppPackageNameLoggingRule();
  if (!record.has_value())
    return;

  auto* chrome_component = system_profile->add_chrome_component();
  chrome_component->set_component_id(
      metrics::
          SystemProfileProto_ComponentId_WEBVIEW_APPS_PACKAGE_NAMES_ALLOWLIST);
  chrome_component->set_version(record.value().GetVersion().GetString());
  // TODO(https://crbug.com/1228535): record the component's Omaha fingerprint.
}

}  // namespace android_webview