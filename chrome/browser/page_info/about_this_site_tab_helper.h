// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_INFO_ABOUT_THIS_SITE_TAB_HELPER_H_
#define CHROME_BROWSER_PAGE_INFO_ABOUT_THIS_SITE_TAB_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/page_info/core/about_this_site_service.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace optimization_guide {
class OptimizationGuideDecider;
class OptimizationMetadata;
}  // namespace optimization_guide

// This WebContentsObserver fetches AboutThisSite hints from OptimizationGuide
// and registers a SidePanel entry.
class AboutThisSiteTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<AboutThisSiteTabHelper>,
      public page_info::AboutThisSiteService::TabHelper {
 public:
  ~AboutThisSiteTabHelper() override;
  AboutThisSiteTabHelper(const AboutThisSiteTabHelper&) = delete;
  AboutThisSiteTabHelper& operator=(const AboutThisSiteTabHelper&) = delete;

  // content::WebContentsObserver implementation
  void PrimaryPageChanged(content::Page& page) override;

  // Get the AboutThisSiteMetadata for this page if available.
  page_info::AboutThisSiteService::DecisionAndMetadata
  GetAboutThisSiteMetadata() const override;

 private:
  explicit AboutThisSiteTabHelper(
      content::WebContents* web_contents,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider);
  friend class content::WebContentsUserData<AboutThisSiteTabHelper>;

  void OnOptimizationGuideDecision(
      const GURL& main_frame_url,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  optimization_guide::OptimizationGuideDecision decision_;
  std::optional<page_info::proto::AboutThisSiteMetadata>
      about_this_site_metadata_;

  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_ = nullptr;

  base::WeakPtrFactory<AboutThisSiteTabHelper> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PAGE_INFO_ABOUT_THIS_SITE_TAB_HELPER_H_
