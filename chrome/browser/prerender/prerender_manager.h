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
  enum TriggerReason {
    kUrlInputPrediction = 0,
    kSearchSuggestion = 1,
  };

  PrerenderManager(const PrerenderManager&) = delete;
  PrerenderManager& operator=(const PrerenderManager&) = delete;

  ~PrerenderManager() override;

  // content::WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override;

  // The entry of prerender.
  // Calling this method will lead to the cancellation of the previous prerender
  // if the given url is differ from the ongoing one's.
  // TODO(https://crbug.com/1278634): return a TriggerResult enum so that
  // callers can record some metrics if they want.
  void Start(const GURL& prerendering_url, TriggerReason reason);

  content::PrerenderHandle* prerender_handle_for_testing() {
    return prerender_handle_.get();
  }

 private:
  explicit PrerenderManager(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PrerenderManager>;

  std::unique_ptr<content::PrerenderHandle> prerender_handle_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
