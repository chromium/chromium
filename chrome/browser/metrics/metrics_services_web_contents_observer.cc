// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_services_web_contents_observer.h"

#include "chrome/browser/browser_process.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"

namespace metrics {

MetricsServicesWebContentsObserver::MetricsServicesWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<MetricsServicesWebContentsObserver>(
          *web_contents) {}

MetricsServicesWebContentsObserver::~MetricsServicesWebContentsObserver() =
    default;

void MetricsServicesWebContentsObserver::DidStartLoading() {
  auto* manager = g_browser_process->GetMetricsServicesManager();
  if (manager)
    manager->LoadingStateChanged(/*is_loading=*/true);
}

void MetricsServicesWebContentsObserver::DidStopLoading() {
  auto* manager = g_browser_process->GetMetricsServicesManager();
  if (manager)
    manager->LoadingStateChanged(/*is_loading=*/false);
}

void MetricsServicesWebContentsObserver::OnRendererUnresponsive(
    content::RenderProcessHost* host) {
  auto* manager = g_browser_process->GetMetricsServicesManager();
  if (manager)
    manager->GetMetricsService()->OnApplicationNotIdle();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MetricsServicesWebContentsObserver);

}  // namespace metrics
