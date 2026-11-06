// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_NAVIGATION_CAPTURE_GLIC_MARKETING_PAGE_TAB_HELPER_H_
#define CHROME_BROWSER_GLIC_NAVIGATION_CAPTURE_GLIC_MARKETING_PAGE_TAB_HELPER_H_

#include <memory>
#include <vector>

#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace glic {

// A tab helper that auto-opens the Glic UI (e.g. bottom sheet, side panel) when
// the user navigates to a configured promotional URL.
//
class GlicMarketingPageTabHelper : public content::WebContentsObserver {
 public:
  // Returns a helper for `web_contents`, or nullptr if the profile's Glic
  // provider doesn't support `GlicFeature::kMarketingAutoOpen` or no marketing
  // URLs are allowlisted.
  static std::unique_ptr<GlicMarketingPageTabHelper> MaybeCreate(
      content::WebContents* web_contents);

  GlicMarketingPageTabHelper(const GlicMarketingPageTabHelper&) = delete;
  GlicMarketingPageTabHelper& operator=(const GlicMarketingPageTabHelper&) =
      delete;
  ~GlicMarketingPageTabHelper() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  GlicMarketingPageTabHelper(content::WebContents* web_contents,
                             std::vector<GURL> allowed_urls,
                             int max_impressions);

  const std::vector<GURL> allowed_urls_;
  const int max_impressions_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_NAVIGATION_CAPTURE_GLIC_MARKETING_PAGE_TAB_HELPER_H_
