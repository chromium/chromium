// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_info/about_this_site_tab_helper.h"

#include <optional>

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/page_info/page_info_features.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/page_info/core/about_this_site_validation.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/page_info/about_this_site_side_panel.h"
#endif

using page_info::about_this_site_validation::AboutThisSiteStatus;
using page_info::about_this_site_validation::ValidateMetadata;
using page_info::proto::AboutThisSiteMetadata;

AboutThisSiteTabHelper::AboutThisSiteTabHelper(
    content::WebContents* web_contents,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<AboutThisSiteTabHelper>(*web_contents),
      optimization_guide_decider_(optimization_guide_decider) {
  DCHECK(optimization_guide_decider_);
}

AboutThisSiteTabHelper::~AboutThisSiteTabHelper() = default;

page_info::AboutThisSiteService::DecisionAndMetadata
AboutThisSiteTabHelper::GetAboutThisSiteMetadata() const {
  return {decision_, about_this_site_metadata_};
}

void AboutThisSiteTabHelper::PrimaryPageChanged(content::Page& page) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  decision_ = optimization_guide::OptimizationGuideDecision::kUnknown;
  about_this_site_metadata_.reset();

  const GURL& url = page.GetMainDocument().GetLastCommittedURL();
  const bool should_consult_optimization_guide =
      url.SchemeIsHTTPOrHTTPS() && !page.GetMainDocument().IsErrorDocument();
  if (should_consult_optimization_guide) {
    optimization_guide_decider_->CanApplyOptimization(
        url, optimization_guide::proto::ABOUT_THIS_SITE,
        base::BindOnce(&AboutThisSiteTabHelper::OnOptimizationGuideDecision,
                       weak_ptr_factory_.GetWeakPtr(), url));
  }
}

void AboutThisSiteTabHelper::OnOptimizationGuideDecision(
    const GURL& main_frame_url,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  // Navigated away.
  if (web_contents()->GetLastCommittedURL() != main_frame_url)
    return;

  decision_ = decision;
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    return;
  }
  about_this_site_metadata_ = metadata.ParsedMetadata<AboutThisSiteMetadata>();

#if defined(TOOLKIT_VIEWS)
  if (page_info::IsPersistentSidePanelEntryFeatureEnabled()) {
    auto status = ValidateMetadata(about_this_site_metadata_);

    if (status != AboutThisSiteStatus::kValid) {
      return;
    }

    RegisterAboutThisSiteSidePanel(
        web_contents(),
        GURL(about_this_site_metadata_->site_info().more_about().url()));
  }
#endif
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AboutThisSiteTabHelper);
