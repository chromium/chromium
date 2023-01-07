// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_FIRST_WEB_CONTENTS_PROFILER_BASE_H_
#define CHROME_BROWSER_METRICS_FIRST_WEB_CONTENTS_PROFILER_BASE_H_

#include "base/time/time.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

class Browser;

namespace metrics {

// Reasons for which profiling is deemed complete. Logged in UMA (do not re-
// order or re-assign).
enum class StartupProfilingFinishReason {
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
  // Abandon if the WebContents was already painted. We set up the profiler too
  // late and it missed the first non empty paint event.
  kAbandonAlreadyPaintedContent = 7,
  kMaxValue = kAbandonAlreadyPaintedContent
};

// Note: Instances of this class self destroy when the first non-empty paint
// happens, or when an event prevents it from being recorded.
class FirstWebContentsProfilerBase : public content::WebContentsObserver {
 public:
  FirstWebContentsProfilerBase(const FirstWebContentsProfilerBase&) = delete;
  FirstWebContentsProfilerBase& operator=(const FirstWebContentsProfilerBase&) =
      delete;

  // Returns a visible webcontents from `browser` that can be observed for
  // startup profiling, or `nullptr` if no compatible one was obtained.
  static content::WebContents* GetVisibleContents(Browser* browser);

 protected:
  explicit FirstWebContentsProfilerBase(content::WebContents* web_contents);

  // Protected destructor as `FirstWebContentsProfilerBase` deletes itself.
  ~FirstWebContentsProfilerBase() override;

  // Whether to abort recording metrics if the main window startup was
  // interrupted. Recording metrics for startups with interruptions pollutes the
  // collected data, however some flows (e.g. startup on ProfilePicker)
  // specifically define their metrics to work around the interruptions.
  virtual bool WasStartupInterrupted() = 0;

  virtual void RecordFinishReason(
      StartupProfilingFinishReason finish_reason) = 0;
  virtual void RecordNavigationFinished(base::TimeTicks navigation_start) = 0;
  virtual void RecordFirstNonEmptyPaint() = 0;

 private:
  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFirstVisuallyNonEmptyPaint() override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;

  // Logs |finish_reason| to UMA and deletes this profiler.
  void FinishedCollectingMetrics(StartupProfilingFinishReason finish_reason);

  // Whether a main frame navigation finished since this was created.
  bool did_finish_first_navigation_ = false;
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_FIRST_WEB_CONTENTS_PROFILER_BASE_H_
