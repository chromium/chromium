// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_PROMOTION_SOURCE_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_GLIC_GLIC_PROMOTION_SOURCE_NAVIGATION_OBSERVER_H_

#include "base/callback_list.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

class Profile;

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

// Synthetic trial name for tracking referral acquisition source.
inline constexpr char kGlicPromotionSourceTrialName[] =
    "GlicPromotionSourceSynthetic";

// Group names for the GlicPromotionSourceSynthetic trial.
inline constexpr char kGlicPromotionSourceChromeDotCom[] = "ChromeDotCom";
inline constexpr char kGlicPromotionSourceWebstore[] = "Webstore";
inline constexpr char kGlicPromotionSourceZss[] = "zss";
inline constexpr char kGlicPromotionSourceMultiple[] = "Multiple";
inline constexpr char kGlicPromotionSourceMultiProfileDetected[] =
    "MultiProfileDetected";

// A tab feature that observes navigations to the Glic promotion marketing page
// and registers synthetic field trials for referral acquisition sources
// (Webstore, ChromeDotCom, zss, or Multiple).
class GlicPromotionSourceNavigationObserver
    : public content::WebContentsObserver {
 public:
  explicit GlicPromotionSourceNavigationObserver(tabs::TabInterface* tab);
  GlicPromotionSourceNavigationObserver(
      const GlicPromotionSourceNavigationObserver&) = delete;
  GlicPromotionSourceNavigationObserver& operator=(
      const GlicPromotionSourceNavigationObserver&) = delete;
  ~GlicPromotionSourceNavigationObserver() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  static void SetPromotionPageUrlForTesting(const GURL& url);

 private:
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);

  void MaybeRegisterPromotionSourceCohort(
      Profile* profile,
      content::NavigationHandle* navigation_handle);

  base::CallbackListSubscription tab_subscription_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_PROMOTION_SOURCE_NAVIGATION_OBSERVER_H_
