// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_

#include "components/omnibox/browser/autocomplete_match.h"
#include "components/search_engines/template_url.h"
#include "content/public/browser/prerender_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class Page;
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
  void PrimaryPageChanged(content::Page& page) override;

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

  content::PrerenderHandle* search_prerender_handle() {
    return search_prerender_handle_.get();
  }

  void set_skip_template_url_service_for_testing() {
    skip_template_url_service_for_testing_ = true;
  }

 private:
  explicit PrerenderManager(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PrerenderManager>;

  std::unique_ptr<content::PrerenderHandle> search_prerender_handle_;
  std::unique_ptr<content::PrerenderHandle> direct_url_input_prerender_handle_;

  // Stores the arguments of the search term that `search_prerender_handle_` is
  // prerendering.
  // TODO(https://crbug.com/1291147): This is a workaround to stop the location
  // bar from displaying the prefetch flag. This should be removed after we
  // confirm the prerendered documents update the url by theirselves.
  TemplateURLRef::SearchTermsArgs prerendered_search_terms_args_;

  bool skip_template_url_service_for_testing_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
