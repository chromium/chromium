// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_ZERO_SUGGEST_PREFETCH_ZERO_SUGGEST_PREFETCH_TAB_HELPER_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_ZERO_SUGGEST_PREFETCH_ZERO_SUGGEST_PREFETCH_TAB_HELPER_H_

#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"

// Prefetches zero-prefix suggestions on opening or switching to a New Tab Page.
// It is owned by the tab's TabFeatures.
class ZeroSuggestPrefetchTabHelper : public content::WebContentsObserver,
                                     public TabStripModelObserver {
 public:
  explicit ZeroSuggestPrefetchTabHelper(content::WebContents* web_contents);
  ~ZeroSuggestPrefetchTabHelper() override;

  ZeroSuggestPrefetchTabHelper(const ZeroSuggestPrefetchTabHelper&) = delete;
  ZeroSuggestPrefetchTabHelper& operator=(const ZeroSuggestPrefetchTabHelper&) =
      delete;

  // content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  // Starts an autocomplete prefetch request so that zero-prefix providers can
  // optionally start a prefetch request to warm up the their underlying
  // service(s) and/or optionally cache their otherwise async response.
  void StartPrefetch();
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_ZERO_SUGGEST_PREFETCH_ZERO_SUGGEST_PREFETCH_TAB_HELPER_H_
