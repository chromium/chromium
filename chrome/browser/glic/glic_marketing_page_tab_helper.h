// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_MARKETING_PAGE_TAB_HELPER_H_
#define CHROME_BROWSER_GLIC_GLIC_MARKETING_PAGE_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"

namespace glic {

// A tab helper that auto-opens the Glic UI (e.g. bottom sheet, side panel) when
// the user navigates to a configured promotional URL.
//
class GlicMarketingPageTabHelper : public content::WebContentsObserver {
 public:
  explicit GlicMarketingPageTabHelper(content::WebContents* web_contents);
  GlicMarketingPageTabHelper(const GlicMarketingPageTabHelper&) = delete;
  GlicMarketingPageTabHelper& operator=(const GlicMarketingPageTabHelper&) =
      delete;
  ~GlicMarketingPageTabHelper() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_MARKETING_PAGE_TAB_HELPER_H_
