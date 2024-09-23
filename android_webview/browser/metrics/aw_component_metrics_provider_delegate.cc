// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_component_metrics_provider_delegate.h"

#include <algorithm>
#include <string>
#include <vector>

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "components/component_updater/android/components_info_holder.h"
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
// TODO(crbug.com/40777796): record the component's Omaha fingerprint.
std::vector<ComponentInfo> AwComponentMetricsProviderDelegate::GetComponents() {
  return component_updater::ComponentsInfoHolder::GetInstance()
      ->GetComponents();
}

}  // namespace android_webview
