// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_guest_actionable_mark_observer.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/timer/timer.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_embedder_interface.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace glic {

namespace {

constexpr std::string_view kInteractiveMarkName = "gemini.interactive";

class GlicGuestActionableMarkObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  explicit GlicGuestActionableMarkObserver(GlicActionableMarkCallback callback)
      : callback_(std::move(callback)) {}
  ~GlicGuestActionableMarkObserver() override = default;

  GlicGuestActionableMarkObserver(const GlicGuestActionableMarkObserver&) =
      delete;
  GlicGuestActionableMarkObserver& operator=(
      const GlicGuestActionableMarkObserver&) = delete;

  // page_load_metrics::PageLoadMetricsObserver:
  const char* GetObserverName() const override {
    return "GlicGuestActionableMarkObserver";
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
    for (const auto& mark : timings) {
      if (mark->mark_name == kInteractiveMarkName) {
        // `start_time` is relative to the navigation start of this page load.
        callback_.Run(GetDelegate().GetNavigationStart() + mark->start_time);
        return;
      }
    }
  }

 private:
  GlicActionableMarkCallback callback_;
};

// Minimal embedder for the Glic guest's `MetricsWebContentsObserver`. It only
// registers a `GlicGuestActionableMarkObserver`, so the guest's page loads
// don't feed any of the standard tab page-load metrics.
class GuestPageLoadMetricsEmbedder
    : public page_load_metrics::PageLoadMetricsEmbedderInterface {
 public:
  explicit GuestPageLoadMetricsEmbedder(GlicActionableMarkCallback callback)
      : callback_(std::move(callback)) {}
  ~GuestPageLoadMetricsEmbedder() override = default;

  // page_load_metrics::PageLoadMetricsEmbedderInterface:
  bool IsNewTabPageUrl(const GURL& url) override { return false; }
  void RegisterObservers(
      page_load_metrics::PageLoadTracker* tracker,
      content::NavigationHandle* navigation_handle) override {
    tracker->AddObserver(
        std::make_unique<GlicGuestActionableMarkObserver>(callback_));
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
  GlicActionableMarkCallback callback_;
};

}  // namespace

void ObserveGlicGuestActionableMark(content::WebContents& guest_contents,
                                    GlicActionableMarkCallback callback) {
  if (page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          &guest_contents)) {
    return;
  }
  page_load_metrics::MetricsWebContentsObserver::CreateForWebContents(
      &guest_contents,
      std::make_unique<GuestPageLoadMetricsEmbedder>(std::move(callback)));
}

}  // namespace glic
