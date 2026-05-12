// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/core/finds_tab_helper.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif

#include "chrome/browser/finds/core/finds_features.h"
#include "chrome/browser/finds/core/finds_pref_names.h"
#include "chrome/browser/finds/core/finds_service.h"
#include "chrome/browser/finds/core/finds_utils.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace finds {

namespace {

bool IsValidNavigation(content::NavigationHandle* navigation_handle) {
  return navigation_handle->HasCommitted() &&
         navigation_handle->IsInPrimaryMainFrame() &&
         !navigation_handle->IsSameDocument() &&
         navigation_handle->GetURL().is_valid() &&
         navigation_handle->GetURL().SchemeIsHTTPOrHTTPS();
}

bool IsFindsOptInPromoCooldownPassed(const PrefService* pref_service) {
  const int64_t last_timestamp_value =
      pref_service->GetInt64(prefs::kFindsOptInPromoLastShownTimestamp);
  if (last_timestamp_value == 0) {
    return true;
  }

  const base::Time last_interacted_time =
      base::Time::FromMillisecondsSinceUnixEpoch(last_timestamp_value);
  return (base::Time::Now() - last_interacted_time) >=
         base::Days(finds::features::kFindsOptInPromoCooldownInDays.Get());
}

bool IsFindsOptInPromoMaxCountExceeded(const PrefService* pref_service) {
  return pref_service->GetInteger(prefs::kFindsOptInPromoShownCount) >=
         finds::features::kFindsOptInPromoMaxInteractedCount.Get();
}

bool IsFindsOptInPromoAlreadyInteracted(const PrefService* pref_service) {
  return pref_service->GetBoolean(prefs::kFindsOptInPromoUserInteracted);
}

}  // namespace

// static
bool FindsTabHelper::IsSupportedPlatform() {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::device_info::is_desktop() ||
      base::android::device_info::is_tv() ||
      base::android::device_info::is_automotive() ||
      base::android::device_info::is_xr()) {
    return false;
  }
#endif
  return true;
}

FindsTabHelper::FindsTabHelper(content::WebContents* web_contents,
                               FindsService* finds_service,
                               OptimizationGuideKeyedService* opt_guide_service,
                               TemplateURLService* template_url_service,
                               PrefService* pref_service)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<FindsTabHelper>(*web_contents) {
  // FindsTabHelper should only be created when the FindsService is non-null.
  CHECK(finds_service);
  finds_service_ = finds_service;
  pref_service_ = pref_service;
  opt_guide_service_ = opt_guide_service;
  template_url_service_ = template_url_service;

  if (opt_guide_service_) {
    opt_guide_service_->RegisterOptimizationTypes(
        {optimization_guide::proto::FINDS_PAGE_THEME});
  }
}

FindsTabHelper::~FindsTabHelper() = default;

void FindsTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!IsValidNavigation(navigation_handle)) {
    return;
  }

  if (!IsSupportedPlatform()) {
    return;
  }

  if (!finds_service_->IsFindsFeatureAllowedForUser()) {
    return;
  }

  // Early exit if the opt in promo has already been interacted with enough
  // times determined by max count, or if the cooldown has not passed yet, or if
  // the user has already interacted with the promo.
  if (IsFindsOptInPromoAlreadyInteracted(pref_service_) ||
      IsFindsOptInPromoMaxCountExceeded(pref_service_) ||
      !IsFindsOptInPromoCooldownPassed(pref_service_)) {
    return;
  }

  if (features::kEnableSrpReturnCountOptIn.Get()) {
    CheckSRPReturnCountAndMaybeTriggerOptIn(navigation_handle);
  }

  if (!opt_guide_service_ || !features::kEnableThemeUrlVisitCountOptIn.Get()) {
    return;
  }

  opt_guide_service_->CanApplyOptimization(
      navigation_handle->GetURL(), optimization_guide::proto::FINDS_PAGE_THEME,
      base::BindOnce(&FindsTabHelper::OnOptimizationGuideDecision,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FindsTabHelper::CheckSRPReturnCountAndMaybeTriggerOptIn(
    content::NavigationHandle* navigation_handle) {
  if (!template_url_service_) {
    return;
  }

  bool is_current_page_srp =
      template_url_service_->IsSearchResultsPageFromDefaultSearchProvider(
          navigation_handle->GetURL());
  ui::PageTransition transition = navigation_handle->GetPageTransition();

  // Check if the user returned to an SRP via forward/back navigation.
  if (is_current_page_srp && (transition & ui::PAGE_TRANSITION_FORWARD_BACK)) {
    srp_return_count_++;

    if (srp_return_count_ >= finds::features::kSRPReturnCountThreshold.Get()) {
      if (finds_service_) {
        finds_service_->SRPBackNavigationCountForOptInReached();
      }
    }
  }
}

void FindsTabHelper::OnOptimizationGuideDecision(
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  if (decision == optimization_guide::OptimizationGuideDecision::kTrue) {
    auto finds_metadata =
        metadata.ParsedMetadata<optimization_guide::proto::FindsMetadata>();
    if (finds_metadata && finds_metadata->has_theme_type() && finds_service_) {
      finds_service_->RecordThemeURLVisited(finds_metadata->theme_type());
    }
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FindsTabHelper);

}  // namespace finds
