// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/aborts_page_load_metrics_observer.h"

#include "components/page_load_metrics/browser/page_load_metrics_util.h"

using page_load_metrics::PageAbortReason;

namespace internal {

const char kHistogramAbortForwardBackBeforeCommit[] =
    "PageLoad.Experimental.AbortTiming.ForwardBackNavigation.BeforeCommit";
const char kHistogramAbortReloadBeforeCommit[] =
    "PageLoad.Experimental.AbortTiming.Reload.BeforeCommit";
const char kHistogramAbortNewNavigationBeforeCommit[] =
    "PageLoad.Experimental.AbortTiming.NewNavigation.BeforeCommit";
const char kHistogramAbortStopBeforeCommit[] =
    "PageLoad.Experimental.AbortTiming.Stop.BeforeCommit";
const char kHistogramAbortCloseBeforeCommit[] =
    "PageLoad.Experimental.AbortTiming.Close.BeforeCommit";
const char kHistogramAbortBackgroundBeforeCommit[] =
    "PageLoad.Experimental.AbortTiming.Background.BeforeCommit";
const char kHistogramAbortOtherBeforeCommit[] =
    "PageLoad.Experimental.AbortTiming.Other.BeforeCommit";

const char kHistogramAbortForwardBackBeforePaint[] =
    "PageLoad.Experimental.AbortTiming.ForwardBackNavigation.AfterCommit."
    "BeforePaint";
const char kHistogramAbortReloadBeforePaint[] =
    "PageLoad.Experimental.AbortTiming.Reload.AfterCommit.BeforePaint";
const char kHistogramAbortNewNavigationBeforePaint[] =
    "PageLoad.Experimental.AbortTiming.NewNavigation.AfterCommit.BeforePaint";
const char kHistogramAbortStopBeforePaint[] =
    "PageLoad.Experimental.AbortTiming.Stop.AfterCommit.BeforePaint";
const char kHistogramAbortCloseBeforePaint[] =
    "PageLoad.Experimental.AbortTiming.Close.AfterCommit.BeforePaint";
const char kHistogramAbortBackgroundBeforePaint[] =
    "PageLoad.Experimental.AbortTiming.Background.AfterCommit.BeforePaint";

const char kHistogramAbortForwardBackDuringParse[] =
    "PageLoad.Experimental.AbortTiming.ForwardBackNavigation.DuringParse";
const char kHistogramAbortReloadDuringParse[] =
    "PageLoad.Experimental.AbortTiming.Reload.DuringParse";
const char kHistogramAbortNewNavigationDuringParse[] =
    "PageLoad.Experimental.AbortTiming.NewNavigation.DuringParse";
const char kHistogramAbortStopDuringParse[] =
    "PageLoad.Experimental.AbortTiming.Stop.DuringParse";
const char kHistogramAbortCloseDuringParse[] =
    "PageLoad.Experimental.AbortTiming.Close.DuringParse";
const char kHistogramAbortBackgroundDuringParse[] =
    "PageLoad.Experimental.AbortTiming.Background.DuringParse";

}  // namespace internal

namespace {

void RecordAbortBeforeCommit(
    const page_load_metrics::PageAbortInfo& abort_info) {
  switch (abort_info.reason) {
    case PageAbortReason::ABORT_RELOAD:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortReloadBeforeCommit,
                          abort_info.time_to_abort);
      if (abort_info.user_initiated_info.user_gesture) {
        PAGE_LOAD_HISTOGRAM(
            "PageLoad.Experimental.AbortTiming.Reload.BeforeCommit."
            "UserGesture",
            abort_info.time_to_abort);
      }
      if (abort_info.user_initiated_info.browser_initiated) {
        PAGE_LOAD_HISTOGRAM(
            "PageLoad.Experimental.AbortTiming.Reload.BeforeCommit."
            "BrowserInitiated",
            abort_info.time_to_abort);
      }
      return;
    case PageAbortReason::ABORT_FORWARD_BACK:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortForwardBackBeforeCommit,
                          abort_info.time_to_abort);
      if (abort_info.user_initiated_info.user_gesture) {
        PAGE_LOAD_HISTOGRAM(
            "PageLoad.Experimental.AbortTiming.ForwardBackNavigation."
            "BeforeCommit.UserGesture",
            abort_info.time_to_abort);
      }
      if (abort_info.user_initiated_info.browser_initiated) {
        PAGE_LOAD_HISTOGRAM(
            "PageLoad.Experimental.AbortTiming.ForwardBackNavigation."
            "BeforeCommit.BrowserInitiated",
            abort_info.time_to_abort);
      }
      return;
    case PageAbortReason::ABORT_NEW_NAVIGATION:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortNewNavigationBeforeCommit,
                          abort_info.time_to_abort);
      if (abort_info.user_initiated_info.user_gesture) {
        PAGE_LOAD_HISTOGRAM(
            "PageLoad.Experimental.AbortTiming.NewNavigation.BeforeCommit."
            "UserGesture",
            abort_info.time_to_abort);
      }
      if (abort_info.user_initiated_info.browser_initiated) {
        PAGE_LOAD_HISTOGRAM(
            "PageLoad.Experimental.AbortTiming.NewNavigation.BeforeCommit."
            "BrowserInitiated",
            abort_info.time_to_abort);
      }
      return;
    case PageAbortReason::ABORT_STOP:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortStopBeforeCommit,
                          abort_info.time_to_abort);
      return;
    case PageAbortReason::ABORT_CLOSE:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortCloseBeforeCommit,
                          abort_info.time_to_abort);
      return;
    case PageAbortReason::ABORT_BACKGROUND:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortBackgroundBeforeCommit,
                          abort_info.time_to_abort);
      return;
    case PageAbortReason::ABORT_OTHER:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortOtherBeforeCommit,
                          abort_info.time_to_abort);
      return;
    case PageAbortReason::ABORT_NONE:
      NOTREACHED();
      return;
  }
  NOTREACHED();
}

void RecordAbortAfterCommitBeforePaint(
    const page_load_metrics::PageAbortInfo& abort_info) {
  switch (abort_info.reason) {
    case PageAbortReason::ABORT_RELOAD:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortReloadBeforePaint,
                          abort_info.time_to_abort);
      if (abort_info.user_initiated_info.user_gesture) {
        PAGE_LOAD_HISTOGRAM(
            "PageLoad.Experimental.AbortTiming.Reload.AfterCommit.BeforePaint."
            "UserGesture",
            abort_info.time_to_abort);
      }
      if (abort_info.user_initiated_info.browser_initiated) {
        PAGE_LOAD_HISTOGRAM(
            "PageLoad.Experimental.AbortTiming.Reload.AfterCommit.BeforePaint."
            "BrowserInitiated",
            abort_info.time_to_abort);
      }
      return;
    case PageAbortReason::ABORT_FORWARD_BACK:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortForwardBackBeforePaint,
                          abort_info.time_to_abort);
      if (abort_info.user_initiated_info.user_gesture) {
        PAGE_LOAD_HISTOGRAM(
            "PageLoad.Experimental.AbortTiming.ForwardBackNavigation."
            "AfterCommit.BeforePaint.UserGesture",
            abort_info.time_to_abort);
      }
      if (abort_info.user_initiated_info.browser_initiated) {
        PAGE_LOAD_HISTOGRAM(
            "PageLoad.Experimental.AbortTiming.ForwardBackNavigation."
            "AfterCommit.BeforePaint.BrowserInitiated",
            abort_info.time_to_abort);
      }
      return;
    case PageAbortReason::ABORT_NEW_NAVIGATION:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortNewNavigationBeforePaint,
                          abort_info.time_to_abort);
      if (abort_info.user_initiated_info.user_gesture) {
        PAGE_LOAD_HISTOGRAM(
            "PageLoad.Experimental.AbortTiming.NewNavigation.AfterCommit."
            "BeforePaint.UserGesture",
            abort_info.time_to_abort);
      }
      if (abort_info.user_initiated_info.browser_initiated) {
        PAGE_LOAD_HISTOGRAM(
            "PageLoad.Experimental.AbortTiming.NewNavigation.AfterCommit."
            "BeforePaint.BrowserInitiated",
            abort_info.time_to_abort);
      }
      return;
    case PageAbortReason::ABORT_STOP:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortStopBeforePaint,
                          abort_info.time_to_abort);
      return;
    case PageAbortReason::ABORT_CLOSE:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortCloseBeforePaint,
                          abort_info.time_to_abort);
      return;
    case PageAbortReason::ABORT_BACKGROUND:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortBackgroundBeforePaint,
                          abort_info.time_to_abort);
      return;
    case PageAbortReason::ABORT_OTHER:
      // This is technically possible, though rare. See ~PageLoadTracker for an
      // explanation.
      return;
    case PageAbortReason::ABORT_NONE:
      NOTREACHED();
      return;
  }
  NOTREACHED();
}

void RecordAbortDuringParse(
    const page_load_metrics::PageAbortInfo& abort_info) {
  switch (abort_info.reason) {
    case PageAbortReason::ABORT_RELOAD:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortReloadDuringParse,
                          abort_info.time_to_abort);
      return;
    case PageAbortReason::ABORT_FORWARD_BACK:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortForwardBackDuringParse,
                          abort_info.time_to_abort);
      return;
    case PageAbortReason::ABORT_NEW_NAVIGATION:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortNewNavigationDuringParse,
                          abort_info.time_to_abort);
      return;
    case PageAbortReason::ABORT_STOP:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortStopDuringParse,
                          abort_info.time_to_abort);
      return;
    case PageAbortReason::ABORT_CLOSE:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortCloseDuringParse,
                          abort_info.time_to_abort);
      return;
    case PageAbortReason::ABORT_BACKGROUND:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortBackgroundDuringParse,
                          abort_info.time_to_abort);
      return;
    case PageAbortReason::ABORT_OTHER:
      // This is technically possible, though rare. See ~PageLoadTracker for an
      // explanation.
      return;
    case PageAbortReason::ABORT_NONE:
      NOTREACHED();
      return;
  }
  NOTREACHED();
}

bool ShouldTrackMetrics(
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate,
    const page_load_metrics::PageAbortInfo& abort_info) {
  if (abort_info.reason == PageAbortReason::ABORT_NONE)
    return false;

  // Don't log abort times if the page was backgrounded before the abort event.
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          abort_info.time_to_abort, delegate))
    return false;

  return true;
}

}  // namespace

AbortsPageLoadMetricsObserver::AbortsPageLoadMetricsObserver() {}

void AbortsPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  page_load_metrics::PageAbortInfo abort_info = GetPageAbortInfo(GetDelegate());
  if (!ShouldTrackMetrics(GetDelegate(), abort_info))
    return;

  // If we did not receive any timing IPCs from the render process, we can't
  // know for certain if the page was truly aborted before paint, or if the
  // abort happened before we received the IPC from the render process. Thus, we
  // do not log aborts for these page loads. Tracked page loads that receive no
  // timing IPCs are tracked via the ERR_NO_IPCS_RECEIVED error code in the
  // PageLoad.Events.InternalError histogram, so we can keep track of how often
  // this happens.
  if (page_load_metrics::IsEmpty(timing))
    return;

  if (timing.parse_timing->parse_start &&
      abort_info.time_to_abort >= timing.parse_timing->parse_start &&
      (!timing.parse_timing->parse_stop ||
       timing.parse_timing->parse_stop >= abort_info.time_to_abort)) {
    RecordAbortDuringParse(abort_info);
  }
  if (!timing.paint_timing->first_paint ||
      timing.paint_timing->first_paint >= abort_info.time_to_abort) {
    RecordAbortAfterCommitBeforePaint(abort_info);
  }
}

void AbortsPageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info) {
  page_load_metrics::PageAbortInfo abort_info = GetPageAbortInfo(GetDelegate());
  if (!ShouldTrackMetrics(GetDelegate(), abort_info))
    return;

  RecordAbortBeforeCommit(abort_info);
}
