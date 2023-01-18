// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_REQUEST_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_REQUEST_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/state_transitions.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

class PrerenderManager;
class Profile;
class SearchPrefetchURLLoader;
class StreamingSearchPrefetchURLLoader;
namespace content {
class PreloadingAttempt;
enum class PreloadingTriggeringOutcome;
enum class PreloadingFailureReason;
}  // namespace content

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Any updates to this class need to be propagated to enums.xml.
enum class SearchPrefetchStatus {
  // The request has not started yet. This status should ideally never be
  // recorded as Start() should be called on the same stack as creating the
  // fetcher (as of now).
  kNotStarted = 0,
  // The request is on the network and may move to any other state.
  kInFlight = 1,
  // The request can be served to the navigation stack, but may still encounter
  // errors and move to |kRequestFailed| or it may complete and move to
  // |kComplete|. It may also move to |kCanBeServedAndUserClicked| when the user
  // navigates to the result in omnibox or |kRequestCancelled| if the user
  // closes omnibox.
  kCanBeServed = 2,
  // The request can be served to the navigation stack, and is marked as being
  // clicked by the user. At this point, it may move to |kComplete| or
  // |kRequestFailed|.
  kCanBeServedAndUserClicked = 3,
  // The request can be served to the navigation stack, and has fully streamed
  // the response with no errors.
  kComplete = 4,
  // The request hit an error and cannot be served.
  kRequestFailed = 5,
  // The request was cancelled before completion. This is a terminal state.
  kRequestCancelled = 6,
  // The request was served to the navigation stack. This is a terminal state.
  kPrefetchServedForRealNavigation = 7,
  // The request was served to the prerender navigation stack. It may move to
  // |kPrerenderedAndClicked| when the user navigates to the result in omnibox
  // or |kRequestCancelled| if the user closes omnibox.
  kPrerendered = 8,
  // Similar to |kCanBeServedAndUserClicked|, the request was served to the
  // prerender navigation stack, and is marked as being
  // clicked by the user. It is expected to move to |kPrerenderActivated| after
  // the corresponding prerender is fully activated by the user.
  kPrerenderedAndClicked = 9,
  // The request was served to the prerender navigation stack, and the prerender
  // page is fully activated by the user. This is a terminal state.
  kPrerenderActivated = 10,
  kMaxValue = kPrerenderActivated,
};

// A class representing a prefetch used by the Search Prefetch Service.
// It plays the following roles to support search preloading.
// - Preparing a resource request to prefetch a search page.
// - Starting prerendering upon the request succeeding to upgrade prefetch to
//   prerender after the Search Prefetch Service tells it that the prefetched
//   term is prerenderable.
// - A container for a StreamingSearchPrefetchURLLoader, to support
// |TakeSearchPrefetchURLLoader()|
//   more easily.
class SearchPrefetchRequest {
 public:
  SearchPrefetchRequest(const GURL& canonical_search_url,
                        const GURL& prefetch_url,
                        bool navigation_prefetch,
                        content::PreloadingAttempt* prefetch_preloading_attempt,
                        base::OnceCallback<void(bool)> report_error_callback);
  ~SearchPrefetchRequest();

  SearchPrefetchRequest(const SearchPrefetchRequest&) = delete;
  SearchPrefetchRequest& operator=(const SearchPrefetchRequest&) = delete;

  // The NTA for any search prefetch request.
  static net::NetworkTrafficAnnotationTag NetworkAnnotationForPrefetch();

  // Starts the network request to prefetch |prefetch_url_|. Sets various fields
  // on a resource request and calls |StartPrefetchRequestInternal()|. Returns
  // |false| if the request is not started (i.e., it would be deferred by
  // throttles).
  bool StartPrefetchRequest(Profile* profile);

  // Returns true if this request should be canceled when the Autocomplete
  // suggestion no longer lists this search prefetch.
  bool ShouldBeCancelledOnResultChanges() const;

  // Marks a prefetch as canceled and stops any ongoing fetch.
  void CancelPrefetch();

  // Called when SearchPrefetchService receives the hint that this prefetch
  // request can be upgraded to a prerender attempt.
  void MaybeStartPrerenderSearchResult(PrerenderManager& prerender_manager,
                                       const GURL& prerender_url,
                                       content::PreloadingAttempt& attempt);

  // Called when the prefetch encounters an error.
  void ErrorEncountered();

  // Called on the URL loader receives servable response.
  void OnServableResponseCodeReceived();

  // Update the status when the request is serveable.
  void MarkPrefetchAsServable();

  // Update the status when the request is complete.
  void MarkPrefetchAsComplete();

  // Update the status when the relevant search item is clicked in omnibox.
  void MarkPrefetchAsClicked();

  // Update the status when the request is actually served to the navigation
  // stack of a real navigation request.
  void MarkPrefetchAsServed();

  // Updates the status when the request is served to a prerendering navigation
  // stack. Note that after this point, this request cannot be served to a real
  // navigation anymore.
  void MarkPrefetchAsPrerendered();

  // Updates the status when the prerendering page it is serving was activated.
  void MarkPrefetchAsPrerenderActivated();

  // Called when AutocompleteMatches changes. It resets PrerenderUpgrader.
  // And if the AutocompleteMatches suggests to prerender a search result,
  // `MaybeStartPrerenderSearchResult` will be called soon.
  void ResetPrerenderUpgrader();

  // Record the time at which the user clicked a suggestion matching this
  // prefetch.
  void RecordClickTime();

  // Takes ownership of underlying data/objects needed to serve the response.
  std::unique_ptr<SearchPrefetchURLLoader> TakeSearchPrefetchURLLoader();

  // Whether the request was started as a navigation prefetch (as opposed to a
  // suggestion prefetch).
  bool navigation_prefetch() const { return navigation_prefetch_; }

  SearchPrefetchStatus current_status() const { return current_status_; }

  const GURL& prefetch_url() const { return prefetch_url_; }

  // Sets `prefetch_preloading_attempt_` PreloadingFailureReason to `reason` if
  // exists.
  void SetPrefetchAttemptFailureReason(content::PreloadingFailureReason reason);

 private:
  // Starts and begins processing |resource_request|.
  void StartPrefetchRequestInternal(
      Profile* profile,
      std::unique_ptr<network::ResourceRequest> resource_request,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      base::OnceCallback<void(bool)> report_error_callback);

  // Stops the on-going prefetch and should mark |current_status_|
  // appropriately.
  void StopPrefetch();

  // Cancels ongoing and pending prerender.
  void StopPrerender();

  // Updates the `current_status_` to status.
  void SetSearchPrefetchStatus(SearchPrefetchStatus status);

  // Sets the PreloadingTriggeringOutcome for `prefetch_preloading_attempt_` to
  // `outcome`.
  void SetPrefetchAttemptTriggeringOutcome(
      content::PreloadingTriggeringOutcome outcome);

  // Whether the request has received a servable response. See
  // `CanServePrefetchRequest` in ./streaming_search_prefetch_url_loader.cc for
  // the definition of servable response.
  bool servable_response_code_received_ = false;

  SearchPrefetchStatus current_status_ = SearchPrefetchStatus::kNotStarted;

  // The canonical representation of the search suggestion including query,
  // intent, and extra parameters that can alter the Search page.
  const GURL canonical_search_url_;

  // The URL to prefetch the search terms from.
  GURL prefetch_url_;

  // The URL to prerender the search terms from.
  // `prerender_url_` can be different from `prefetch_url_`. The latter is used
  // to send network requests, so it may contain a special parameter for the
  // server to recognize that it is a prefetch request, but the former does not
  // send network requests, i.e. this parameter is not required.
  GURL prerender_url_;

  // Whether this is for a navigation-time prefetch.
  bool navigation_prefetch_;

  std::unique_ptr<net::NetworkTrafficAnnotationTag> network_traffic_annotation_;

  // The ongoing prefetch request. Null before and after the fetch.
  std::unique_ptr<StreamingSearchPrefetchURLLoader> streaming_url_loader_;

  // Once set, this is used to log the metrics corresponding to the prefetch
  // attempt. Please note this is different from `prerender_preloading_attempt_`
  // which is for corresponding prerender attempt. We store the WeakPtr because
  // it is possible that the PreloadingAttempt can be deleted (on navigation) or
  // not created (when no WebContents is present) before search prefetch uses
  // it.
  base::WeakPtr<content::PreloadingAttempt> prefetch_preloading_attempt_;

  // Called when there is a network/server error on the prefetch request.
  base::OnceCallback<void(bool)> report_error_callback_;

  // The time at which the prefetched URL was clicked in the Omnibox.
  base::TimeTicks time_clicked_;

  // Once set, this request will trigger search prerender upon receiving success
  // response.
  base::WeakPtr<PrerenderManager> prerender_manager_;

  // Once set, this PreloadingAttempt corresponding to prerender attempt will be
  // passed to log various metrics. We store WeakPtr as prerender can be deleted
  // before we receive a prefetch response or the prerender is not created.
  base::WeakPtr<content::PreloadingAttempt> prerender_preloading_attempt_;

  base::raw_ptr<Profile> profile_;
};

// Used when DCHECK_STATE_TRANSITION triggers.
std::ostream& operator<<(std::ostream& o, const SearchPrefetchStatus& s);

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_REQUEST_H_
