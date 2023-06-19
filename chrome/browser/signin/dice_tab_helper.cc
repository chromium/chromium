// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_tab_helper.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "google_apis/gaia/gaia_auth_util.h"

DiceTabHelper::ResetableState::ResetableState() = default;
DiceTabHelper::ResetableState::ResetableState(const ResetableState& other) =
    default;
DiceTabHelper::ResetableState& DiceTabHelper::ResetableState::operator=(
    const ResetableState& other) = default;

DiceTabHelper::DiceTabHelper(content::WebContents* web_contents)
    : content::WebContentsUserData<DiceTabHelper>(*web_contents),
      content::WebContentsObserver(web_contents) {}

DiceTabHelper::~DiceTabHelper() = default;

void DiceTabHelper::InitializeSigninFlow(
    const GURL& signin_url,
    signin_metrics::AccessPoint access_point,
    signin_metrics::Reason reason,
    signin_metrics::PromoAction promo_action,
    const GURL& redirect_url) {
  DCHECK(signin_url.is_valid());
  DCHECK(state_.signin_url.is_empty() || state_.signin_url == signin_url);

  Reset();
  state_.redirect_url = redirect_url;
  state_.signin_url = signin_url;
  state_.signin_access_point = access_point;
  state_.signin_promo_action = promo_action;
  state_.signin_reason = reason;

  is_chrome_signin_page_ = true;
  signin_page_load_recorded_ = false;

  // Note: if a Dice signin tab is reused, `InitializeSigninFlow()` is not
  // called again, and the tab reuse does not generate new metrics.

  if (reason == signin_metrics::Reason::kSigninPrimaryAccount ||
      reason == signin_metrics::Reason::kAddSecondaryAccount) {
    // See details at go/chrome-signin-metrics-revamp.
    base::UmaHistogramEnumeration(
        "Signin.SignIn.Started", access_point,
        signin_metrics::AccessPoint::ACCESS_POINT_MAX);
  }

  if (reason == signin_metrics::Reason::kSigninPrimaryAccount) {
    state_.sync_signin_flow_status = SyncSigninFlowStatus::kStarted;
    signin_metrics::LogSigninAccessPointStarted(access_point, promo_action);
    signin_metrics::RecordSigninUserActionForAccessPoint(access_point);
    base::RecordAction(base::UserMetricsAction("Signin_SigninPage_Loading"));
  }
}

bool DiceTabHelper::IsChromeSigninPage() const {
  return is_chrome_signin_page_;
}

bool DiceTabHelper::IsSyncSigninInProgress() const {
  return state_.sync_signin_flow_status == SyncSigninFlowStatus::kStarted;
}

void DiceTabHelper::OnSyncSigninFlowComplete() {
  // The flow is complete, reset to initial state.
  Reset();
}

void DiceTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!is_chrome_signin_page_)
    return;

  // Ignore internal navigations.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!IsSigninPageNavigation(navigation_handle)) {
    // Navigating away from the signin page.
    // Note that currently any indication of a navigation is enough to consider
    // this tab unsuitable for re-use, even if the navigation does not end up
    // committing.
    is_chrome_signin_page_ = false;
  }
}

void DiceTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!is_chrome_signin_page_)
    return;

  // Ignore internal navigations.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!IsSigninPageNavigation(navigation_handle)) {
    // Navigating away from the signin page.
    // Note that currently any indication of a navigation is enough to consider
    // this tab unsuitable for re-use, even if the navigation does not end up
    // committing.
    is_chrome_signin_page_ = false;
    return;
  }

  if (state_.signin_reason == signin_metrics::Reason::kSigninPrimaryAccount &&
      !signin_page_load_recorded_) {
    signin_page_load_recorded_ = true;
    base::RecordAction(base::UserMetricsAction("Signin_SigninPage_Shown"));
  }
}

bool DiceTabHelper::IsSigninPageNavigation(
    content::NavigationHandle* navigation_handle) const {
  return !navigation_handle->IsErrorPage() &&
         navigation_handle->GetRedirectChain()[0] == state_.signin_url &&
         gaia::HasGaiaSchemeHostPort(navigation_handle->GetURL());
}

void DiceTabHelper::Reset() {
  state_ = {};
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DiceTabHelper);
