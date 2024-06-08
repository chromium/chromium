// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_metrics_service_client_delegate.h"

#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/debugging_metrics_provider.h"
#include "android_webview/browser/lifecycle/aw_contents_lifecycle_notifier.h"
#include "android_webview/browser/metrics/android_metrics_provider.h"
#include "android_webview/browser/metrics/aw_component_metrics_provider_delegate.h"
#include "android_webview/browser/metrics/aw_metrics_filtering_status_metrics_provider.h"
#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/browser/metrics/aw_server_side_allowlist_metrics_provider.h"
#include "android_webview/browser/metrics/renderer_process_metrics_provider.h"
#include "android_webview/browser/metrics/visibility_metrics_provider.h"
#include "android_webview/browser/page_load_metrics/aw_page_load_metrics_provider.h"
#include "android_webview/browser/tracing/aw_background_tracing_metrics_provider.h"
#include "components/metrics/component_metrics_provider.h"
#include "components/metrics/metrics_service.h"

namespace android_webview {

AwMetricsServiceClientDelegate::AwMetricsServiceClientDelegate() = default;
AwMetricsServiceClientDelegate::~AwMetricsServiceClientDelegate() = default;

void AwMetricsServiceClientDelegate::RegisterAdditionalMetricsProviders(
    metrics::MetricsService* service) {
  PrefService* local_state = AwBrowserProcess::GetInstance()->local_state();

  service->RegisterMetricsProvider(
      std::make_unique<AndroidMetricsProvider>(local_state));
  service->RegisterMetricsProvider(
      std::make_unique<AwPageLoadMetricsProvider>());
  service->RegisterMetricsProvider(std::make_unique<VisibilityMetricsProvider>(
      AwBrowserProcess::GetInstance()->visibility_metrics_logger()));
  service->RegisterMetricsProvider(
      std::make_unique<RendererProcessMetricsProvider>());
  service->RegisterMetricsProvider(
      std::make_unique<DebuggingMetricsProvider>());
  service->RegisterMetricsProvider(
      std::make_unique<metrics::ComponentMetricsProvider>(
          std::make_unique<AwComponentMetricsProviderDelegate>(
              AwMetricsServiceClient::GetInstance())));
  service->RegisterMetricsProvider(
      std::make_unique<tracing::AwBackgroundTracingMetricsProvider>());
  service->RegisterMetricsProvider(
      std::make_unique<AwServerSideAllowlistMetricsProvider>(
          AwMetricsServiceClient::GetInstance()));
  service->RegisterMetricsProvider(
      std::make_unique<AwMetricsFilteringStatusMetricsProvider>(
          AwMetricsServiceClient::GetInstance()));
}

void AwMetricsServiceClientDelegate::AddWebViewAppStateObserver(
    WebViewAppStateObserver* observer) {
  AwContentsLifecycleNotifier::GetInstance().AddObserver(observer);
}

bool AwMetricsServiceClientDelegate::HasAwContentsEverCreated() const {
  return AwContentsLifecycleNotifier::GetInstance()
      .has_aw_contents_ever_created();
}

}  // namespace android_webview
