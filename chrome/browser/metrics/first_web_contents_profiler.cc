// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/first_web_contents_profiler.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/metrics/first_web_contents_profiler_base.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "pdf/buildflags.h"

namespace metrics {
namespace {

void RecordFirstWebContentsFinishReason(
    StartupProfilingFinishReason finish_reason) {
  base::UmaHistogramEnumeration("Startup.FirstWebContents.FinishReason",
                                finish_reason);
}

class FirstWebContentsProfiler : public FirstWebContentsProfilerBase {
 public:
  explicit FirstWebContentsProfiler(content::WebContents* web_contents);

  FirstWebContentsProfiler(const FirstWebContentsProfiler&) = delete;
  FirstWebContentsProfiler& operator=(const FirstWebContentsProfiler&) = delete;

 protected:
  // FirstWebContentsProfilerBase:
  void RecordFinishReason(StartupProfilingFinishReason finish_reason) override;
  void RecordNavigationFinished(base::TimeTicks navigation_start) override;
  void RecordFirstNonEmptyPaint() override;
  void RecordFirstNonEmptyPaintForOsLaunch() override;
  void RecordFirstContentfulPaint(base::TimeTicks fcp_ticks) override;
  void RecordLargestContentfulPaint(base::TimeTicks lcp_ticks) override;
#if BUILDFLAG(ENABLE_PDF)
  void RecordPdfFirstContentPaint(base::TimeTicks pdf_paint_ticks) override;
#endif  // BUILDFLAG(ENABLE_PDF)
  bool WasStartupInterrupted() override;
  bool ShouldObservePaintTimingMetrics() override;

 private:
  ~FirstWebContentsProfiler() override = default;

  std::unique_ptr<AfterStartupTaskUtils::StartupInProgressRef>
      startup_in_progress_ref_;
};

// FirstWebContentsProfiler is created before the main MessageLoop starts
// running. It is generally expected that any visible WebContents at this
// time will have a pending NavigationEntry, i.e. should have dispatched
// DidStartNavigation() but notDidFinishNavigation().
//
// This assumption does not hold true on ChromeOS during a session restore,
// where the profiler is created before the navigation starts. The base
// class (`FirstWebContentsProfilerBase`) contains the logic to handle this
// specific case.
//
// This seemingly empty constructor is necessary. It serves as a public
// entry point to call the protected constructor of the base class, which is
// required because this class is instantiated by the free function
// `BeginFirstWebContentsProfiling`.
FirstWebContentsProfiler::FirstWebContentsProfiler(
    content::WebContents* web_contents)
    : FirstWebContentsProfilerBase(web_contents) {
  if (base::FeatureList::IsEnabled(features::kImprovedStartupBestEffortDelay)) {
    startup_in_progress_ref_ =
        AfterStartupTaskUtils::RegisterStartupInProgressRef(
            StartupIsCompleteReason::kFirstWebContentsProfiler);
  }
}

void FirstWebContentsProfiler::RecordFinishReason(
    StartupProfilingFinishReason finish_reason) {
  RecordFirstWebContentsFinishReason(finish_reason);
  // Release the startup-in-progress reference now that first-web-contents
  // profiling has reached its terminal decision (first paint recorded or
  // abandoned). This profiler may keep observing for FCP/LCP after a successful
  // paint, but browser startup completion (and thus BEST_EFFORT task
  // scheduling) must not be blocked on that extended observation.
  startup_in_progress_ref_.reset();
}

void FirstWebContentsProfiler::RecordNavigationFinished(
    base::TimeTicks navigation_start) {
  startup_metric_utils::GetBrowser().RecordFirstWebContentsMainNavigationStart(
      navigation_start);
  startup_metric_utils::GetBrowser()
      .RecordFirstWebContentsMainNavigationFinished(base::TimeTicks::Now());
}

void FirstWebContentsProfiler::RecordFirstNonEmptyPaint() {
  startup_metric_utils::GetBrowser().RecordFirstWebContentsNonEmptyPaint(
      base::TimeTicks::Now(),
      web_contents()->GetPrimaryMainFrame()->GetProcess()->GetLastInitTime());
}

void FirstWebContentsProfiler::RecordFirstNonEmptyPaintForOsLaunch() {
  startup_metric_utils::GetBrowser()
      .RecordFirstWebContentsNonEmptyPaintForOsLaunch(base::TimeTicks::Now());
}

void FirstWebContentsProfiler::RecordFirstContentfulPaint(
    base::TimeTicks fcp_ticks) {
  startup_metric_utils::GetBrowser().RecordFirstWebContentsFirstContentfulPaint(
      fcp_ticks);
}

void FirstWebContentsProfiler::RecordLargestContentfulPaint(
    base::TimeTicks lcp_ticks) {
  startup_metric_utils::GetBrowser()
      .RecordFirstWebContentsLargestContentfulPaint(lcp_ticks);
}

#if BUILDFLAG(ENABLE_PDF)
void FirstWebContentsProfiler::RecordPdfFirstContentPaint(
    base::TimeTicks pdf_paint_ticks) {
  startup_metric_utils::GetBrowser().RecordFirstWebContentsPdfFirstContentPaint(
      pdf_paint_ticks);
}
#endif  // BUILDFLAG(ENABLE_PDF)

bool FirstWebContentsProfiler::WasStartupInterrupted() {
  return startup_metric_utils::GetBrowser().WasMainWindowStartupInterrupted();
}

bool FirstWebContentsProfiler::ShouldObservePaintTimingMetrics() {
  return true;
}

}  // namespace

// Determines the WebContents to profile for startup metrics.
// This is typically the currently visible WebContents. However, if window
// visibility is deferred, e.g., waiting for the WebUI toolbar to finish
// loading, it instead returns the active WebContents without relying on a
// strict visibility check.
content::WebContents* GetWebContentsToProfile(
    BrowserWindowInterface* browser_interface) {
  content::WebContents* visible_contents =
      FirstWebContentsProfilerBase::GetVisibleContents(browser_interface);
  if (visible_contents) {
    return visible_contents;
  }

  const auto* manager = InitialWebUIManager::From(browser_interface);
  if (manager && manager->IsShowPending()) {
    return browser_interface->GetTabStripModel()->GetActiveWebContents();
  }

  return nullptr;
}

void BeginFirstWebContentsProfiling() {
  content::WebContents* visible_contents = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (features::kWebUIReloadButtonDeferBrowserViewShow.Get()) {
          visible_contents = GetWebContentsToProfile(browser);
        } else {
          visible_contents =
              FirstWebContentsProfilerBase::GetVisibleContents(browser);
        }
        if (visible_contents) {
          return false;  // stop iterating
        }
        return true;  // continue iterating
      });

  if (!visible_contents) {
    RecordFirstWebContentsFinishReason(
        StartupProfilingFinishReason::kAbandonNoInitiallyVisibleContent);
    return;
  }

  // FirstWebContentsProfiler owns itself and is also bound to
  // |visible_contents|'s lifetime by observing WebContentsDestroyed().
  new FirstWebContentsProfiler(visible_contents);
}

}  // namespace metrics
