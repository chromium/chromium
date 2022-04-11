// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_info/about_this_site_tab_helper.h"

#include "base/callback.h"
#include "base/metrics/histogram_functions.h"
#include "build/buildflag.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/page_info/core/about_this_site_service.h"
#include "components/page_info/core/about_this_site_validation.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/page_info/about_this_site_message_delegate_android.h"
#endif

using page_info::about_this_site_validation::AboutThisSiteStatus;
using page_info::about_this_site_validation::ValidateBannerInfo;

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

std::pair<AboutThisSiteStatus, absl::optional<page_info::proto::BannerInfo>>
GetBannerInfo(const optimization_guide::OptimizationMetadata& metadata) {
  auto parsed =
      metadata.ParsedMetadata<page_info::proto::AboutThisSiteMetadata>();

  if (!parsed) {
    return {AboutThisSiteStatus::kNoResult, absl::nullopt};
  }
  if (!parsed->has_banner_info()) {
    return {AboutThisSiteStatus::kMissingBannerInfo, absl::nullopt};
  }

  auto status = ValidateBannerInfo(parsed->banner_info());
  return {status, parsed->banner_info()};
}

bool IsExampleUrl(const GURL& url) {
  return url.DomainIs("example.com") && url.ref_piece() == "banner";
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
  BannerStatus status =
      HandleOptimizationGuideDecision(main_frame_url, decision, metadata);
  base::UmaHistogramEnumeration("Privacy.AboutThisSite.BannerStatus", status);
}

AboutThisSiteTabHelper::BannerStatus
AboutThisSiteTabHelper::HandleOptimizationGuideDecision(
    const GURL& main_frame_url,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  if (IsExampleUrl(main_frame_url)) {
    // Always provide a response for https://example.com/#banner.
    decision = optimization_guide::OptimizationGuideDecision::kTrue;
  }

  if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    return BannerStatus::kNoHints;
  }

  auto [status, banner_info] = GetBannerInfo(metadata);

  if (status != AboutThisSiteStatus::kValid && IsExampleUrl(main_frame_url)) {
    status = AboutThisSiteStatus::kValid;
    banner_info = page_info::proto::BannerInfo();
    banner_info->set_title("A Sample Note");
    banner_info->set_label("This is an example website");
    banner_info->mutable_url()->set_label("Example URL");
    banner_info->mutable_url()->set_url("https://example.com");
  }

  base::UmaHistogramEnumeration("Privacy.AboutThisSite.BannerValidation",
                                status);
  if (status != AboutThisSiteStatus::kValid) {
    return BannerStatus::kInvalidOrMissingBannerInfo;
  }

  if (!about_this_site_service_->CanShowBanner(main_frame_url)) {
    return BannerStatus::kNotAllowedToShow;
  }

  if (web_contents()->GetLastCommittedURL() != main_frame_url) {
    return BannerStatus::kNavigatedAway;
  }
  ShowBanner(std::move(*banner_info));
  return BannerStatus::kShown;
}

void AboutThisSiteTabHelper::ShowBanner(
    page_info::proto::BannerInfo banner_info) {
  ukm::SourceId source_id =
      ukm::GetSourceIdForWebContentsDocument(web_contents());
  GURL url = web_contents()->GetLastCommittedURL();
  base::OnceClosure on_dimiss =
      base::BindOnce(&page_info::AboutThisSiteService::OnBannerDismissed,
                     about_this_site_service_->GetWeakPtr(), url, source_id);
  base::OnceClosure on_url_opened =
      base::BindOnce(&page_info::AboutThisSiteService::OnBannerURLOpened,
                     about_this_site_service_->GetWeakPtr(), url, source_id);

#if BUILDFLAG(IS_ANDROID)
  AboutThisSiteMessageDelegateAndroid::Create(
      web_contents(), std::move(banner_info), std::move(on_dimiss),
      std::move(on_url_opened));
#endif
  // TODO(crbug.com/1307295): Implement desktop UI.
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AboutThisSiteTabHelper);
