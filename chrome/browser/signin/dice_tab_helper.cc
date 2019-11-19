// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_tab_helper.h"

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "google_apis/gaia/gaia_urls.h"

DiceTabHelper::DiceTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

DiceTabHelper::~DiceTabHelper() = default;

void DiceTabHelper::InitializeSigninFlow(
    const GURL& signin_url,
    signin_metrics::AccessPoint access_point,
    signin_metrics::Reason reason,
    signin_metrics::PromoAction promo_action,
    const GURL& redirect_url) {
  DCHECK(signin_url.is_valid());
  DCHECK(signin_url_.is_empty() || signin_url_ == signin_url);

  // The signin page must be loading.
  DCHECK(web_contents()->GetController().GetPendingEntry());
  DCHECK_EQ(signin_url,
            web_contents()->GetController().GetPendingEntry()->GetURL());

  signin_url_ = signin_url;
  signin_access_point_ = access_point;
  signin_reason_ = reason;
  signin_promo_action_ = promo_action;
  is_chrome_signin_page_ = true;
  signin_page_load_recorded_ = false;
  redirect_url_ = redirect_url;

  if (reason == signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT) {
    signin_metrics::LogSigninAccessPointStarted(access_point, promo_action);
    signin_metrics::RecordSigninUserActionForAccessPoint(access_point,
                                                         promo_action);
    base::RecordAction(base::UserMetricsAction("Signin_SigninPage_Loading"));
  }
}

bool DiceTabHelper::IsChromeSigninPage() const {
  return is_chrome_signin_page_;
}

void DiceTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!is_chrome_signin_page_)
    return;

  // Ignore internal navigations.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!IsSigninPageNavigation(navigation_handle)) {
    // Navigating away from the signin page.
    is_chrome_signin_page_ = false;
  }
}

void DiceTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!is_chrome_signin_page_)
    return;

  // Ignore internal navigations.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!IsSigninPageNavigation(navigation_handle)) {
    // Navigating away from the signin page.
    is_chrome_signin_page_ = false;
    return;
  }

  if (!signin_page_load_recorded_) {
    signin_page_load_recorded_ = true;
    base::RecordAction(base::UserMetricsAction("Signin_SigninPage_Shown"));
  }
}

bool DiceTabHelper::IsSigninPageNavigation(
    content::NavigationHandle* navigation_handle) const {
  return !navigation_handle->IsErrorPage() &&
         navigation_handle->GetRedirectChain()[0] == signin_url_ &&
         navigation_handle->GetURL().GetOrigin() ==
             GaiaUrls::GetInstance()->gaia_url();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DiceTabHelper)
