// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/first_web_contents_profiler.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/check.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/metrics/histogram_functions.h"
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
  // Abandon if the WebContents navigates away from its initial page, as it:
  //   (1) is no longer a fair timing; and
  //   (2) can cause http://crbug.com/525209 where the first paint didn't fire
  //       for the initial content but fires after a lot of idle time when the
  //       user finally navigates to another page that does trigger it.
  kAbandonNewNavigation = 4,
  // Abandon if the WebContents fails to load (e.g. network error, etc.).
  kAbandonNavigationError = 5,
  // Abandon if no WebContents was visible at the beginning of startup
  kAbandonNoInitiallyVisibleContent = 6,
  kMaxValue = kAbandonNoInitiallyVisibleContent
};

void RecordFinishReason(FinishReason finish_reason) {
  base::UmaHistogramEnumeration("Startup.FirstWebContents.FinishReason",
                                finish_reason);
}

// Note: Instances of this class self destroy when the first non-empty paint
// happens, or when an event prevents it from being recorded.
class FirstWebContentsProfiler : public content::WebContentsObserver {
 public:
  explicit FirstWebContentsProfiler(content::WebContents* web_contents);

 private:
  ~FirstWebContentsProfiler() override = default;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFirstVisuallyNonEmptyPaint() override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;

  // Logs |finish_reason| to UMA and deletes this FirstWebContentsProfiler.
  void FinishedCollectingMetrics(FinishReason finish_reason);

  // Whether a main frame navigation finished since this was created.
  bool did_finish_first_navigation_ = false;

  // Memory pressure listener that will be used to check if memory pressure has
  // an impact on startup.
  base::MemoryPressureListener memory_pressure_listener_;

  DISALLOW_COPY_AND_ASSIGN(FirstWebContentsProfiler);
};

FirstWebContentsProfiler::FirstWebContentsProfiler(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      memory_pressure_listener_(
          FROM_HERE,
          base::BindRepeating(&startup_metric_utils::
                                  OnMemoryPressureBeforeFirstNonEmptyPaint)) {
  // FirstWebContentsProfiler is created before the main MessageLoop starts
  // running. At that time, any visible WebContents should have a pending
  // NavigationEntry, i.e. should have dispatched DidStartNavigation() but not
  // DidFinishNavigation().
  DCHECK(web_contents->GetController().GetPendingEntry());
}

void FirstWebContentsProfiler::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Ignore subframe navigations and same-document navigations.
  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main frame
  // to preserve its semantics. Follow up to confirm correctness.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // FirstWebContentsProfiler is created after DidStartNavigation() has been
  // dispatched for the first top-level navigation. If another
  // DidStartNavigation() is received, it means that a new navigation was
  // initiated.
  FinishedCollectingMetrics(FinishReason::kAbandonNewNavigation);
}

void FirstWebContentsProfiler::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (startup_metric_utils::WasMainWindowStartupInterrupted()) {
    FinishedCollectingMetrics(FinishReason::kAbandonBlockingUI);
    return;
  }

  // Ignore subframe navigations and same-document navigations.
  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main frame
  // to preserve its semantics. Follow up to confirm correctness.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsErrorPage()) {
    FinishedCollectingMetrics(FinishReason::kAbandonNavigationError);
    return;
  }

  // It is not possible to get a second top-level DidFinishNavigation() without
  // first having a DidStartNavigation(), which would have deleted |this|.
  DCHECK(!did_finish_first_navigation_);

  did_finish_first_navigation_ = true;

  startup_metric_utils::RecordFirstWebContentsMainNavigationStart(
      navigation_handle->NavigationStart());
  startup_metric_utils::RecordFirstWebContentsMainNavigationFinished(
      base::TimeTicks::Now());
}

void FirstWebContentsProfiler::DidFirstVisuallyNonEmptyPaint() {
  DCHECK(did_finish_first_navigation_);

  if (startup_metric_utils::WasMainWindowStartupInterrupted()) {
    FinishedCollectingMetrics(FinishReason::kAbandonBlockingUI);
    return;
  }

  startup_metric_utils::RecordFirstWebContentsNonEmptyPaint(
      base::TimeTicks::Now(),
      web_contents()->GetMainFrame()->GetProcess()->GetLastInitTime());

  FinishedCollectingMetrics(FinishReason::kDone);
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
  const BrowserList* browser_list = BrowserList::GetInstance();

  content::WebContents* visible_contents = nullptr;
  for (Browser* browser : *browser_list) {
    if (!browser->window()->IsVisible())
      continue;

    // The active WebContents may be hidden when the window height is small.
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();

#if defined(OS_MAC)
    // TODO(https://crbug.com/1032348): It is incorrect to have a visible
    // browser window with no active WebContents, but reports on Mac show that
    // it happens.
    if (!contents)
      continue;
#endif  // defined(OS_MAC)

    if (contents->GetVisibility() != content::Visibility::VISIBLE)
      continue;

    visible_contents = contents;
    break;
  }

  if (!visible_contents) {
    RecordFinishReason(FinishReason::kAbandonNoInitiallyVisibleContent);
    return;
  }

  // FirstWebContentsProfiler owns itself and is also bound to
  // |visible_contents|'s lifetime by observing WebContentsDestroyed().
  new FirstWebContentsProfiler(visible_contents);
}

}  // namespace metrics
