// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_FIRST_WEB_CONTENTS_PROFILER_BASE_H_
#define CHROME_BROWSER_METRICS_FIRST_WEB_CONTENTS_PROFILER_BASE_H_

#include "base/time/time.h"
#include "content/public/browser/web_contents_observer.h"
#include "pdf/buildflags.h"

#if BUILDFLAG(ENABLE_PDF)
#include "base/callback_list.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace content {
class WebContents;
}

class BrowserWindowInterface;

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
  //   (2) can cause http://crbug.com/41197629 where the first paint didn't fire
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
  // Abandon if launched without user interaction (eg. launched by OS).
  kAbandonNonInteractiveStartup = 8,
  kMaxValue = kAbandonNonInteractiveStartup,
};

// Note: Instances of this class self destroy. For profilers that do not observe
// paint timing metrics (see `ShouldObservePaintTimingMetrics()`), this happens
// when the first non-empty paint happens, or when an event prevents it from
// being recorded. For profilers that do observe paint timing metrics, the first
// non-empty paint records its metric but the profiler keeps observing to
// capture the first contentful paint and the final largest contentful paint;
// it self destructs when the page is hidden, a new navigation starts, or the
// contents is destroyed.
class FirstWebContentsProfilerBase : public content::WebContentsObserver {
 public:
  FirstWebContentsProfilerBase(const FirstWebContentsProfilerBase&) = delete;
  FirstWebContentsProfilerBase& operator=(const FirstWebContentsProfilerBase&) =
      delete;

  // Returns a visible webcontents from `browser` that can be observed for
  // startup profiling, or `nullptr` if no compatible one was obtained.
  static content::WebContents* GetVisibleContents(
      BrowserWindowInterface* browser);

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
  virtual void RecordFirstNonEmptyPaintForOsLaunch() = 0;

  // Records the first contentful paint of the profiled WebContents. Only called
  // when `ShouldObservePaintTimingMetrics()` returns true. |fcp_ticks| is the
  // renderer-side presentation timestamp of the first contentful paint.
  virtual void RecordFirstContentfulPaint(base::TimeTicks fcp_ticks) {}

  // Records the largest contentful paint of the profiled WebContents. Only
  // called when `ShouldObservePaintTimingMetrics()` returns true. |lcp_ticks|
  // is the renderer-side presentation timestamp of the latest largest
  // contentful paint candidate.
  virtual void RecordLargestContentfulPaint(base::TimeTicks lcp_ticks) {}

#if BUILDFLAG(ENABLE_PDF)
  // Records the first paint of document content by the PDF plugin embedded in
  // the profiled WebContents. Only called when
  // `ShouldObservePaintTimingMetrics()` returns true. |pdf_paint_ticks| is the
  // renderer-side timestamp of that paint.
  virtual void RecordPdfFirstContentPaint(base::TimeTicks pdf_paint_ticks) {}
#endif  // BUILDFLAG(ENABLE_PDF)

  // Whether this profiler should keep observing past the first non-empty paint
  // to record the first contentful paint and the final largest contentful
  // paint. Defaults to false; profilers that record FCP/LCP override this.
  virtual bool ShouldObservePaintTimingMetrics();

 private:
  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFirstVisuallyNonEmptyPaint() override;
  void OnFirstContentfulPaintInPrimaryMainFrame(
      base::TimeTicks presentation_time) override;
  void OnLargestContentfulPaintInPrimaryMainFrame(
      base::TimeTicks presentation_time) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;

  // Records the first contentful paint if it has been observed and the profiler
  // has committed to recording paint timing metrics.
  void MaybeRecordFirstContentfulPaint();

  // Records the latest observed largest contentful paint candidate, if any.
  void MaybeRecordLargestContentfulPaint();

#if BUILDFLAG(ENABLE_PDF)
  // Invoked for every PDF in the process that reports a first content paint,
  // so it filters down to the contents being profiled.
  void OnPdfFirstContentPainted(content::WebContents* embedder,
                                base::TimeTicks paint_time);

  // Records the PDF first content paint once it has been observed and the
  // profiler has committed to recording paint timing metrics.
  void MaybeRecordPdfFirstContentPaint();
#endif  // BUILDFLAG(ENABLE_PDF)

  // Logs |finish_reason| to UMA (unless already logged) and deletes this
  // profiler. When paint timing metrics are being observed, the final largest
  // contentful paint is recorded before deletion.
  void FinishedCollectingMetrics(StartupProfilingFinishReason finish_reason);

  // Whether a main frame navigation finished since this was created.
  bool did_finish_first_navigation_ = false;

  // Whether a navigation was pending at the time this profiler was constructed,
  // or if a navigation start was observed post-construction.
  bool first_navigation_started_ = false;

  // Whether the finish reason has been recorded. When observing paint
  // timing metrics, this is set at the first non-empty paint (with kDone)
  // while the profiler keeps observing for FCP/LCP. Once set, a later
  // terminal event (a new navigation, the tab being hidden/deprioritized,
  // or the contents being destroyed) stops observation and deletes the
  // profiler, but does not overwrite the finish reason: it stays kDone,
  // matching the pre-existing NonEmptyPaint3 behavior. Only the FCP/LCP
  // that arrived before that event are recorded; the terminal event
  // doubles as the LCP finalization point.
  bool finish_reason_recorded_ = false;

  // The renderer-side presentation timestamp of the first contentful paint,
  // and whether it has been recorded yet.
  base::TimeTicks first_contentful_paint_ticks_;
  bool did_record_first_contentful_paint_ = false;

  // The renderer-side presentation timestamp of the latest largest contentful
  // paint candidate.
  base::TimeTicks last_largest_contentful_paint_ticks_;

#if BUILDFLAG(ENABLE_PDF)
  // The renderer-side timestamp of the first content paint reported by a PDF
  // plugin in the profiled contents, and whether it has been recorded yet.
  base::TimeTicks pdf_first_content_paint_ticks_;
  bool did_record_pdf_first_content_paint_ = false;

  // Held for the lifetime of this profiler. Subscribed in the constructor
  // rather than on the PDF's own PDFDocumentHelper because that helper does
  // not exist yet: it is created when the PDF renderer binds its PdfHost,
  // which is after this point.
  base::CallbackListSubscription pdf_first_content_paint_subscription_;
#endif  // BUILDFLAG(ENABLE_PDF)
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_FIRST_WEB_CONTENTS_PROFILER_BASE_H_
