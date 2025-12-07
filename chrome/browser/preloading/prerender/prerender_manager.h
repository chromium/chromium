// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PRERENDER_PRERENDER_MANAGER_H_
#define CHROME_BROWSER_PRELOADING_PRERENDER_PRERENDER_MANAGER_H_

#include <optional>
#include <string>

#include "chrome/browser/preloading/preloading_features.h"
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

// If you change this, please follow the process in
// go/preloading-dashboard-updates to update the mapping reflected in dashboard,
// or if you are not a Googler, please file an FYI bug on https://crbug.new with
// component Internals>Preload.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(PrerenderPredictionStatus)
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
// LINT.ThenChange()

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

  // Maybe start prerendering a prewarm page if we haven't prewarm it yet for
  // the underlying WebContents. Returns true if a new prerender is started.
  // TODO(https://crbug.com/423465927): Decide a better timing to close.
  bool MaybeStartPrewarmSearchResult();

  // Deletes the existing prewarm page to start another one for testing.
  void StopPrewarmSearchResultForTesting();

  // Sets the prewarm page URL for testing as it's difficult to set the testing
  // server's URL as a Finch parameter in the tests.
  void SetPrewarmUrlForTesting(const GURL& url);

  // Calling this method will lead to the cancellation of the previous prerender
  // if the given `canonical_search_url` differs from the ongoing one's.
  void StartPrerenderSearchResult(
      const GURL& canonical_search_url,
      const GURL& prerendering_url,
      base::WeakPtr<content::PreloadingAttempt> attempt);

  // Cancels the prerender that is prerendering the given
  // `canonical_search_url`.
  void StopPrerenderSearchResult(const GURL& canonical_search_url);

  // The entry of direct url input prerender.
  // Calling this method will return WeakPtr of the started prerender, and lead
  // to the cancellation of the previous prerender if the given url is different
  // from the on-going one. If the url given is already on-going, this function
  // will return the weak pointer to the on-going prerender handle.
  // PreloadingAttempt represents the attempt corresponding to this prerender to
  // log the necessary metrics.
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

 private:
  class SearchPrerenderTask;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(PrewarmDecision)
  enum class PrewarmDecision {
    kReady = 0,
    kAlreadyExists = 1,
    kDisabled = 2,
    kInHeadlessMode = 3,
    kDebuggerAttached = 4,
    kInvalidUrl = 5,
    kNoTemplateUrlService = 6,
    kNoDefaultSearchProvider = 7,
    kNotSameOriginWithDSE = 8,
    kInPictureInPicture = 9,
    kInIsolatedWebApp = 10,
    kInKioskSession = 11,
    kMaxValue = kInKioskSession,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/navigation/enums.xml:PrerenderPrewarmDecision)

  explicit PrerenderManager(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PrerenderManager>;

  void ResetPrerenderHandlesOnPrimaryPageChanged(
      content::NavigationHandle* navigation_handle);

  // Decides if prewarm should be triggered. If not, returns the reason why.
  // Otherwise, returns kReady and sets `prewarm_url`.
  PrewarmDecision ShouldPrewarm(GURL& prewarm_url);

  void OnPrerenderNavigationHandle(
      content::NavigationHandle& navigation_handle);

  std::unique_ptr<content::PrerenderHandle> search_prewarm_handle_;
  std::optional<GURL> prewarm_url_for_testing_;

  // Stores the prerender which serves for search results. It is responsible for
  // tracking a started search prerender, and informing `SearchPrefetchService`
  // of the prerender status.
  std::unique_ptr<SearchPrerenderTask> search_prerender_task_;

  std::unique_ptr<content::PrerenderHandle> direct_url_input_prerender_handle_;

  base::WeakPtrFactory<PrerenderManager> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PRELOADING_PRERENDER_PRERENDER_MANAGER_H_
