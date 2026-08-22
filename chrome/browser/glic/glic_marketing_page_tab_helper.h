// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_MARKETING_PAGE_TAB_HELPER_H_
#define CHROME_BROWSER_GLIC_GLIC_MARKETING_PAGE_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace glic {

// A tab helper that auto-opens the Glic UI (e.g. bottom sheet, side panel) when
// the user navigates to a configured promotional URL.
//
class GlicMarketingPageTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<GlicMarketingPageTabHelper> {
 public:
  ~GlicMarketingPageTabHelper() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  explicit GlicMarketingPageTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<GlicMarketingPageTabHelper>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_MARKETING_PAGE_TAB_HELPER_H_
