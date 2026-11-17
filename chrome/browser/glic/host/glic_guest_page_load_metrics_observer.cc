// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_guest_page_load_metrics_observer.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_embedder_interface.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace glic {

namespace {

class GlicGuestPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  explicit GlicGuestPageLoadMetricsObserver(
      GlicGuestTimingMarkCallback callback)
      : callback_(std::move(callback)) {}
  ~GlicGuestPageLoadMetricsObserver() override = default;

  GlicGuestPageLoadMetricsObserver(const GlicGuestPageLoadMetricsObserver&) =
      delete;
  GlicGuestPageLoadMetricsObserver& operator=(
      const GlicGuestPageLoadMetricsObserver&) = delete;

  // page_load_metrics::PageLoadMetricsObserver:
  const char* GetObserverName() const override {
    return "GlicGuestPageLoadMetricsObserver";
  }
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override {
    return STOP_OBSERVING;
  }
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override {
    return STOP_OBSERVING;
  }
  void OnCustomUserTimingMarkObserved(
      const std::vector<page_load_metrics::mojom::CustomUserTimingMarkPtr>&
          timings) override {
    const base::TimeTicks nav_start = GetDelegate().GetNavigationStart();
    for (const auto& mark : timings) {
      // `start_time` is relative to the navigation start of this page load.
      callback_.Run(mark->mark_name, nav_start + mark->start_time);
    }
  }

 private:
  GlicGuestTimingMarkCallback callback_;
};

// Minimal embedder for the Glic guest's `MetricsWebContentsObserver`. It only
// registers a `GlicGuestPageLoadMetricsObserver`, so the guest's page loads
// don't feed any of the standard tab page-load metrics.
class GuestPageLoadMetricsEmbedder
    : public page_load_metrics::PageLoadMetricsEmbedderInterface {
 public:
  explicit GuestPageLoadMetricsEmbedder(GlicGuestTimingMarkCallback callback)
      : callback_(std::move(callback)) {}
  ~GuestPageLoadMetricsEmbedder() override = default;

  GuestPageLoadMetricsEmbedder(const GuestPageLoadMetricsEmbedder&) = delete;
  GuestPageLoadMetricsEmbedder& operator=(const GuestPageLoadMetricsEmbedder&) =
      delete;

  // page_load_metrics::PageLoadMetricsEmbedderInterface:
  bool IsNewTabPageUrl(const GURL& url) override { return false; }
  void RegisterObservers(
      page_load_metrics::PageLoadTracker* tracker,
      content::NavigationHandle* navigation_handle) override {
    tracker->AddObserver(
        std::make_unique<GlicGuestPageLoadMetricsObserver>(callback_));
  }
  page_load_metrics::NavigationScenario GetNavigationScenario(
      content::NavigationHandle* navigation_handle) const override {
    return page_load_metrics::NavigationScenario::kUnknown;
  }
  std::unique_ptr<base::OneShotTimer> CreateTimer() override {
    return std::make_unique<base::OneShotTimer>();
  }
  bool HasWebUIConfig(const GURL& url) override { return false; }
  bool IsNoStatePrefetch(content::WebContents* web_contents) override {
    return false;
  }
  bool IsExtensionUrl(const GURL& url) override { return false; }
  bool IsNonTabWebUI(const GURL& url) override { return false; }
  bool IsInternalWebUI(const GURL& url) override { return false; }
  bool ShouldObserveScheme(std::string_view scheme) override { return false; }

 private:
  GlicGuestTimingMarkCallback callback_;
};

}  // namespace

void ObserveGlicGuestPageLoadMetrics(content::WebContents& guest_contents,
                                     GlicGuestTimingMarkCallback callback) {
  CHECK(callback);
  if (page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          &guest_contents)) {
    return;
  }
  page_load_metrics::MetricsWebContentsObserver::CreateForWebContents(
      &guest_contents,
      std::make_unique<GuestPageLoadMetricsEmbedder>(std::move(callback)));
}

}  // namespace glic
