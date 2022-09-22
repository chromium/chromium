// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_info/about_this_site_tab_helper.h"

#include "base/callback.h"
#include "base/metrics/histogram_functions.h"
#include "build/buildflag.h"
#include "chrome/browser/page_info/page_info_features.h"
#include "chrome/browser/ui/page_info/about_this_site_side_panel.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/page_info/core/about_this_site_service.h"
#include "components/page_info/core/about_this_site_validation.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using page_info::about_this_site_validation::AboutThisSiteStatus;
using page_info::about_this_site_validation::ValidateMetadata;
using page_info::proto::AboutThisSiteMetadata;

namespace {

bool ShouldConsultOptimizationGuide(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument())
    return false;
  if (navigation_handle->IsErrorPage())
    return false;
  if (!navigation_handle->IsInPrimaryMainFrame())
    return false;
  if (!navigation_handle->HasCommitted())
    return false;
  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS())
    return false;
  return true;
}
}  // namespace

AboutThisSiteTabHelper::AboutThisSiteTabHelper(
    content::WebContents* web_contents,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
    page_info::AboutThisSiteService* about_this_site_service)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<AboutThisSiteTabHelper>(*web_contents),
      optimization_guide_decider_(optimization_guide_decider),
      about_this_site_service_(about_this_site_service) {
  DCHECK(optimization_guide_decider_);
  DCHECK(about_this_site_service_);
}

AboutThisSiteTabHelper::~AboutThisSiteTabHelper() = default;

void AboutThisSiteTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!ShouldConsultOptimizationGuide(navigation_handle)) {
    return;
  }

  optimization_guide_decider_->CanApplyOptimizationAsync(
      navigation_handle, optimization_guide::proto::ABOUT_THIS_SITE,
      base::BindOnce(&AboutThisSiteTabHelper::OnOptimizationGuideDecision,
                     weak_ptr_factory_.GetWeakPtr(),
                     navigation_handle->GetURL()));
}

void AboutThisSiteTabHelper::OnOptimizationGuideDecision(
    const GURL& main_frame_url,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  // Navigated away.
  if (web_contents()->GetLastCommittedURL() != main_frame_url)
    return;

  if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    return;
  }

  absl::optional<AboutThisSiteMetadata> about_this_site_metadata =
      metadata.ParsedMetadata<AboutThisSiteMetadata>();

  auto status =
      ValidateMetadata(about_this_site_metadata,
                       page_info::IsDescriptionPlaceholderFeatureEnabled());

  base::UmaHistogramEnumeration("Privacy.AboutThisSite.PageLoadValidation",
                                status);
  if (status != AboutThisSiteStatus::kValid)
    return;

  RegisterAboutThisSiteSidePanel(
      web_contents(),
      GURL(about_this_site_metadata->site_info().more_about().url()));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AboutThisSiteTabHelper);
