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

namespace content {
class Page;
}

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
  // TODO(https://crbug.com/1278634): return a TriggerResult enum so that
  // callers can record some metrics if they want.
  void StartPrerenderAutocompleteMatch(const AutocompleteMatch& match);

  // The entry of direct url input prerender.
  // Calling this method will return WeakPtr of the started prerender, and lead
  // to the cancellation of the previous prerender if the given url is different
  // from the on-going one. If the url given is already on-going, this function
  // will return nullptr instead.
  // TODO(https://crbug.com/1278634): Merge the start method with DSE interface
  // using AutocompleteMatch as the parameter instead of GURL.
  base::WeakPtr<content::PrerenderHandle> StartPrerenderDirectUrlInput(
      const GURL& prerendering_url);

  void CancelPrerenderDirectUrlInput();

  content::PrerenderHandle* search_prerender_handle_for_testing() {
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
  // Stores the search terms that `search_prerender_handle_` is prerendering.
  std::u16string prerendered_search_terms_;
  bool skip_template_url_service_for_testing_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
