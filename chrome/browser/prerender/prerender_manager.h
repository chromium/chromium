// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_

#include "components/omnibox/browser/autocomplete_match.h"
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

  // The entry of prerender.
  // Calling this method will lead to the cancellation of the previous prerender
  // if the given `match`'s search terms differ from the ongoing one's.
  void StartPrerenderSearchSuggestion(const AutocompleteMatch& match);

  // The entry of direct url input prerender.
  // Calling this method will return WeakPtr of the started prerender, and lead
  // to the cancellation of the previous prerender if the given url is different
  // from the on-going one. If the url given is already on-going, this function
  // will return the weak pointer to the on-going prerender handle.
  // TODO(https://crbug.com/1278634): Merge the start method with DSE interface
  // using AutocompleteMatch as the parameter instead of GURL.
  base::WeakPtr<content::PrerenderHandle> StartPrerenderDirectUrlInput(
      const GURL& prerendering_url);

  // Returns true if the current tab prerendered a search result for omnibox
  // inputs.
  bool HasSearchResultPagePrerendered() const;

  // Returns the prerendered search terms if search_prerender_task_ exists.
  // Returns empty string otherwise.
  const std::u16string GetPrerenderSearchTermForTesting() const;

  void set_skip_template_url_service_for_testing() {
    skip_template_url_service_for_testing_ = true;
  }

 private:
  class SearchPrerenderTask;

  explicit PrerenderManager(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PrerenderManager>;

  void ResetPrerenderHandlesOnPrimaryPageChanged(
      content::NavigationHandle* navigation_handle);

  // Stores the prerender which serves for search results. It is responsible for
  // tracking a started search prerender, and it keeps alive even if the
  // prerender has been destroyed by the timer. With its help, PrerenderManager
  // can record the prediction regardless whether a prerender is expired or not.
  std::unique_ptr<SearchPrerenderTask> search_prerender_task_;

  std::unique_ptr<content::PrerenderHandle> direct_url_input_prerender_handle_;

  bool skip_template_url_service_for_testing_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
