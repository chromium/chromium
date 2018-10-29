// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_OBSERVER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/resource_type.h"
#include "net/base/host_port_pair.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace page_load_metrics {

// This enum represents how a page load ends. If the action occurs before the
// page load finishes (or reaches some point like first paint), then we consider
// the load to be aborted.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. For any additions, also update the
// corresponding PageEndReason enum in enums.xml.
enum PageEndReason {
  // Page lifetime has not yet ended (page is still active).
  END_NONE = 0,

  // The page was reloaded, possibly by the user.
  END_RELOAD = 1,

  // The page was navigated away from, via a back or forward navigation.
  END_FORWARD_BACK = 2,

  // The navigation is replaced with a navigation with the qualifier
  // ui::PAGE_TRANSITION_CLIENT_REDIRECT, which is caused by Javascript, or the
  // meta refresh tag.
  END_CLIENT_REDIRECT = 3,

  // If the page load is replaced by a new navigation. This includes link
  // clicks, typing in the omnibox (not a reload), and form submissions.
  END_NEW_NAVIGATION = 4,

  // The page load was stopped (e.g. the user presses the stop X button).
  END_STOP = 5,

  // Page load ended due to closing the tab or browser.
  END_CLOSE = 6,

  // The provisional load for this page load failed before committing.
  END_PROVISIONAL_LOAD_FAILED = 7,

  // The render process hosting the page terminated unexpectedly.
  END_RENDER_PROCESS_GONE = 8,

  // We don't know why the page load ended. This is the value we assign to a
  // terminated provisional load if the only signal we get is the load finished
  // without committing, either without error or with net::ERR_ABORTED.
  END_OTHER = 9,

  PAGE_END_REASON_COUNT
};

// Information related to failed provisional loads.
struct FailedProvisionalLoadInfo {
  FailedProvisionalLoadInfo(base::TimeDelta interval, net::Error error);
  ~FailedProvisionalLoadInfo();

  base::TimeDelta time_to_failed_provisional_load;
  net::Error error;
};

// Information related to whether an associated action, such as a navigation or
// an abort, was initiated by a user. Clicking a link or tapping on a UI
// element are examples of user initiation actions.
struct UserInitiatedInfo {
  static UserInitiatedInfo NotUserInitiated() {
    return UserInitiatedInfo(false, false, false);
  }

  static UserInitiatedInfo BrowserInitiated() {
    return UserInitiatedInfo(true, false, false);
  }

  static UserInitiatedInfo RenderInitiated(bool user_gesture,
                                           bool user_input_event) {
    return UserInitiatedInfo(false, user_gesture, user_input_event);
  }

  // Whether the associated action was initiated from the browser process, as
  // opposed to from the render process. We generally assume that all actions
  // initiated from the browser process are user initiated.
  bool browser_initiated;

  // Whether the associated action was initiated by a user, according to user
  // gesture tracking in content and Blink, as reported by NavigationHandle.
  bool user_gesture;

  // Whether the associated action was initiated by a user, based on our
  // heuristic-driven implementation that tests to see if there was an input
  // event that happened shortly before the given action.
  bool user_input_event;

 private:
  UserInitiatedInfo(bool browser_initiated,
                    bool user_gesture,
                    bool user_input_event)
      : browser_initiated(browser_initiated),
        user_gesture(user_gesture),
        user_input_event(user_input_event) {}
};

struct PageLoadExtraInfo {
  PageLoadExtraInfo(
      base::TimeTicks navigation_start,
      const base::Optional<base::TimeDelta>& first_background_time,
      const base::Optional<base::TimeDelta>& first_foreground_time,
      bool started_in_foreground,
      UserInitiatedInfo user_initiated_info,
      const GURL& url,
      const GURL& start_url,
      bool did_commit,
      PageEndReason page_end_reason,
      UserInitiatedInfo page_end_user_initiated_info,
      const base::Optional<base::TimeDelta>& page_end_time,
      const mojom::PageLoadMetadata& main_frame_metadata,
      const mojom::PageLoadMetadata& subframe_metadata,
      ukm::SourceId source_id);

  // Simplified version of the constructor, intended for use in tests.
  static PageLoadExtraInfo CreateForTesting(const GURL& url,
                                            bool started_in_foreground);

  PageLoadExtraInfo(const PageLoadExtraInfo& other);

  ~PageLoadExtraInfo();

  // The time the navigation was initiated.
  const base::TimeTicks navigation_start;

  // The first time that the page was backgrounded since the navigation started.
  const base::Optional<base::TimeDelta> first_background_time;

  // The first time that the page was foregrounded since the navigation started.
  const base::Optional<base::TimeDelta> first_foreground_time;

  // True if the page load started in the foreground.
  const bool started_in_foreground;

  // Whether the page load was initiated by a user.
  const UserInitiatedInfo user_initiated_info;

  // Most recent URL for this page. Can be updated at navigation start, upon
  // redirection, and at commit time.
  const GURL url;

  // The URL that started the navigation, before redirects.
  const GURL start_url;

  // Whether the navigation for this page load committed.
  const bool did_commit;

  // The reason the page load ended. If the page is still active,
  // |page_end_reason| will be |END_NONE|. |page_end_time| contains the duration
  // of time until the cause of the page end reason was encountered.
  const PageEndReason page_end_reason;

  // Whether the end reason for this page load was user initiated. For example,
  // if
  // this page load was ended due to a new navigation, this field tracks whether
  // that new navigation was user-initiated. This field is only useful if this
  // page load's end reason is a value other than END_NONE. Note that this
  // value is currently experimental, and is subject to change. In particular,
  // this field is not currently set for some end reasons, such as stop and
  // close, since we don't yet have sufficient instrumentation to know if a stop
  // or close was caused by a user action.
  //
  // TODO(csharrison): If more metadata for end reasons is needed we should
  // provide a
  // better abstraction. Note that this is an approximation.
  UserInitiatedInfo page_end_user_initiated_info;

  // Total lifetime of the page from the user's standpoint, starting at
  // navigation start. The page lifetime ends when the first of the following
  // events happen:
  // * the load of the main resource fails
  // * the page load is stopped
  // * the tab hosting the page is closed
  // * the render process hosting the page goes away
  // * a new navigation which later commits is initiated in the same tab
  // This field will not be set if the page is still active and hasn't yet
  // finished.
  const base::Optional<base::TimeDelta> page_end_time;

  // Extra information supplied to the page load metrics system from the
  // renderer for the main frame.
  const mojom::PageLoadMetadata main_frame_metadata;

  // PageLoadMetadata for subframes of the current page load.
  const mojom::PageLoadMetadata subframe_metadata;

  // UKM SourceId for the current page load.
  const ukm::SourceId source_id;
};

// Container for various information about a completed request within a page
// load.
struct ExtraRequestCompleteInfo {
  ExtraRequestCompleteInfo(
      const GURL& url,
      const net::HostPortPair& host_port_pair,
      int frame_tree_node_id,
      bool was_cached,
      int64_t raw_body_bytes,
      int64_t original_network_content_length,
      std::unique_ptr<data_reduction_proxy::DataReductionProxyData>
          data_reduction_proxy_data,
      content::ResourceType detected_resource_type,
      int net_error,
      std::unique_ptr<net::LoadTimingInfo> load_timing_info);

  ExtraRequestCompleteInfo(const ExtraRequestCompleteInfo& other);

  ~ExtraRequestCompleteInfo();

  // The URL for the request.
  const GURL url;

  // The host (IP address) and port for the request.
  const net::HostPortPair host_port_pair;

  // The frame tree node id that initiated the request.
  const int frame_tree_node_id;

  // True if the resource was loaded from cache.
  const bool was_cached;

  // The number of body (not header) prefilter bytes.
  const int64_t raw_body_bytes;

  // The number of body (not header) bytes that the data reduction proxy saw
  // before it compressed the requests.
  const int64_t original_network_content_length;

  // Data related to data saver.
  const std::unique_ptr<data_reduction_proxy::DataReductionProxyData>
      data_reduction_proxy_data;

  // The type of the request as gleaned from the mime type.  This may
  // be more accurate than the type in the ExtraRequestStartInfo since we can
  // examine the type headers that arrived with the request.  During XHRs, we
  // sometimes see resources come back as a different type than we expected.
  const content::ResourceType resource_type;

  // The network error encountered by the request, as defined by
  // net/base/net_error_list.h. If no error was encountered, this value will be
  // 0.
  const int net_error;

  // Additional timing information.
  const std::unique_ptr<net::LoadTimingInfo> load_timing_info;
};

// Interface for PageLoadMetrics observers. All instances of this class are
// owned by the PageLoadTracker tracking a page load.
class PageLoadMetricsObserver {
 public:
  // ObservePolicy is used as a return value on some PageLoadMetricsObserver
  // callbacks to indicate whether the observer would like to continue observing
  // metric callbacks. Observers that wish to continue observing metric
  // callbacks should return CONTINUE_OBSERVING; observers that wish to stop
  // observing callbacks should return STOP_OBSERVING. Observers that return
  // STOP_OBSERVING may be deleted.
  enum ObservePolicy {
    CONTINUE_OBSERVING,
    STOP_OBSERVING,
  };

  using FrameTreeNodeId = int;

  virtual ~PageLoadMetricsObserver() {}

  static bool IsStandardWebPageMimeType(const std::string& mime_type);

  // The page load started, with the given navigation handle.
  // currently_committed_url contains the URL of the committed page load at the
  // time the navigation for navigation_handle was initiated, or the empty URL
  // if there was no committed page load at the time the navigation was
  // initiated.
  virtual ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                                const GURL& currently_committed_url,
                                bool started_in_foreground);

  // OnRedirect is triggered when a page load redirects to another URL.
  // The navigation handle holds relevant data for the navigation, but will
  // be destroyed soon after this call. Don't hold a reference to it. This can
  // be called multiple times.
  virtual ObservePolicy OnRedirect(
      content::NavigationHandle* navigation_handle);

  // OnCommit is triggered when a page load commits, i.e. when we receive the
  // first data for the request. The navigation handle holds relevant data for
  // the navigation, but will be destroyed soon after this call. Don't hold a
  // reference to it.
  // Observers that return STOP_OBSERVING will not receive any additional
  // callbacks, and will be deleted after invocation of this method returns.
  virtual ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                                 ukm::SourceId source_id);

  // OnDidInternalNavigationAbort is triggered when the main frame navigation
  // aborts with HTTP responses that don't commit, such as HTTP 204 responses
  // and downloads. Note that |navigation_handle| will be destroyed
  // soon after this call. Don't hold a reference to it.
  virtual void OnDidInternalNavigationAbort(
      content::NavigationHandle* navigation_handle) {}

  // OnDidFinishSubFrameNavigation is triggered when a sub-frame of the
  // committed page has finished navigating. It has either committed, aborted,
  // was a same document navigation, or has been replaced. It is up to the
  // observer to query |navigation_handle| to determine which happened. Note
  // that |navigation_handle| will be destroyed soon after this call. Don't
  // hold a reference to it.
  virtual void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) {}

  // OnCommitSameDocumentNavigation is triggered when a same-document navigation
  // commits within the main frame of the current page. Note that
  // |navigation_handle| will be destroyed soon after this call. Don't hold a
  // reference to it.
  virtual void OnCommitSameDocumentNavigation(
      content::NavigationHandle* navigation_handle) {}

  // OnHidden is triggered when a page leaves the foreground. It does not fire
  // when a foreground page is permanently closed; for that, listen to
  // OnComplete instead.
  virtual ObservePolicy OnHidden(const mojom::PageLoadTiming& timing,
                                 const PageLoadExtraInfo& extra_info);

  // OnShown is triggered when a page is brought to the foreground. It does not
  // fire when the page first loads; for that, listen for OnStart instead.
  virtual ObservePolicy OnShown();

  // Called before OnCommit. The observer should return whether it wishes to
  // observe navigations whose main resource has MIME type |mine_type|. The
  // default is to observe HTML and XHTML only. Note that PageLoadTrackers only
  // track XHTML, HTML, and MHTML (related/multipart).
  virtual ObservePolicy ShouldObserveMimeType(
      const std::string& mime_type) const;

  // The callbacks below are only invoked after a navigation commits, for
  // tracked page loads. Page loads that don't meet the criteria for being
  // tracked at the time a navigation commits will not receive any of the
  // callbacks below.

  // OnTimingUpdate is triggered when an updated PageLoadTiming is available at
  // the page (page is essentially main frame, with merged values across all
  // frames for some paint timing values) or subframe level. This method may be
  // called multiple times over the course of the page load. This method is
  // currently only intended for use in testing. Most implementers should
  // implement one of the On* callbacks, such as OnFirstContentfulPaint or
  // OnDomContentLoadedEventStart. Please email loading-dev@chromium.org if you
  // intend to override this method.
  //
  // If |subframe_rfh| is nullptr, the update took place in the main frame.
  virtual void OnTimingUpdate(content::RenderFrameHost* subframe_rfh,
                              const mojom::PageLoadTiming& timing,
                              const PageLoadExtraInfo& extra_info) {}

  // OnUserInput is triggered when a new user input is passed in to
  // web_contents. Contains a TimeDelta from navigation start.
  virtual void OnUserInput(const blink::WebInputEvent& event) {}

  // The following methods are invoked at most once, when the timing for the
  // associated event first becomes available.
  virtual void OnDomContentLoadedEventStart(
      const mojom::PageLoadTiming& timing,
      const PageLoadExtraInfo& extra_info) {}
  virtual void OnLoadEventStart(const mojom::PageLoadTiming& timing,
                                const PageLoadExtraInfo& extra_info) {}
  virtual void OnFirstLayout(const mojom::PageLoadTiming& timing,
                             const PageLoadExtraInfo& extra_info) {}
  virtual void OnParseStart(const mojom::PageLoadTiming& timing,
                            const PageLoadExtraInfo& extra_info) {}
  virtual void OnParseStop(const mojom::PageLoadTiming& timing,
                           const PageLoadExtraInfo& extra_info) {}

  // On*PaintInPage(...) are invoked when the first relevant paint in the page,
  // across all frames, is observed.
  virtual void OnFirstPaintInPage(const mojom::PageLoadTiming& timing,
                                  const PageLoadExtraInfo& extra_info) {}
  virtual void OnFirstTextPaintInPage(const mojom::PageLoadTiming& timing,
                                      const PageLoadExtraInfo& extra_info) {}
  virtual void OnFirstImagePaintInPage(const mojom::PageLoadTiming& timing,
                                       const PageLoadExtraInfo& extra_info) {}
  virtual void OnFirstContentfulPaintInPage(
      const mojom::PageLoadTiming& timing,
      const PageLoadExtraInfo& extra_info) {}

  // Unlike other paint callbacks, OnFirstMeaningfulPaintInMainFrameDocument is
  // tracked per document, and is reported for the main frame document only.
  virtual void OnFirstMeaningfulPaintInMainFrameDocument(
      const mojom::PageLoadTiming& timing,
      const PageLoadExtraInfo& extra_info) {}

  // These signatures are used to report the last candidate for each of FCP++
  // metrics. They will be invoked at the end of page load's life time, around
  // the time of the OnComplete callback.
  virtual void OnLargestImagePaintInMainFrameDocument(
      const mojom::PageLoadTiming& last_candidate,
      const page_load_metrics::PageLoadExtraInfo& info) {}
  virtual void OnLastImagePaintInMainFrameDocument(
      const mojom::PageLoadTiming& last_candidate,
      const page_load_metrics::PageLoadExtraInfo& info) {}
  virtual void OnLargestTextPaintInMainFrameDocument(
      const mojom::PageLoadTiming& last_candidate,
      const page_load_metrics::PageLoadExtraInfo& info) {}
  virtual void OnLastTextPaintInMainFrameDocument(
      const mojom::PageLoadTiming& last_candidate,
      const page_load_metrics::PageLoadExtraInfo& info) {}

  virtual void OnPageInteractive(const mojom::PageLoadTiming& timing,
                                 const PageLoadExtraInfo& extra_info) {}

  virtual void OnFirstInputInPage(const mojom::PageLoadTiming& timing,
                                  const PageLoadExtraInfo& extra_info) {}

  // Invoked when there is a change in either the main_frame_metadata or the
  // subframe_metadata's loading behavior_flags.
  virtual void OnLoadingBehaviorObserved(const PageLoadExtraInfo& extra_info) {}

  // Invoked when new use counter features are observed across all frames.
  virtual void OnFeaturesUsageObserved(const mojom::PageLoadFeatures& features,
                                       const PageLoadExtraInfo& extra_info) {}

  // Invoked when there is data use for loading a resource on the page
  // across all frames. This only contains resources that have had new
  // data use since the last callback.
  virtual void OnResourceDataUseObserved(
      const std::vector<mojom::ResourceDataUpdatePtr>& resources) {}

  // Invoked when a media element starts playing.
  virtual void MediaStartedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& video_type,
      bool is_in_main_frame) {}

  // Invoked when the UMA metrics subsystem is persisting metrics as the
  // application goes into the background, on platforms where the browser
  // process may be killed after backgrounding (Android). Implementers should
  // persist any metrics that have been buffered in memory in this callback, as
  // the application may be killed at any time after this method is invoked
  // without further notification. Note that this may be called both for
  // provisional loads as well as committed loads. Implementations that only
  // want to track committed loads should check whether extra_info.committed_url
  // is empty to determine if the load had committed. If the implementation
  // returns CONTINUE_OBSERVING, this method may be called multiple times per
  // observer, once for each time that the application enters the backround.
  //
  // The default implementation does nothing, and returns CONTINUE_OBSERVING.
  virtual ObservePolicy FlushMetricsOnAppEnterBackground(
      const mojom::PageLoadTiming& timing,
      const PageLoadExtraInfo& extra_info);

  // One of OnComplete or OnFailedProvisionalLoad is invoked for tracked page
  // loads, immediately before the observer is deleted. These callbacks will not
  // be invoked for page loads that did not meet the criteria for being tracked
  // at the time the navigation completed. The PageLoadTiming struct contains
  // timing data and the PageLoadExtraInfo struct contains other useful data
  // collected over the course of the page load. Most observers should not need
  // to implement these callbacks, and should implement the On* timing callbacks
  // instead.

  // OnComplete is invoked for tracked page loads that committed, immediately
  // before the observer is deleted. Observers that implement OnComplete may
  // also want to implement FlushMetricsOnAppEnterBackground, to avoid loss of
  // data if the application is killed while in the background (this happens
  // frequently on Android).
  virtual void OnComplete(const mojom::PageLoadTiming& timing,
                          const PageLoadExtraInfo& extra_info) {}

  // OnFailedProvisionalLoad is invoked for tracked page loads that did not
  // commit, immediately before the observer is deleted.
  virtual void OnFailedProvisionalLoad(
      const FailedProvisionalLoadInfo& failed_provisional_load_info,
      const PageLoadExtraInfo& extra_info) {}

  // Called whenever a request is loaded for this page load. This is restricted
  // to requests with HTTP or HTTPS only schemes.
  virtual void OnLoadedResource(
      const ExtraRequestCompleteInfo& extra_request_complete_info) {}

  // Called when the event corresponding to |event_key| occurs in this page
  // load.
  virtual void OnEventOccurred(const void* const event_key) {}
};

}  // namespace page_load_metrics

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_OBSERVER_H_
