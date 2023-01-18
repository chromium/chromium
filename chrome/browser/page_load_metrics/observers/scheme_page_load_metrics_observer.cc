// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/scheme_page_load_metrics_observer.h"

#include "base/metrics/histogram_functions.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"

namespace {

// Must remain synchronized with the enum of the same name in enums.xml.
enum class PageLoadTimingUnderStat {
  kTotal = 0,
  kLessThan1Second = 1,
  kLessThan2Seconds = 2,
  kLessThan5Seconds = 3,
  kLessThan8Seconds = 4,
  kLessThan10Seconds = 5,
  kMaxValue = kLessThan10Seconds
};

}  // namespace

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SchemePageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  return started_in_foreground ? CONTINUE_OBSERVING : STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SchemePageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This observer is interested in comparing performance among HTTP and HTTPS.
  // Including prerendering cases can be another factor to differentiate
  // performance, and it will be a noise for the original goal.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SchemePageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  // Capture committed transition type.
  transition_ = navigation_handle->GetPageTransition();
  if (navigation_handle->GetURL().scheme() == url::kHttpScheme ||
      navigation_handle->GetURL().scheme() == url::kHttpsScheme) {
    return CONTINUE_OBSERVING;
  }
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SchemePageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class is interested in events that are dispatched only for the primary
  // page or preprocessed by PageLoadTracker to be per-outermost page. So, no
  // need to forward events at the observer layer.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SchemePageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  return STOP_OBSERVING;
}

void SchemePageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (GetDelegate().GetUrl().scheme() == url::kHttpScheme) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.Scheme.HTTP.ParseTiming.NavigationToParseStart",
        timing.parse_timing->parse_start.value());
  } else if (GetDelegate().GetUrl().scheme() == url::kHttpsScheme) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.Scheme.HTTPS.ParseTiming.NavigationToParseStart",
        timing.parse_timing->parse_start.value());
  }
}

void SchemePageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK(GetDelegate().GetUrl().scheme() == url::kHttpScheme ||
         GetDelegate().GetUrl().scheme() == url::kHttpsScheme);

  base::TimeDelta fcp = timing.paint_timing->first_contentful_paint.value();
  base::TimeDelta parse_start_to_fcp =
      fcp - timing.parse_timing->parse_start.value();

  if (GetDelegate().GetUrl().scheme() == url::kHttpScheme) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.Scheme.HTTP.PaintTiming."
        "NavigationToFirstContentfulPaint",
        fcp);
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.Scheme.HTTP.PaintTiming."
        "ParseStartToFirstContentfulPaint",
        parse_start_to_fcp);
  } else {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.Scheme.HTTPS.PaintTiming."
        "NavigationToFirstContentfulPaint",
        fcp);
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.Scheme.HTTPS.PaintTiming."
        "ParseStartToFirstContentfulPaint",
        parse_start_to_fcp);
  }

  static constexpr char kUnderStatHistogramHttp[] =
      "PageLoad.Clients.Scheme.HTTP.PaintTiming.UnderStat";
  static constexpr char kUnderStatHistogramHttps[] =
      "PageLoad.Clients.Scheme.HTTPS.PaintTiming.UnderStat";
  static constexpr char kUnderStatHistogramHttpUserNewNav[] =
      "PageLoad.Clients.Scheme.HTTP.PaintTiming.UnderStat.UserInitiated."
      "NewNavigation";
  static constexpr char kUnderStatHistogramHttpsUserNewNav[] =
      "PageLoad.Clients.Scheme.HTTPS.PaintTiming.UnderStat.UserInitiated."
      "NewNavigation";

  bool is_user_initiated =
      GetDelegate().GetUserInitiatedInfo().browser_initiated ||
      GetDelegate().GetUserInitiatedInfo().user_gesture;
  bool is_user_initiated_new_navigation =
      is_user_initiated && ui::PageTransitionIsNewNavigation(transition_);

  // Record understat metrics for the time to first contentful paint.
  static constexpr const int kUnderStatRecordingIntervalsSeconds[] = {1, 2, 5,
                                                                      8, 10};
  static_assert(std::size(kUnderStatRecordingIntervalsSeconds) ==
                    static_cast<int>(PageLoadTimingUnderStat::kMaxValue),
                " mismatch in  array length and enum size");

  // Record the total count bucket first.
  base::UmaHistogramEnumeration(
      GetDelegate().GetUrl().scheme() == url::kHttpScheme
          ? kUnderStatHistogramHttp
          : kUnderStatHistogramHttps,
      PageLoadTimingUnderStat::kTotal);
  if (is_user_initiated_new_navigation) {
    base::UmaHistogramEnumeration(
        GetDelegate().GetUrl().scheme() == url::kHttpScheme
            ? kUnderStatHistogramHttpUserNewNav
            : kUnderStatHistogramHttpsUserNewNav,
        PageLoadTimingUnderStat::kTotal);
  }

  for (size_t index = 0; index < std::size(kUnderStatRecordingIntervalsSeconds);
       ++index) {
    base::TimeDelta threshold(
        base::Seconds(kUnderStatRecordingIntervalsSeconds[index]));
    if (fcp <= threshold) {
      base::UmaHistogramEnumeration(
          GetDelegate().GetUrl().scheme() == url::kHttpScheme
              ? kUnderStatHistogramHttp
              : kUnderStatHistogramHttps,
          static_cast<PageLoadTimingUnderStat>(index + 1));
      if (is_user_initiated_new_navigation) {
        base::UmaHistogramEnumeration(
            GetDelegate().GetUrl().scheme() == url::kHttpScheme
                ? kUnderStatHistogramHttpUserNewNav
                : kUnderStatHistogramHttpsUserNewNav,
            static_cast<PageLoadTimingUnderStat>(index + 1));
      }
    }
  }
}

void SchemePageLoadMetricsObserver::OnFirstMeaningfulPaintInMainFrameDocument(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (GetDelegate().GetUrl().scheme() == url::kHttpScheme) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.Scheme.HTTP.Experimental.PaintTiming."
        "NavigationToFirstMeaningfulPaint",
        timing.paint_timing->first_meaningful_paint.value());
  } else if (GetDelegate().GetUrl().scheme() == url::kHttpsScheme) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.Scheme.HTTPS.Experimental.PaintTiming."
        "NavigationToFirstMeaningfulPaint",
        timing.paint_timing->first_meaningful_paint.value());
  }
}
