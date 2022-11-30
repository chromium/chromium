// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_ZERO_SUGGEST_PREFETCH_ZERO_SUGGEST_PREFETCH_TAB_HELPER_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_ZERO_SUGGEST_PREFETCH_ZERO_SUGGEST_PREFETCH_TAB_HELPER_H_

#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// Prefetches zero-prefix suggestions on opening or switching to a New Tab Page.
class ZeroSuggestPrefetchTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ZeroSuggestPrefetchTabHelper>,
      public TabStripModelObserver {
 public:
  ~ZeroSuggestPrefetchTabHelper() override;

  ZeroSuggestPrefetchTabHelper(const ZeroSuggestPrefetchTabHelper&) = delete;
  ZeroSuggestPrefetchTabHelper& operator=(const ZeroSuggestPrefetchTabHelper&) =
      delete;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  explicit ZeroSuggestPrefetchTabHelper(content::WebContents* web_contents);

  // Starts an autocomplete prefetch request so that zero-prefix providers can
  // optionally start a prefetch request to warm up the their underlying
  // service(s) and/or optionally cache their otherwise async response.
  void StartPrefetch();

  friend class content::WebContentsUserData<ZeroSuggestPrefetchTabHelper>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_ZERO_SUGGEST_PREFETCH_ZERO_SUGGEST_PREFETCH_TAB_HELPER_H_
