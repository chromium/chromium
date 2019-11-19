// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/first_web_contents_profiler.h"

#include <string>

#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace {

// Reasons for which profiling is deemed complete. Logged in UMA (do not re-
// order or re-assign).
enum class FinishReason {
  // All metrics were successfully gathered.
  kDone = 0,
  // Abandon if blocking UI was shown during startup.
  kAbandonBlockingUI = 1,
  // Abandon if the WebContents is hidden (lowers scheduling priority).
  kAbandonContentHidden = 2,
  // Abandon if the WebContents is destroyed.
  kAbandonContentDestroyed = 3,
  // Abandon if the WebContents navigates away from its initial page.
  kAbandonNewNavigation = 4,
  // Abandon if the WebContents fails to load (e.g. network error, etc.).
  kAbandonNavigationError = 5,
  // Abandon if no WebContents was visible at the beginning of startup
  kAbandonNoInitiallyVisibleContent = 6,
  kMaxValue = kAbandonNoInitiallyVisibleContent
};

void RecordFinishReason(FinishReason finish_reason) {
  UMA_HISTOGRAM_ENUMERATION("Startup.FirstWebContents.FinishReason",
                            finish_reason);
}

class FirstWebContentsProfiler : public content::WebContentsObserver {
 public:
  FirstWebContentsProfiler(content::WebContents* web_contents,
                           startup_metric_utils::WebContentsWorkload workload);

 private:
  // Steps of main frame navigation in a WebContents.
  enum class NavigationStep {
    // DidStartNavigation() is invalid
    // DidFinishNavigation() transitions to kNavigationFinished
    // DidFirstVisuallyNonEmptyPaint() is invalid
    kNavigationStarted,

    // DidStartNavigation() stops profiling with kAbandonNewNavigation
    // DidFinishNavigation() is invalid
    // DidFirstVisuallyNonEmptyPaint() stops profiling with kDone
    kNavigationFinished,
  };

  ~FirstWebContentsProfiler() override = default;

  // content::WebContentsObserver:
  void DidFirstVisuallyNonEmptyPaint() override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;

  // Logs |finish_reason| to UMA and deletes this FirstWebContentsProfiler.
  void FinishedCollectingMetrics(FinishReason finish_reason);

  const startup_metric_utils::WebContentsWorkload workload_;
  NavigationStep navigation_step_ = NavigationStep::kNavigationStarted;

  DISALLOW_COPY_AND_ASSIGN(FirstWebContentsProfiler);
};

FirstWebContentsProfiler::FirstWebContentsProfiler(
    content::WebContents* web_contents,
    startup_metric_utils::WebContentsWorkload workload)
    : content::WebContentsObserver(web_contents), workload_(workload) {
  // FirstWebContentsProfiler is always created with a WebContents that started
  // but did not finish navigating.
  DCHECK(web_contents->GetController().GetPendingEntry());
}

void FirstWebContentsProfiler::DidFirstVisuallyNonEmptyPaint() {
  DCHECK_EQ(navigation_step_, NavigationStep::kNavigationFinished);

  if (startup_metric_utils::WasMainWindowStartupInterrupted()) {
    FinishedCollectingMetrics(FinishReason::kAbandonBlockingUI);
    return;
  }

  startup_metric_utils::RecordFirstWebContentsNonEmptyPaint(
      base::TimeTicks::Now(), web_contents()
                                  ->GetMainFrame()
                                  ->GetProcess()
                                  ->GetInitTimeForNavigationMetrics());

  FinishedCollectingMetrics(FinishReason::kDone);
}

void FirstWebContentsProfiler::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Ignore subframe navigations and same-document navigations.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // DidFinishNavigation() should have been called to finish the preceding
  // navigation.
  DCHECK_EQ(navigation_step_, NavigationStep::kNavigationFinished);

  // Abandon profiling on a top-level navigation to a different page as it:
  //   (1) is no longer a fair timing; and
  //   (2) can cause http://crbug.com/525209 where the first paint didn't fire
  //       for the initial content but fires after a lot of idle time when the
  //       user finally navigates to another page that does trigger it.
  FinishedCollectingMetrics(FinishReason::kAbandonNewNavigation);
}

void FirstWebContentsProfiler::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Ignore subframe navigations and same-document navigations.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  DCHECK_EQ(navigation_step_, NavigationStep::kNavigationStarted);
  navigation_step_ = NavigationStep::kNavigationFinished;

  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsErrorPage()) {
    FinishedCollectingMetrics(FinishReason::kAbandonNavigationError);
    return;
  }

  if (startup_metric_utils::WasMainWindowStartupInterrupted()) {
    FinishedCollectingMetrics(FinishReason::kAbandonBlockingUI);
    return;
  }

  startup_metric_utils::RecordFirstWebContentsMainNavigationStart(
      navigation_handle->NavigationStart(), workload_);
  startup_metric_utils::RecordFirstWebContentsMainNavigationFinished(
      base::TimeTicks::Now());
}

void FirstWebContentsProfiler::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility != content::Visibility::VISIBLE) {
    // Stop profiling if the content gets hidden as its load may be
    // deprioritized and timing it becomes meaningless.
    FinishedCollectingMetrics(FinishReason::kAbandonContentHidden);
  }
}

void FirstWebContentsProfiler::WebContentsDestroyed() {
  FinishedCollectingMetrics(FinishReason::kAbandonContentDestroyed);
}

void FirstWebContentsProfiler::FinishedCollectingMetrics(
    FinishReason finish_reason) {
  RecordFinishReason(finish_reason);
  delete this;
}

}  // namespace

namespace metrics {

void BeginFirstWebContentsProfiling() {
  using startup_metric_utils::WebContentsWorkload;

  const BrowserList* browser_list = BrowserList::GetInstance();

  Browser* visible_browser = nullptr;
  for (Browser* browser : *browser_list) {
    if (browser->window()->IsVisible()) {
      visible_browser = browser;
      break;
    }
  }

  if (!visible_browser) {
    RecordFinishReason(FinishReason::kAbandonNoInitiallyVisibleContent);
    return;
  }

  const TabStripModel* tab_strip = visible_browser->tab_strip_model();
  DCHECK(!tab_strip->empty());

  content::WebContents* web_contents = tab_strip->GetActiveWebContents();
  DCHECK(web_contents);
  DCHECK_EQ(web_contents->GetVisibility(), content::Visibility::VISIBLE);

  const bool single_tab = browser_list->size() == 1 && tab_strip->count() == 1;

  // FirstWebContentsProfiler owns itself and is also bound to
  // |web_contents|'s lifetime by observing WebContentsDestroyed().
  new FirstWebContentsProfiler(web_contents,
                               single_tab ? WebContentsWorkload::SINGLE_TAB
                                          : WebContentsWorkload::MULTI_TABS);
}

}  // namespace metrics
