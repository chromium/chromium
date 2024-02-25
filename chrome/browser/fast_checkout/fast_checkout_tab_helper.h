// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_TAB_HELPER_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// A tab helper that listens to navigations, uses the commerce heuristics
// framework to decide whether the URL indicates that the user is approaching
// checkout and, if so, ask the `FastCheckoutCapabilitiesFetcher` to prepare
// its cache.
class FastCheckoutTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<FastCheckoutTabHelper> {
 public:
  ~FastCheckoutTabHelper() override;
  FastCheckoutTabHelper(const FastCheckoutTabHelper&) = delete;
  FastCheckoutTabHelper& operator=(const FastCheckoutTabHelper&) = delete;

  // WebContentsObserver:
  // Analyses the URL using commerce heuristics to decide whether to ask the
  // `FastCheckoutCapabilitiesFetcher` to fetch availability for the URL.
  // The analysis is done on navigation start to allow the capabilities fetcher
  // enough time.
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  explicit FastCheckoutTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<FastCheckoutTabHelper>;

  void FetchCapabilities(const GURL& url);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_TAB_HELPER_H_
