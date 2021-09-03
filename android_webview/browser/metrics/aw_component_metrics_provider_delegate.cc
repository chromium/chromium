// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_component_metrics_provider_delegate.h"

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/common/components/aw_apps_package_names_allowlist_component_utils.h"
#include "android_webview/common/metrics/app_package_name_logging_rule.h"
#include "components/component_updater/component_updater_service.h"
#include "components/metrics/component_metrics_provider.h"

using component_updater::ComponentInfo;

namespace android_webview {

AwComponentMetricsProviderDelegate::AwComponentMetricsProviderDelegate(
    AwMetricsServiceClient* client)
    : client_(client) {
  DCHECK(client);
}

// The returned ComponentInfo have component's id and version as this is the
// only info WebView keeps about components.
// TODO(https://crbug.com/1228535): record the component's Omaha fingerprint.
std::vector<ComponentInfo> AwComponentMetricsProviderDelegate::GetComponents() {
  // TODO(https://crbug.com/1227882): Add info about other components too.
  std::vector<ComponentInfo> components;
  absl::optional<AppPackageNameLoggingRule> record =
      client_->GetCachedAppPackageNameLoggingRule();
  if (record.has_value()) {
    components.push_back(
        ComponentInfo(kWebViewAppsPackageNamesAllowlistComponentId, "",
                      std::u16string(), record.value().GetVersion()));
  }

  return components;
}

}  // namespace android_webview