// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/first_web_contents_profiler_base.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "pdf/buildflags.h"

#if BUILDFLAG(ENABLE_PDF)
#include "base/functional/bind.h"
#include "components/pdf/browser/pdf_first_content_paint_registry.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace {

// Returns whether this instance was launched automatically by the OS as part of
// its startup.
bool IsAutoLaunchedByOs() {
#if BUILDFLAG(IS_WIN)
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kStartupForegroundLaunch);
#else
  return false;
#endif
}

}  // namespace

namespace metrics {

FirstWebContentsProfilerBase::FirstWebContentsProfilerBase(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  // On ChromeOS, session restore can create the profiler *before* the
  // navigation for a restored tab has started. In this case, GetPendingEntry()
  // will return nullptr. We store this initial state so that the first
  // DidStartNavigation event is correctly identified as the initial navigation
  // rather than an abandoned one. On other platforms, a navigation is
  // typically pending at construction.
  first_navigation_started_ = web_contents->GetController().GetPendingEntry();

#if BUILDFLAG(ENABLE_PDF)
  // `base::Unretained()` is safe because the subscription is a member that is
  // destroyed with this profiler, which unsubscribes the callback before
  // `this` is invalidated.
  pdf_first_content_paint_subscription_ =
      pdf::RegisterPdfFirstContentPaintCallback(base::BindRepeating(
          &FirstWebContentsProfilerBase::OnPdfFirstContentPainted,
          base::Unretained(this)));
#endif  // BUILDFLAG(ENABLE_PDF)
}

FirstWebContentsProfilerBase::~FirstWebContentsProfilerBase() = default;

// static
content::WebContents* FirstWebContentsProfilerBase::GetVisibleContents(
    BrowserWindowInterface* browser) {
  if (!browser->GetWindow()->IsVisible()) {
    return nullptr;
  }

  // The active WebContents may be hidden when the window height is small.
  content::WebContents* contents =
      browser->GetTabStripModel()->GetActiveWebContents();

  // It is incorrect to have a visible browser window with no active
  // WebContents, but reports on show that it happens.
  // See https://crbug.com/40662817 for Mac or https://crbug.com/40892329 for
  // Win.
  if (!contents) {
    return nullptr;
  }

  if (contents->GetVisibility() != content::Visibility::VISIBLE) {
    return nullptr;
  }

  return contents;
}

void FirstWebContentsProfilerBase::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // The profiler is concerned with the primary main frame navigation only.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // If a navigation wasn't pending when this profiler was created, the first
  // DidStartNavigation we see is the initial navigation. We should not treat
  // it as an abandoned navigation.
  if (!first_navigation_started_) {
    first_navigation_started_ = true;
    return;
  }

  // The profiler is created after DidStartNavigation() has been dispatched for
  // the first top-level navigation. If another DidStartNavigation() is
  // received, it means that a new navigation was initiated.
  FinishedCollectingMetrics(
      StartupProfilingFinishReason::kAbandonNewNavigation);
}

void FirstWebContentsProfilerBase::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (WasStartupInterrupted()) {
    FinishedCollectingMetrics(StartupProfilingFinishReason::kAbandonBlockingUI);
    return;
  }

  // Ignore subframe navigations, pre-rendering, and same-document navigations.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!navigation_handle->HasCommitted() || navigation_handle->IsErrorPage()) {
    FinishedCollectingMetrics(
        StartupProfilingFinishReason::kAbandonNavigationError);
    return;
  }

  // It is not possible to get a second top-level DidFinishNavigation() without
  // first having a DidStartNavigation(), which would have deleted |this|.
  DCHECK(!did_finish_first_navigation_);

  did_finish_first_navigation_ = true;

  RecordNavigationFinished(navigation_handle->NavigationStart());
}

void FirstWebContentsProfilerBase::DidFirstVisuallyNonEmptyPaint() {
  DCHECK(did_finish_first_navigation_);

  if (WasStartupInterrupted()) {
    FinishedCollectingMetrics(StartupProfilingFinishReason::kAbandonBlockingUI);
    return;
  }

  if (IsAutoLaunchedByOs()) {
    RecordFirstNonEmptyPaintForOsLaunch();
    FinishedCollectingMetrics(
        StartupProfilingFinishReason::kAbandonNonInteractiveStartup);
    return;
  }

  RecordFirstNonEmptyPaint();

  if (!ShouldObservePaintTimingMetrics()) {
    FinishedCollectingMetrics(StartupProfilingFinishReason::kDone);
    return;
  }

  // Record the successful finish reason now (preserving its timing), but keep
  // observing to capture the first contentful paint and the final largest
  // contentful paint, which occur after the first non-empty paint. The profiler
  // self-destructs when the page is hidden, a new navigation starts, or the
  // contents is destroyed.
  RecordFinishReason(StartupProfilingFinishReason::kDone);
  finish_reason_recorded_ = true;
  MaybeRecordFirstContentfulPaint();
#if BUILDFLAG(ENABLE_PDF)
  MaybeRecordPdfFirstContentPaint();
#endif  // BUILDFLAG(ENABLE_PDF)
}

void FirstWebContentsProfilerBase::OnFirstContentfulPaintInPrimaryMainFrame(
    base::TimeTicks presentation_time) {
  if (!ShouldObservePaintTimingMetrics() || WasStartupInterrupted()) {
    return;
  }

  if (first_contentful_paint_ticks_.is_null()) {
    first_contentful_paint_ticks_ = presentation_time;
  }
  MaybeRecordFirstContentfulPaint();
}

void FirstWebContentsProfilerBase::OnLargestContentfulPaintInPrimaryMainFrame(
    base::TimeTicks presentation_time) {
  if (!ShouldObservePaintTimingMetrics() || WasStartupInterrupted()) {
    return;
  }

  // The largest contentful paint may be updated multiple times; keep the latest
  // candidate. It is recorded when profiling ends. Ignore a null timestamp so a
  // spurious notification cannot clobber a previously observed valid candidate.
  if (!presentation_time.is_null()) {
    last_largest_contentful_paint_ticks_ = presentation_time;
  }
}

void FirstWebContentsProfilerBase::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility != content::Visibility::VISIBLE) {
    // Stop profiling if the content gets hidden as its load may be
    // deprioritized and timing it becomes meaningless.
    FinishedCollectingMetrics(
        StartupProfilingFinishReason::kAbandonContentHidden);
  }
}

void FirstWebContentsProfilerBase::WebContentsDestroyed() {
  FinishedCollectingMetrics(
      StartupProfilingFinishReason::kAbandonContentDestroyed);
}

void FirstWebContentsProfilerBase::MaybeRecordFirstContentfulPaint() {
  if (!finish_reason_recorded_ || did_record_first_contentful_paint_ ||
      first_contentful_paint_ticks_.is_null()) {
    return;
  }
  did_record_first_contentful_paint_ = true;
  RecordFirstContentfulPaint(first_contentful_paint_ticks_);
}

void FirstWebContentsProfilerBase::MaybeRecordLargestContentfulPaint() {
  if (last_largest_contentful_paint_ticks_.is_null()) {
    return;
  }
  RecordLargestContentfulPaint(last_largest_contentful_paint_ticks_);
}

#if BUILDFLAG(ENABLE_PDF)
void FirstWebContentsProfilerBase::OnPdfFirstContentPainted(
    content::WebContents* embedder,
    base::TimeTicks paint_time) {
  if (!ShouldObservePaintTimingMetrics() || WasStartupInterrupted()) {
    return;
  }

  // The broadcast is process-wide, so a PDF in any other tab or window reaches
  // here too. Only the contents being profiled describes this startup.
  if (embedder != web_contents()) {
    return;
  }

  // As with the contentful paints, the timestamp originates in the renderer,
  // so a null one is ignored rather than latched: latching it would
  // permanently satisfy "already have a candidate" while never being
  // recordable.
  if (pdf_first_content_paint_ticks_.is_null() && !paint_time.is_null()) {
    pdf_first_content_paint_ticks_ = paint_time;
  }
  MaybeRecordPdfFirstContentPaint();
}

void FirstWebContentsProfilerBase::MaybeRecordPdfFirstContentPaint() {
  // Gated on the non-empty paint the same way the contentful paints are, so
  // this only records for a startup that was not abandoned. For a PDF the
  // non-empty paint fires early - it marks the viewer shell finishing parsing
  // - so in practice it is already recorded by the time the plugin paints, but
  // the ordering is not guaranteed and this is also called from there.
  if (!finish_reason_recorded_ || did_record_pdf_first_content_paint_ ||
      pdf_first_content_paint_ticks_.is_null()) {
    return;
  }
  did_record_pdf_first_content_paint_ = true;
  RecordPdfFirstContentPaint(pdf_first_content_paint_ticks_);
}
#endif  // BUILDFLAG(ENABLE_PDF)

bool FirstWebContentsProfilerBase::ShouldObservePaintTimingMetrics() {
  return false;
}

void FirstWebContentsProfilerBase::FinishedCollectingMetrics(
    StartupProfilingFinishReason finish_reason) {
  if (finish_reason_recorded_) {
    // The finish reason was already recorded (kDone) at the first non-empty
    // paint while observing paint timing metrics. Record the final largest
    // contentful paint before deleting.
    MaybeRecordLargestContentfulPaint();
  } else {
    RecordFinishReason(finish_reason);
  }
  delete this;
}

}  // namespace metrics
