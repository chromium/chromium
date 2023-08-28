// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PRERENDER_PRERENDER_MANAGER_H_
#define CHROME_BROWSER_PRELOADING_PRERENDER_PRERENDER_MANAGER_H_

#include <string>

#include "components/omnibox/browser/autocomplete_match.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/prerender_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
}

namespace internal {
extern const char kHistogramPrerenderPredictionStatusDefaultSearchEngine[];
extern const char kHistogramPrerenderPredictionStatusDirectUrlInput[];
}  // namespace internal

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PrerenderPredictionStatus {
  // The prerender was not started at all for this omnibox interaction.
  kNotStarted = 0,
  // The prerender was cancelled since the prediction is updated.
  kCancelled = 1,
  // The prerender was unused.
  kUnused = 2,
  // The predicted URL was used.
  kHitFinished = 3,
  kMaxValue = kHitFinished,
};

// Manages running prerenders in the //chrome.
// Chrome manages running prerenders separately, as it prioritizes the latest
// prerender requests, while the //content prioritizes the earliest requests.
class PrerenderManager : public content::WebContentsObserver,
                         public content::WebContentsUserData<PrerenderManager> {
 public:
  PrerenderManager(const PrerenderManager&) = delete;
  PrerenderManager& operator=(const PrerenderManager&) = delete;

  ~PrerenderManager() override;

  // content::WebContentsObserver
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // The entry of Default Search Engine prerender. Calling this method will lead
  // to the cancellation of the previous prerender if the given `match`'s search
  // suggestion canonical URL differs from the ongoing one's.
  // TODO(https://crbug.com/1295170): Remove this method after Search prerender
  // work properly with Search prefetch.
  void StartPrerenderSearchSuggestion(const AutocompleteMatch& match,
                                      const GURL& canonical_search_url);

  // Calling this method will lead to the cancellation of the previous prerender
  // if the given `canonical_search_url` differs from the ongoing one's.
  void StartPrerenderSearchResult(
      const GURL& canonical_search_url,
      const GURL& prerendering_url,
      base::WeakPtr<content::PreloadingAttempt> attempt);

  // Cancels the prerender that is prerendering the given
  // `canonical_search_url`.
  // TODO(https://crbug.com/1295170): Use the creator's address to identify the
  // owner that can cancels the corresponding prerendering?
  void StopPrerenderSearchResult(const GURL& canonical_search_url);

  // The entry of bookmark prerender.
  // Calling this method will return WeakPtr of the started prerender, and lead
  // to the cancellation of the previous prerender if the given url is different
  // from the on-going one. If the url given is already on-going, this function
  // will return the weak pointer to the on-going prerender handle.
  base::WeakPtr<content::PrerenderHandle> StartPrerenderBookmark(
      const GURL& prerendering_url,
      content::PreloadingPredictor predictor);
  void StopPrerenderBookmark(
      base::WeakPtr<content::PrerenderHandle> prerender_handle);

  // The entry of new tab page prerender.
  // Calling this method will return WeakPtr of the started prerender, and lead
  // to the cancellation of the previous prerender if the given url is different
  // from the on-going one. If the url given is already on-going, this function
  // will return the weak pointer to the on-going prerender handle.
  base::WeakPtr<content::PrerenderHandle> StartPrerenderNewTabPage(
      const GURL& prerendering_url,
      content::PreloadingPredictor predictor);
  void StopPrerenderNewTabPage(
      base::WeakPtr<content::PrerenderHandle> prerender_handle);

  // The entry of direct url input prerender.
  // Calling this method will return WeakPtr of the started prerender, and lead
  // to the cancellation of the previous prerender if the given url is different
  // from the on-going one. If the url given is already on-going, this function
  // will return the weak pointer to the on-going prerender handle.
  // PreloadingAttempt represents the attempt corresponding to this prerender to
  // log the necessary metrics.
  // TODO(https://crbug.com/1278634): Merge the start method with DSE interface
  // using AutocompleteMatch as the parameter instead of GURL.
  base::WeakPtr<content::PrerenderHandle> StartPrerenderDirectUrlInput(
      const GURL& prerendering_url,
      content::PreloadingAttempt& preloading_attempt);

  // Returns true if the current tab prerendered a search result for omnibox
  // inputs.
  bool HasSearchResultPagePrerendered() const;

  base::WeakPtr<PrerenderManager> GetWeakPtr();

  // Returns the prerendered search terms if search_prerender_task_ exists.
  // Returns empty string otherwise.
  const GURL GetPrerenderCanonicalSearchURLForTesting() const;

  void set_skip_template_url_service_for_testing() {
    skip_template_url_service_for_testing_ = true;
  }

 private:
  class SearchPrerenderTask;

  explicit PrerenderManager(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PrerenderManager>;

  void ResetPrerenderHandlesOnPrimaryPageChanged(
      content::NavigationHandle* navigation_handle);

  // Maybe cancel the ongoing search prerender to restart a new one if this
  // finds the callers' intentions changed. The number of concurrence search
  // prerender is limited to 1, so it is needed to cancel the old one in order
  // to start a new one. Returns true if this finds the caller wants to
  // prerender another search result. Here `attempt` represents the
  // PreloadingAttempt corresponding to this prerender attempt to log metrics.
  bool ResetSearchPrerenderTaskIfNecessary(
      const GURL& canonical_search_url,
      base::WeakPtr<content::PreloadingAttempt> attempt);

  void StartPrerenderSearchResultInternal(
      const GURL& canonical_search_url,
      const GURL& prerendering_url,
      base::WeakPtr<content::PreloadingAttempt> attempt);

  // Stops search prefetch from being upgraded to prerender.
  void UnregisterSearchPrerender();

  // Stores the prerender which serves for search results. It is responsible for
  // tracking a started search prerender, and it keeps alive even if the
  // prerender has been destroyed by the timer. With its help, PrerenderManager
  // can record the prediction regardless whether a prerender is expired or not.
  std::unique_ptr<SearchPrerenderTask> search_prerender_task_;

  std::unique_ptr<content::PrerenderHandle> bookmark_prerender_handle_;

  std::unique_ptr<content::PrerenderHandle> new_tab_page_prerender_handle_;

  std::unique_ptr<content::PrerenderHandle> direct_url_input_prerender_handle_;

  bool skip_template_url_service_for_testing_ = false;

  base::WeakPtrFactory<PrerenderManager> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PRELOADING_PRERENDER_PRERENDER_MANAGER_H_
