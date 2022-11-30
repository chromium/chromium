// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_METRICS_SERVICE_CLIENT_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_METRICS_SERVICE_CLIENT_DELEGATE_H_

#include "android_webview/browser/metrics/aw_metrics_service_client.h"

namespace android_webview {

// Interceptor to handle urls for media assets in the apk.
class AwMetricsServiceClientDelegate : public AwMetricsServiceClient::Delegate {
 public:
  AwMetricsServiceClientDelegate();
  ~AwMetricsServiceClientDelegate() override;

  // AwMetricsServiceClient::Delegate
  void RegisterAdditionalMetricsProviders(
      metrics::MetricsService* service) override;
  void AddWebViewAppStateObserver(WebViewAppStateObserver* observer) override;
  bool HasAwContentsEverCreated() const override;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_METRICS_SERVICE_CLIENT_DELEGATE_H_