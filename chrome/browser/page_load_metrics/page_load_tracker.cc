// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/page_load_tracker.h"

#include <algorithm>
#include <ostream>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_embedder_interface.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/page_transition_types.h"

// This macro invokes the specified method on each observer, passing the
// variable length arguments as the method's arguments, and removes the observer
// from the list of observers if the given method returns STOP_OBSERVING.
#define INVOKE_AND_PRUNE_OBSERVERS(observers, Method, ...)      \
  {                                                             \
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),          \
                 "PageLoadMetricsObserver::" #Method);          \
    for (auto it = observers.begin(); it != observers.end();) { \
      if ((*it)->Method(__VA_ARGS__) ==                         \
          PageLoadMetricsObserver::STOP_OBSERVING) {            \
        it = observers.erase(it);                               \
      } else {                                                  \
        ++it;                                                   \
      }                                                         \
    }                                                           \
  }

namespace page_load_metrics {

namespace internal {

const char kErrorEvents[] = "PageLoad.Internal.ErrorCode";
const char kAbortChainSizeReload[] =
    "PageLoad.Internal.ProvisionalAbortChainSize.Reload";
const char kAbortChainSizeForwardBack[] =
    "PageLoad.Internal.ProvisionalAbortChainSize.ForwardBack";
const char kAbortChainSizeNewNavigation[] =
    "PageLoad.Internal.ProvisionalAbortChainSize.NewNavigation";
const char kAbortChainSizeSameURL[] =
    "PageLoad.Internal.ProvisionalAbortChainSize.SameURL";
const char kAbortChainSizeNoCommit[] =
    "PageLoad.Internal.ProvisionalAbortChainSize.NoCommit";
const char kClientRedirectFirstPaintToNavigation[] =
    "PageLoad.Internal.ClientRedirect.FirstPaintToNavigation";
const char kClientRedirectWithoutPaint[] =
    "PageLoad.Internal.ClientRedirect.NavigationWithoutPaint";
const char kPageLoadCompletedAfterAppBackground[] =
    "PageLoad.Internal.PageLoadCompleted.AfterAppBackground";
const char kPageLoadStartedInForeground[] =
    "PageLoad.Internal.NavigationStartedInForeground";
const char kPageLoadPrerender[] = "PageLoad.Internal.Prerender";

}  // namespace internal

void RecordInternalError(InternalErrorLoadEvent event) {
  UMA_HISTOGRAM_ENUMERATION(internal::kErrorEvents, event, ERR_LAST_ENTRY);
}

// TODO(csharrison): Add a case for client side redirects, which is what JS
// initiated window.location / window.history navigations get set to.
PageEndReason EndReasonForPageTransition(ui::PageTransition transition) {
  if (transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT) {
    return END_CLIENT_REDIRECT;
  }
  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD))
    return END_RELOAD;
  if (transition & ui::PAGE_TRANSITION_FORWARD_BACK)
    return END_FORWARD_BACK;
  if (ui::PageTransitionIsNewNavigation(transition))
    return END_NEW_NAVIGATION;
  NOTREACHED()
      << "EndReasonForPageTransition received unexpected ui::PageTransition: "
      << transition;
  return END_OTHER;
}

void LogAbortChainSameURLHistogram(int aborted_chain_size_same_url) {
  if (aborted_chain_size_same_url > 0) {
    UMA_HISTOGRAM_COUNTS_1M(internal::kAbortChainSizeSameURL,
                            aborted_chain_size_same_url);
  }
}

bool IsNavigationUserInitiated(content::NavigationHandle* handle) {
  // TODO(crbug.com/617904): Browser initiated navigations should have
  // HasUserGesture() set to true. In the meantime, we consider all
  // browser-initiated navigations to be user initiated.
  //
  // TODO(crbug.com/637345): Some browser-initiated navigations incorrectly
  // report that they are renderer-initiated. We will currently report that
  // these navigations are not user initiated, when in fact they are user
  // initiated.
  return handle->HasUserGesture() || !handle->IsRendererInitiated();
}

namespace {

void RecordAppBackgroundPageLoadCompleted(bool completed_after_background) {
  UMA_HISTOGRAM_BOOLEAN(internal::kPageLoadCompletedAfterAppBackground,
                        completed_after_background);
}

void DispatchObserverTimingCallbacks(
    PageLoadMetricsObserver* observer,
    const mojom::PageLoadTiming& last_timing,
    const mojom::PageLoadTiming& new_timing,
    const PageLoadExtraInfo& extra_info) {
  if (!last_timing.Equals(new_timing))
    observer->OnTimingUpdate(nullptr, new_timing, extra_info);
  if (new_timing.document_timing->dom_content_loaded_event_start &&
      !last_timing.document_timing->dom_content_loaded_event_start)
    observer->OnDomContentLoadedEventStart(new_timing, extra_info);
  if (new_timing.document_timing->load_event_start &&
      !last_timing.document_timing->load_event_start)
    observer->OnLoadEventStart(new_timing, extra_info);
  if (new_timing.document_timing->first_layout &&
      !last_timing.document_timing->first_layout)
    observer->OnFirstLayout(new_timing, extra_info);
  if (new_timing.interactive_timing->first_input_delay &&
      !last_timing.interactive_timing->first_input_delay)
    observer->OnFirstInputInPage(new_timing, extra_info);
  if (new_timing.paint_timing->first_paint &&
      !last_timing.paint_timing->first_paint)
    observer->OnFirstPaintInPage(new_timing, extra_info);
  if (new_timing.paint_timing->first_text_paint &&
      !last_timing.paint_timing->first_text_paint)
    observer->OnFirstTextPaintInPage(new_timing, extra_info);
  if (new_timing.paint_timing->first_image_paint &&
      !last_timing.paint_timing->first_image_paint)
    observer->OnFirstImagePaintInPage(new_timing, extra_info);
  if (new_timing.paint_timing->first_contentful_paint &&
      !last_timing.paint_timing->first_contentful_paint)
    observer->OnFirstContentfulPaintInPage(new_timing, extra_info);
  if (new_timing.paint_timing->first_meaningful_paint &&
      !last_timing.paint_timing->first_meaningful_paint)
    observer->OnFirstMeaningfulPaintInMainFrameDocument(new_timing, extra_info);
  if (new_timing.paint_timing->largest_image_paint &&
      !last_timing.paint_timing->largest_image_paint)
    observer->OnLargestImagePaintInMainFrameDocument(new_timing, extra_info);
  if (new_timing.paint_timing->last_image_paint &&
      !last_timing.paint_timing->last_image_paint)
    observer->OnLastImagePaintInMainFrameDocument(new_timing, extra_info);
  if (new_timing.paint_timing->largest_text_paint &&
      !last_timing.paint_timing->largest_text_paint)
    observer->OnLargestTextPaintInMainFrameDocument(new_timing, extra_info);
  if (new_timing.paint_timing->last_text_paint &&
      !last_timing.paint_timing->last_text_paint)
    observer->OnLastTextPaintInMainFrameDocument(new_timing, extra_info);
  if (new_timing.interactive_timing->interactive &&
      !last_timing.interactive_timing->interactive)
    observer->OnPageInteractive(new_timing, extra_info);
  if (new_timing.parse_timing->parse_start &&
      !last_timing.parse_timing->parse_start)
    observer->OnParseStart(new_timing, extra_info);
  if (new_timing.parse_timing->parse_stop &&
      !last_timing.parse_timing->parse_stop)
    observer->OnParseStop(new_timing, extra_info);
}

}  // namespace

PageLoadTracker::PageLoadTracker(
    bool in_foreground,
    PageLoadMetricsEmbedderInterface* embedder_interface,
    const GURL& currently_committed_url,
    content::NavigationHandle* navigation_handle,
    UserInitiatedInfo user_initiated_info,
    int aborted_chain_size,
    int aborted_chain_size_same_url)
    : did_stop_tracking_(false),
      app_entered_background_(false),
      navigation_start_(navigation_handle->NavigationStart()),
      url_(navigation_handle->GetURL()),
      start_url_(navigation_handle->GetURL()),
      did_commit_(false),
      page_end_reason_(END_NONE),
      page_end_user_initiated_info_(UserInitiatedInfo::NotUserInitiated()),
      started_in_foreground_(in_foreground),
      last_dispatched_merged_page_timing_(CreatePageLoadTiming()),
      page_transition_(navigation_handle->GetPageTransition()),
      user_initiated_info_(user_initiated_info),
      aborted_chain_size_(aborted_chain_size),
      aborted_chain_size_same_url_(aborted_chain_size_same_url),
      embedder_interface_(embedder_interface),
      metrics_update_dispatcher_(this, navigation_handle, embedder_interface),
      source_id_(ukm::ConvertToSourceId(navigation_handle->GetNavigationId(),
                                        ukm::SourceIdType::NAVIGATION_ID)) {
  DCHECK(!navigation_handle->HasCommitted());
  embedder_interface_->RegisterObservers(this);
  INVOKE_AND_PRUNE_OBSERVERS(observers_, OnStart, navigation_handle,
                             currently_committed_url, started_in_foreground_);

  UMA_HISTOGRAM_BOOLEAN(internal::kPageLoadStartedInForeground,
                        started_in_foreground_);
  const bool is_prerender = prerender::PrerenderContents::FromWebContents(
                                navigation_handle->GetWebContents()) != nullptr;
  if (is_prerender) {
    UMA_HISTOGRAM_BOOLEAN(internal::kPageLoadPrerender, true);
  }
}

PageLoadTracker::~PageLoadTracker() {
  if (app_entered_background_) {
    RecordAppBackgroundPageLoadCompleted(true);
  }

  if (did_stop_tracking_)
    return;

  metrics_update_dispatcher_.ShutDown();

  if (page_end_time_.is_null()) {
    // page_end_time_ can be unset in some cases, such as when a navigation is
    // aborted by a navigation that started before it. In these cases, set the
    // end time to the current time.
    RecordInternalError(ERR_NO_PAGE_LOAD_END_TIME);
    NotifyPageEnd(END_OTHER, UserInitiatedInfo::NotUserInitiated(),
                  base::TimeTicks::Now(), true);
  }

  if (!did_commit_) {
    if (!failed_provisional_load_info_)
      RecordInternalError(ERR_NO_COMMIT_OR_FAILED_PROVISIONAL_LOAD);

    // Don't include any aborts that resulted in a new navigation, as the chain
    // length will be included in the aborter PageLoadTracker.
    if (page_end_reason_ != END_RELOAD &&
        page_end_reason_ != END_FORWARD_BACK &&
        page_end_reason_ != END_NEW_NAVIGATION) {
      LogAbortChainHistograms(nullptr);
    }
  } else if (page_load_metrics::IsEmpty(metrics_update_dispatcher_.timing())) {
    RecordInternalError(ERR_NO_IPCS_RECEIVED);
  }

  const PageLoadExtraInfo info = ComputePageLoadExtraInfo();
  for (const auto& observer : observers_) {
    if (failed_provisional_load_info_) {
      observer->OnFailedProvisionalLoad(*failed_provisional_load_info_, info);
    } else if (did_commit_) {
      observer->OnComplete(metrics_update_dispatcher_.timing(), info);
    }
  }
}

void PageLoadTracker::LogAbortChainHistograms(
    content::NavigationHandle* final_navigation) {
  if (aborted_chain_size_ == 0)
    return;
  // Note that this could be broken out by this navigation's abort type, if more
  // granularity is needed. Add one to the chain size to count the current
  // navigation. In the other cases, the current navigation is the final
  // navigation (which commits).
  if (!final_navigation) {
    UMA_HISTOGRAM_COUNTS_1M(internal::kAbortChainSizeNoCommit,
                            aborted_chain_size_ + 1);
    LogAbortChainSameURLHistogram(aborted_chain_size_same_url_ + 1);
    return;
  }

  // The following is only executed for committing trackers.
  DCHECK(did_commit_);

  // Note that histograms could be separated out by this commit's transition
  // type, but for simplicity they will all be bucketed together.
  LogAbortChainSameURLHistogram(aborted_chain_size_same_url_);

  ui::PageTransition committed_transition =
      final_navigation->GetPageTransition();
  switch (EndReasonForPageTransition(committed_transition)) {
    case END_RELOAD:
      UMA_HISTOGRAM_COUNTS_1M(internal::kAbortChainSizeReload,
                              aborted_chain_size_);
      return;
    case END_FORWARD_BACK:
      UMA_HISTOGRAM_COUNTS_1M(internal::kAbortChainSizeForwardBack,
                              aborted_chain_size_);
      return;
    // TODO(csharrison): Refactor this code so it is based on the WillStart*
    // code path instead of the committed load code path. Then, for every abort
    // chain, log a histogram of the counts of each of these metrics. For now,
    // merge client redirects with new navigations, which was (basically) the
    // previous behavior.
    case END_CLIENT_REDIRECT:
    case END_NEW_NAVIGATION:
      UMA_HISTOGRAM_COUNTS_1M(internal::kAbortChainSizeNewNavigation,
                              aborted_chain_size_);
      return;
    default:
      NOTREACHED()
          << "LogAbortChainHistograms received unexpected ui::PageTransition: "
          << committed_transition;
      return;
  }
}

void PageLoadTracker::WebContentsHidden() {
  // Only log the first time we background in a given page load.
  if (background_time_.is_null()) {
    // Make sure we either started in the foreground and haven't been
    // foregrounded yet, or started in the background and have already been
    // foregrounded.
    DCHECK_EQ(started_in_foreground_, foreground_time_.is_null());
    background_time_ = base::TimeTicks::Now();
    ClampBrowserTimestampIfInterProcessTimeTickSkew(&background_time_);
  }
  const PageLoadExtraInfo info = ComputePageLoadExtraInfo();
  INVOKE_AND_PRUNE_OBSERVERS(observers_, OnHidden,
                             metrics_update_dispatcher_.timing(), info);
}

void PageLoadTracker::WebContentsShown() {
  // Only log the first time we foreground in a given page load.
  if (foreground_time_.is_null()) {
    // Make sure we either started in the background and haven't been
    // backgrounded yet, or started in the foreground and have already been
    // backgrounded.
    DCHECK_NE(started_in_foreground_, background_time_.is_null());
    foreground_time_ = base::TimeTicks::Now();
    ClampBrowserTimestampIfInterProcessTimeTickSkew(&foreground_time_);
  }

  INVOKE_AND_PRUNE_OBSERVERS(observers_, OnShown);
}

void PageLoadTracker::WillProcessNavigationResponse(
    content::NavigationHandle* navigation_handle) {
  DCHECK(!navigation_request_id_.has_value());
  navigation_request_id_ = navigation_handle->GetGlobalRequestID();
}

void PageLoadTracker::Commit(content::NavigationHandle* navigation_handle) {
  did_commit_ = true;
  url_ = navigation_handle->GetURL();
  // Some transitions (like CLIENT_REDIRECT) are only known at commit time.
  page_transition_ = navigation_handle->GetPageTransition();
  user_initiated_info_.user_gesture = navigation_handle->HasUserGesture();

  const std::string& mime_type =
      navigation_handle->GetWebContents()->GetContentsMimeType();
  INVOKE_AND_PRUNE_OBSERVERS(observers_, ShouldObserveMimeType, mime_type);
  INVOKE_AND_PRUNE_OBSERVERS(observers_, OnCommit, navigation_handle,
                             source_id_);
  LogAbortChainHistograms(navigation_handle);
}

void PageLoadTracker::DidCommitSameDocumentNavigation(
    content::NavigationHandle* navigation_handle) {
  for (const auto& observer : observers_) {
    observer->OnCommitSameDocumentNavigation(navigation_handle);
  }
}

void PageLoadTracker::DidInternalNavigationAbort(
    content::NavigationHandle* navigation_handle) {
  for (const auto& observer : observers_) {
    observer->OnDidInternalNavigationAbort(navigation_handle);
  }
}

void PageLoadTracker::DidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  for (const auto& observer : observers_) {
    observer->OnDidFinishSubFrameNavigation(navigation_handle);
  }
}

void PageLoadTracker::FailedProvisionalLoad(
    content::NavigationHandle* navigation_handle,
    base::TimeTicks failed_load_time) {
  DCHECK(!failed_provisional_load_info_);
  failed_provisional_load_info_.reset(new FailedProvisionalLoadInfo(
      failed_load_time - navigation_handle->NavigationStart(),
      navigation_handle->GetNetErrorCode()));
}

void PageLoadTracker::Redirect(content::NavigationHandle* navigation_handle) {
  url_ = navigation_handle->GetURL();
  INVOKE_AND_PRUNE_OBSERVERS(observers_, OnRedirect, navigation_handle);
}

void PageLoadTracker::OnInputEvent(const blink::WebInputEvent& event) {
  input_tracker_.OnInputEvent(event);
  for (const auto& observer : observers_) {
    observer->OnUserInput(event);
  }
}

void PageLoadTracker::FlushMetricsOnAppEnterBackground() {
  if (!app_entered_background_) {
    RecordAppBackgroundPageLoadCompleted(false);
    app_entered_background_ = true;
  }

  const PageLoadExtraInfo info = ComputePageLoadExtraInfo();
  INVOKE_AND_PRUNE_OBSERVERS(observers_, FlushMetricsOnAppEnterBackground,
                             metrics_update_dispatcher_.timing(), info);
}

void PageLoadTracker::NotifyClientRedirectTo(
    const PageLoadTracker& destination) {
  if (metrics_update_dispatcher_.timing().paint_timing->first_paint) {
    base::TimeTicks first_paint_time =
        navigation_start() +
        metrics_update_dispatcher_.timing().paint_timing->first_paint.value();
    base::TimeDelta first_paint_to_navigation;
    if (destination.navigation_start() > first_paint_time)
      first_paint_to_navigation =
          destination.navigation_start() - first_paint_time;
    PAGE_LOAD_HISTOGRAM(internal::kClientRedirectFirstPaintToNavigation,
                        first_paint_to_navigation);
  } else {
    UMA_HISTOGRAM_BOOLEAN(internal::kClientRedirectWithoutPaint, true);
  }
}

void PageLoadTracker::OnLoadedResource(
    const ExtraRequestCompleteInfo& extra_request_complete_info) {
  for (const auto& observer : observers_) {
    observer->OnLoadedResource(extra_request_complete_info);
  }
}

void PageLoadTracker::StopTracking() {
  did_stop_tracking_ = true;
  observers_.clear();
}

void PageLoadTracker::AddObserver(
    std::unique_ptr<PageLoadMetricsObserver> observer) {
  observers_.push_back(std::move(observer));
}

void PageLoadTracker::ClampBrowserTimestampIfInterProcessTimeTickSkew(
    base::TimeTicks* event_time) {
  DCHECK(event_time != nullptr);
  // Windows 10 GCE bot non-deterministically failed because TimeTicks::Now()
  // called in the browser process e.g. commit_time was less than
  // navigation_start_ that was populated in the renderer process because the
  // clock was not system-wide monotonic.
  // Note that navigation_start_ can also be set in the browser process in
  // some cases and in those cases event_time should never be <
  // navigation_start_. If it is due to a code error and it gets clamped in this
  // function, on high resolution systems it should lead to a dcheck failure.

  // TODO(shivanisha): Currently IsHighResolution is the best way to check
  // if the clock is system-wide monotonic. However IsHighResolution
  // does a broader check to see if the clock in use is high resolution
  // which also implies it is system-wide monotonic (on Windows).
  if (base::TimeTicks::IsHighResolution()) {
    DCHECK(event_time->is_null() || *event_time >= navigation_start_);
    return;
  }

  if (!event_time->is_null() && *event_time < navigation_start_) {
    RecordInternalError(ERR_INTER_PROCESS_TIME_TICK_SKEW);
    *event_time = navigation_start_;
  }
}

PageLoadExtraInfo PageLoadTracker::ComputePageLoadExtraInfo() const {
  base::Optional<base::TimeDelta> first_background_time;
  base::Optional<base::TimeDelta> first_foreground_time;
  base::Optional<base::TimeDelta> page_end_time;

  if (!background_time_.is_null()) {
    DCHECK_GE(background_time_, navigation_start_);
    first_background_time = background_time_ - navigation_start_;
  }

  if (!foreground_time_.is_null()) {
    DCHECK_GE(foreground_time_, navigation_start_);
    first_foreground_time = foreground_time_ - navigation_start_;
  }

  if (page_end_reason_ != END_NONE) {
    DCHECK_GE(page_end_time_, navigation_start_);
    page_end_time = page_end_time_ - navigation_start_;
  } else {
    DCHECK(page_end_time_.is_null());
  }

  // page_end_reason_ == END_NONE implies page_end_user_initiated_info_ is not
  // user initiated.
  DCHECK(page_end_reason_ != END_NONE ||
         (!page_end_user_initiated_info_.browser_initiated &&
          !page_end_user_initiated_info_.user_gesture &&
          !page_end_user_initiated_info_.user_input_event));
  return PageLoadExtraInfo(
      navigation_start_, first_background_time, first_foreground_time,
      started_in_foreground_, user_initiated_info_, url(), start_url_,
      did_commit_, page_end_reason_, page_end_user_initiated_info_,
      page_end_time, metrics_update_dispatcher_.main_frame_metadata(),
      metrics_update_dispatcher_.subframe_metadata(), source_id_);
}

bool PageLoadTracker::HasMatchingNavigationRequestID(
    const content::GlobalRequestID& request_id) const {
  DCHECK(request_id != content::GlobalRequestID());
  return navigation_request_id_.has_value() &&
         navigation_request_id_.value() == request_id;
}

void PageLoadTracker::NotifyPageEnd(PageEndReason page_end_reason,
                                    UserInitiatedInfo user_initiated_info,
                                    base::TimeTicks timestamp,
                                    bool is_certainly_browser_timestamp) {
  DCHECK_NE(page_end_reason, END_NONE);
  // Use UpdatePageEnd to update an already notified PageLoadTracker.
  if (page_end_reason_ != END_NONE)
    return;

  UpdatePageEndInternal(page_end_reason, user_initiated_info, timestamp,
                        is_certainly_browser_timestamp);
}

void PageLoadTracker::UpdatePageEnd(PageEndReason page_end_reason,
                                    UserInitiatedInfo user_initiated_info,
                                    base::TimeTicks timestamp,
                                    bool is_certainly_browser_timestamp) {
  DCHECK_NE(page_end_reason, END_NONE);
  DCHECK_NE(page_end_reason, END_OTHER);
  DCHECK_EQ(page_end_reason_, END_OTHER);
  DCHECK(!page_end_time_.is_null());
  if (page_end_time_.is_null() || page_end_reason_ != END_OTHER)
    return;

  // For some aborts (e.g. navigations), the initiated timestamp can be earlier
  // than the timestamp that aborted the load. Taking the minimum gives the
  // closest user initiated time known.
  UpdatePageEndInternal(page_end_reason, user_initiated_info,
                        std::min(page_end_time_, timestamp),
                        is_certainly_browser_timestamp);
}

bool PageLoadTracker::IsLikelyProvisionalAbort(
    base::TimeTicks abort_cause_time) const {
  // Note that |abort_cause_time - page_end_time_| can be negative.
  return page_end_reason_ == END_OTHER &&
         (abort_cause_time - page_end_time_).InMilliseconds() < 100;
}

bool PageLoadTracker::MatchesOriginalNavigation(
    content::NavigationHandle* navigation_handle) {
  // Neither navigation should have committed.
  DCHECK(!navigation_handle->HasCommitted());
  DCHECK(!did_commit_);
  return navigation_handle->GetURL() == start_url_;
}

void PageLoadTracker::UpdatePageEndInternal(
    PageEndReason page_end_reason,
    UserInitiatedInfo user_initiated_info,
    base::TimeTicks timestamp,
    bool is_certainly_browser_timestamp) {
  // When a provisional navigation commits, that navigation's start time is
  // interpreted as the abort time for other provisional loads in the tab.
  // However, this only makes sense if the committed load started after the
  // aborted provisional loads started. Thus we ignore cases where the committed
  // load started before the aborted provisional load, as this would result in
  // recording a negative time-to-abort. The real issue here is that we have to
  // infer the cause of aborts. It would be better if the navigation code could
  // instead report the actual cause of an aborted navigation. See crbug/571647
  // for details.
  if (timestamp < navigation_start_) {
    RecordInternalError(ERR_END_BEFORE_NAVIGATION_START);
    page_end_reason_ = END_NONE;
    page_end_time_ = base::TimeTicks();
    return;
  }
  page_end_reason_ = page_end_reason;
  page_end_time_ = timestamp;
  // A client redirect can never be user initiated. Due to the way Blink
  // implements user gesture tracking, where all events that occur within 1
  // second after a user interaction are considered to be triggered by user
  // activation (based on HTML spec:
  // https://html.spec.whatwg.org/multipage/interaction.html#triggered-by-user-activation),
  // these navs may sometimes be reported as user initiated by Blink. Thus, we
  // explicitly filter these types of aborts out when deciding if the abort was
  // user initiated.
  if (page_end_reason != END_CLIENT_REDIRECT)
    page_end_user_initiated_info_ = user_initiated_info;

  if (is_certainly_browser_timestamp) {
    ClampBrowserTimestampIfInterProcessTimeTickSkew(&page_end_time_);
  }
}

void PageLoadTracker::MediaStartedPlaying(
    const content::WebContentsObserver::MediaPlayerInfo& video_type,
    bool is_in_main_frame) {
  for (const auto& observer : observers_)
    observer->MediaStartedPlaying(video_type, is_in_main_frame);
}

void PageLoadTracker::OnTimingChanged() {
  DCHECK(!last_dispatched_merged_page_timing_->Equals(
      metrics_update_dispatcher_.timing()));

  PageLoadExtraInfo extra_info(ComputePageLoadExtraInfo());
  for (const auto& observer : observers_) {
    DispatchObserverTimingCallbacks(
        observer.get(), *last_dispatched_merged_page_timing_,
        metrics_update_dispatcher_.timing(), extra_info);
  }
  last_dispatched_merged_page_timing_ =
      metrics_update_dispatcher_.timing().Clone();
}

void PageLoadTracker::OnSubFrameTimingChanged(
    content::RenderFrameHost* rfh,
    const mojom::PageLoadTiming& timing) {
  PageLoadExtraInfo extra_info(ComputePageLoadExtraInfo());
  DCHECK(rfh->GetParent());
  for (const auto& observer : observers_) {
    observer->OnTimingUpdate(rfh, timing, extra_info);
  }
}

void PageLoadTracker::OnMainFrameMetadataChanged() {
  PageLoadExtraInfo extra_info(ComputePageLoadExtraInfo());
  for (const auto& observer : observers_) {
    observer->OnLoadingBehaviorObserved(extra_info);
  }
}

void PageLoadTracker::OnSubframeMetadataChanged() {
  PageLoadExtraInfo extra_info(ComputePageLoadExtraInfo());
  for (const auto& observer : observers_) {
    observer->OnLoadingBehaviorObserved(extra_info);
  }
}

void PageLoadTracker::BroadcastEventToObservers(const void* const event_key) {
  for (const auto& observer : observers_) {
    observer->OnEventOccurred(event_key);
  }
}

void PageLoadTracker::UpdateFeaturesUsage(
    const mojom::PageLoadFeatures& new_features) {
  PageLoadExtraInfo extra_info(ComputePageLoadExtraInfo());
  for (const auto& observer : observers_) {
    observer->OnFeaturesUsageObserved(new_features, extra_info);
  }
}

void PageLoadTracker::UpdateResourceDataUse(
    const std::vector<mojom::ResourceDataUpdatePtr>& resources) {
  for (const auto& observer : observers_) {
    observer->OnResourceDataUseObserved(resources);
  }
}

}  // namespace page_load_metrics
