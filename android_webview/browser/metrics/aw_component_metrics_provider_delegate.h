// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_METRICS_AW_COMPONENT_METRICS_PROVIDER_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_METRICS_AW_COMPONENT_METRICS_PROVIDER_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "components/metrics/component_metrics_provider.h"

namespace component_updater {
struct ComponentInfo;
}  // namespace component_updater

namespace android_webview {

class AwMetricsServiceClient;

// WebView delegate to provide WebView's own list of loaded components to be
// recorded in the system profile UMA log. Unlike chrome, WebView doesn't use
// `component_updater::ComponentUpdateService` to load or keep track of
// components.
class AwComponentMetricsProviderDelegate
    : public metrics::ComponentMetricsProviderDelegate {
 public:
  explicit AwComponentMetricsProviderDelegate(AwMetricsServiceClient* client);
  ~AwComponentMetricsProviderDelegate() override = default;

  // ComponentsInfoProvider:
  std::vector<component_updater::ComponentInfo> GetComponents() override;

 private:
  raw_ptr<AwMetricsServiceClient> client_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_METRICS_AW_COMPONENT_METRICS_PROVIDER_DELEGATE_H_
