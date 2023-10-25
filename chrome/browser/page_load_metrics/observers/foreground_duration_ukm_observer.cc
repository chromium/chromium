// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/foreground_duration_ukm_observer.h"

#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/navigation_handle.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"

ForegroundDurationUKMObserver::ForegroundDurationUKMObserver()
    : last_page_input_timing_(page_load_metrics::mojom::InputTiming::New()) {}

ForegroundDurationUKMObserver::~ForegroundDurationUKMObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ForegroundDurationUKMObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  currently_in_foreground_ = started_in_foreground;
  if (currently_in_foreground_) {
    last_time_shown_ = navigation_handle->NavigationStart();
  }
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ForegroundDurationUKMObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class doesn't use information on subframes and inner pages. No need to
  // forward.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ForegroundDurationUKMObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ForegroundDurationUKMObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordUkmIfInForeground(base::TimeTicks::Now());
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ForegroundDurationUKMObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordUkmIfInForeground(base::TimeTicks::Now());
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ForegroundDurationUKMObserver::OnShown() {
  if (!currently_in_foreground_) {
    last_time_shown_ = base::TimeTicks::Now();
    currently_in_foreground_ = true;
  }
  return CONTINUE_OBSERVING;
}

void ForegroundDurationUKMObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // If we have a page_end_time, use it as our end time, else fall back to the
  // current time. Note that we expect page_end_time.has_value() to always be
  // true in OnComplete (the PageLoadTracker destructor is supposed to guarantee
  // it), but we use Now() as a graceful fallback just in case.
  base::TimeTicks end_time =
      GetDelegate().GetPageEndReason() != page_load_metrics::END_NONE
          ? GetDelegate().GetPageEndTime()
          : base::TimeTicks::Now();
  RecordUkmIfInForeground(end_time);
}

void ForegroundDurationUKMObserver::RecordUkmIfInForeground(
    base::TimeTicks end_time) {
  if (!currently_in_foreground_)
    return;

  if (GetDelegate().GetPrerenderingState() ==
      page_load_metrics::PrerenderingState::kInPrerendering) {
    return;
  }

  base::TimeDelta foreground_duration = end_time - last_time_shown_;
  ukm::builders::PageForegroundSession ukm_builder(
      GetDelegate().GetPageUkmSourceId());
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  ukm_builder.SetForegroundDuration(foreground_duration.InMilliseconds());
  ukm_builder.Record(ukm_recorder);
  currently_in_foreground_ = false;
}

void ForegroundDurationUKMObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  if (GetDelegate().WasPrerenderedThenActivatedInForeground()) {
    last_time_shown_ = base::TimeTicks::Now();
    currently_in_foreground_ = true;
  } else {
    currently_in_foreground_ = false;
  }
}
