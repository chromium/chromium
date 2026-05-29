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
#include "chrome/common/url_constants.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace finds {

namespace {

// Evaluates whether the URL is the native Android New Tab Page.
// We perform this check instead of calling search::IsNTPURL() because
// importing //chrome/browser/search introduces a dependency cycle.
bool IsAndroidNTP(const GURL& url) {
  return url.SchemeIs(chrome::kChromeNativeScheme) &&
         url.host() == chrome::kChromeUINewTabHost;
}

bool IsValidNavigation(content::NavigationHandle* navigation_handle) {
  return navigation_handle->HasCommitted() &&
         navigation_handle->IsInPrimaryMainFrame() &&
         !navigation_handle->IsSameDocument() &&
         navigation_handle->GetURL().is_valid() &&
         (navigation_handle->GetURL().SchemeIsHTTPOrHTTPS() ||
          IsAndroidNTP(navigation_handle->GetURL()));
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

  omnibox_tracker_observation_ =
      OmniboxEventGlobalTracker::GetInstance()->RegisterCallback(
          base::BindRepeating(&FindsTabHelper::OnURLOpenedFromOmnibox,
                              base::Unretained(this)));
}

FindsTabHelper::~FindsTabHelper() = default;

void FindsTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Reset the Finds opt-in pending states on any new committed primary
  // main-frame navigation to prevent previous state leaking to new navigations.
  if (navigation_handle->HasCommitted() &&
      navigation_handle->IsInPrimaryMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    is_srp_return_opt_in_pending_ = false;
    is_recent_search_suggestion_opt_in_pending_ = false;
  }

  if (!IsValidNavigation(navigation_handle)) {
    return;
  }

  // Store in a temporary variable and immediately reset the pending state so
  // it isn't leaked if checks below do not pass.
  bool was_recent_search_suggestion_navigation_pending =
      pending_omnibox_recent_search_suggestion_navigation_;
  pending_omnibox_recent_search_suggestion_navigation_ = false;

  if (!IsSupportedPlatform()) {
    return;
  }

  if (!finds_service_->IsFindsFeatureAllowedForUser()) {
    return;
  }

  // Ensure navigation was indeed a recent search suggestion to SRP.
  if (was_recent_search_suggestion_navigation_pending &&
      features::kEnableOmniboxRecentSearchSuggestionOptIn.Get() &&
      template_url_service_ &&
      template_url_service_->IsSearchResultsPageFromDefaultSearchProvider(
          navigation_handle->GetURL())) {
    if (finds_service_
            ->RecordRecentSearchSuggestionClickAndCheckThresholdReached()) {
      is_recent_search_suggestion_opt_in_pending_ = true;
    }
  }

  // Early exit if the opt in promo has already been interacted with enough
  // times determined by max count, or if the cooldown has not passed yet, or if
  // the user has already interacted with the promo.
  if (IsFindsOptInPromoAlreadyInteracted(pref_service_) ||
      IsFindsOptInPromoMaxCountExceeded(pref_service_) ||
      !IsFindsOptInPromoCooldownPassed(pref_service_)) {
    return;
  }

  if (IsAndroidNTP(navigation_handle->GetURL())) {
    finds_service_->RecordNTPVisited();
    return;
  }

  if (features::kEnableSrpReturnCountOptIn.Get()) {
    CheckSRPReturnCount(navigation_handle);
  }

  if (!opt_guide_service_ || !features::kEnableThemeUrlVisitCountOptIn.Get()) {
    return;
  }

  opt_guide_service_->CanApplyOptimization(
      navigation_handle->GetURL(), optimization_guide::proto::FINDS_PAGE_THEME,
      base::BindOnce(&FindsTabHelper::OnOptimizationGuideDecision,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FindsTabHelper::DidFirstVisuallyNonEmptyPaint() {
  if (is_srp_return_opt_in_pending_) {
    is_srp_return_opt_in_pending_ = false;
    if (finds_service_) {
      finds_service_->SRPBackNavigationCountForOptInReached();
    }
  }

  if (is_recent_search_suggestion_opt_in_pending_) {
    is_recent_search_suggestion_opt_in_pending_ = false;
    if (finds_service_) {
      finds_service_->RecentSearchSuggestionCountForOptInReached();
    }
  }
}

void FindsTabHelper::CheckSRPReturnCount(
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
      is_srp_return_opt_in_pending_ = true;
    }
  }
}

void FindsTabHelper::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  if (!finds_service_ || !finds_service_->IsFindsFeatureAllowedForUser()) {
    return;
  }
  if (!features::kEnableOmniboxRecentSearchSuggestionOptIn.Get()) {
    return;
  }
  // Verify that the omnibox event occurred in this tab.
  if (log->tab_id != sessions::SessionTabHelper::IdForTab(web_contents())) {
    return;
  }
  if (log->selection.line >= log->result->size()) {
    return;
  }
  const AutocompleteMatch& match = log->result->match_at(log->selection.line);
  // Identify recent search suggestions styled with the history clock icon.
  if (match.type == AutocompleteMatchType::SEARCH_HISTORY ||
      match.type == AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED) {
    // Signal that a recent search suggestion navigation is pending.
    pending_omnibox_recent_search_suggestion_navigation_ = true;
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
