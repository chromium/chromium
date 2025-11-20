// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_INITIAL_WEBUI_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_INITIAL_WEBUI_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "url/gurl.h"

class MetricsReporter;
class WaapUIMetricsService;

namespace content {
class NavigationHandle;
}  // namespace content

// The metrics observer for page loads of InitialWebUI.
//
// A InitialWebUI is a WebUI shown in the top chrome UI, e.g. toolbar. It is
// different from the normal WebUI which is shown in the content area, e.g.
// New Tab Page.
//
// See
// https://docs.google.com/document/d/13nVm0v4hKFfTjbsE0n7loh3seBdRmqyLXByZqjlpc8Q/edit?tab=t.0
class InitialWebUIPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  InitialWebUIPageLoadMetricsObserver();

  // Not movable or copyable.
  InitialWebUIPageLoadMetricsObserver(
      const InitialWebUIPageLoadMetricsObserver&) = delete;
  InitialWebUIPageLoadMetricsObserver& operator=(
      const InitialWebUIPageLoadMetricsObserver&) = delete;

  ~InitialWebUIPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  const char* GetObserverName() const override;
  void OnMonotonicFirstPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnMonotonicFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnUserInput(
      const blink::WebInputEvent& event,
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy ShouldObserveScheme(const GURL& url) const override;

 private:
  // Returns the service for the current profile.
  // The service is guaranteed to be non-null.
  WaapUIMetricsService* service() const;

  // Returns the MetricsReporter for the current WebContents.
  // The MetricsReporter is tighted to WebContents, and so is this observer.
  // Thus the MetricsReporter is guaranteed to be non-null.
  MetricsReporter& GetMetricsReporter();
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_INITIAL_WEBUI_PAGE_LOAD_METRICS_OBSERVER_H_
