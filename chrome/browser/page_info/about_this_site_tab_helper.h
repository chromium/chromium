// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_INFO_ABOUT_THIS_SITE_TAB_HELPER_H_
#define CHROME_BROWSER_PAGE_INFO_ABOUT_THIS_SITE_TAB_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace optimization_guide {
class OptimizationGuideDecider;
class OptimizationMetadata;
}  // namespace optimization_guide

namespace page_info {
namespace proto {
class BannerInfo;
}  // namespace proto
class AboutThisSiteService;
}  // namespace page_info

// This WebContentsObserver fetches AboutThisSite hints from OptimizationGuide
// and displays a banner if there is a BannerInfo message.
class AboutThisSiteTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<AboutThisSiteTabHelper> {
 public:
  ~AboutThisSiteTabHelper() override;
  AboutThisSiteTabHelper(const AboutThisSiteTabHelper&) = delete;
  AboutThisSiteTabHelper& operator=(const AboutThisSiteTabHelper&) = delete;

  // content::WebContentsObserver implementation
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // Keep in sync with AboutThisSiteBannerStatus in enums.xml
  enum class BannerStatus {
    kNoHints = 0,
    kShown = 1,
    kInvalidOrMissingBannerInfo = 2,
    kNavigatedAway = 3,
    kNotAllowedToShow = 4,

    kMaxValue = kNotAllowedToShow
  };

  explicit AboutThisSiteTabHelper(
      content::WebContents* web_contents,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
      page_info::AboutThisSiteService* about_this_site_service);
  friend class content::WebContentsUserData<AboutThisSiteTabHelper>;

  void ShowBanner(page_info::proto::BannerInfo banner_info);

  void OnOptimizationGuideDecision(
      const GURL& main_frame_url,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  BannerStatus HandleOptimizationGuideDecision(
      const GURL& main_frame_url,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_ = nullptr;
  raw_ptr<page_info::AboutThisSiteService> about_this_site_service_ = nullptr;

  base::WeakPtrFactory<AboutThisSiteTabHelper> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PAGE_INFO_ABOUT_THIS_SITE_TAB_HELPER_H_
