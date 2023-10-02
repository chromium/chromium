// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/tracking_protection_notice_service.h"
#include <memory>
#include "base/check.h"
#include "components/feature_engagement/public/feature_constants.h"

#include "chrome/browser/privacy_sandbox/tracking_protection_notice_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "content/public/browser/navigation_handle.h"

namespace privacy_sandbox {

namespace {

bool IsLocationBarEligible(Browser* browser) {
  bool is_secure = browser->location_bar_model()->GetSecurityLevel() ==
                   security_state::SECURE;

  bool is_element_visible =
      ui::ElementTracker::GetElementTracker()->IsElementVisible(
          kLocationIconElementId, browser->window()->GetElementContext());

  return is_secure && is_element_visible;
}

bool IsPromoShowing(Browser* browser) {
  return browser->window()->IsFeaturePromoActive(
      feature_engagement::kIPHTrackingProtectionOnboardingFeature);
}

void HidePromo(Browser* browser) {
  browser->window()->CloseFeaturePromo(
      feature_engagement::kIPHTrackingProtectionOnboardingFeature,
      user_education::FeaturePromoCloseReason::kAbortPromo);
}

}  // namespace

TrackingProtectionNoticeService::TrackingProtectionNoticeService(
    Profile* profile,
    TrackingProtectionOnboarding* onboarding_service)
    : profile_(profile), onboarding_service_(onboarding_service) {
  CHECK(profile_);
  CHECK(onboarding_service_);
  onboarding_observation_.Observe(onboarding_service_);

  // We call this once here manually for initialization.
  OnShouldShowNoticeUpdated();
}

TrackingProtectionNoticeService::~TrackingProtectionNoticeService() = default;

void TrackingProtectionNoticeService::InitializeTabStripTracker() {
  tab_strip_tracker_ = std::make_unique<BrowserTabStripTracker>(this, this);
  tab_strip_tracker_->Init();
}

void TrackingProtectionNoticeService::ResetTabStripTracker() {
  tab_strip_tracker_ = nullptr;
}

void TrackingProtectionNoticeService::OnShouldShowNoticeUpdated() {
  if (onboarding_service_->ShouldShowOnboardingNotice()) {
    // We only start watching updates on TabStripTracker when we actually need
    // to show a notice.
    InitializeTabStripTracker();
  } else {
    // If we no longer need to show the notice, we stop watching so we don't run
    // logic unnecessarily.
    ResetTabStripTracker();
  }
}

void TrackingProtectionNoticeService::OnNoticeClosed(
    base::Time showed_when,
    user_education::FeaturePromoController* promo_controller) {
  if (!promo_controller) {
    return;
  }

  user_education::FeaturePromoStorageService::CloseReason close_reason;
  bool has_been_dismissed = promo_controller->HasPromoBeenDismissed(
      feature_engagement::kIPHTrackingProtectionOnboardingFeature,
      &close_reason);

  if (!has_been_dismissed) {
    return;
  }
  switch (close_reason) {
    case user_education::FeaturePromoStorageService::kDismiss:
      onboarding_service_->NoticeActionTaken(
          TrackingProtectionOnboarding::NoticeAction::kGotIt);
      return;
    case user_education::FeaturePromoStorageService::kAction:
      onboarding_service_->NoticeActionTaken(
          TrackingProtectionOnboarding::NoticeAction::kSettings);
      return;
    case user_education::FeaturePromoStorageService::kCancel:
      onboarding_service_->NoticeActionTaken(
          TrackingProtectionOnboarding::NoticeAction::kClosed);
      return;
    default:
      onboarding_service_->NoticeActionTaken(
          TrackingProtectionOnboarding::NoticeAction::kOther);
      return;
  }
}

void TrackingProtectionNoticeService::MaybeUpdateNoticeVisibility(
    content::WebContents* web_content) {
  if (!web_content) {
    return;
  }

  auto* browser = chrome::FindBrowserWithWebContents(web_content);

  if (!browser || !browser->window() || !browser->location_bar_model() ||
      !browser->tab_strip_model()) {
    return;
  }

  // Exclude Popups, PWAs and other non normal browsers.
  if (browser->type() != Browser::TYPE_NORMAL) {
    return;
  }

  // If the notice should no longer be shown, then hide it and add metrics.
  if (!onboarding_service_->ShouldShowOnboardingNotice()) {
    if (IsPromoShowing(browser)) {
      HidePromo(browser);
      // TODO(b/302008359) Add Metrics. We shouldn't be in this state.
    }
    return;
  }

  // If tab triggering the update isn't the active one, avoid triggering the
  // promo.
  // No additional checks on the window Active/Minimized, as the Promos can only
  // be shown on active windows.
  if (browser->tab_strip_model()->GetActiveWebContents() != web_content) {
    return;
  }

  // We should hide the notice at this point if the browser isn't eligible.
  if (!IsLocationBarEligible(browser)) {
    HidePromo(browser);
    return;
  }

  // At this point, the update is happening in an active tab, Secure location,
  // with a visible LocationIcon. We should attempt to show the notice if it's
  // not already shown.
  if (IsPromoShowing(browser)) {
    return;
  }

  base::Time shown_when = base::Time::Now();
  user_education::FeaturePromoParams params(
      feature_engagement::kIPHTrackingProtectionOnboardingFeature);
  params.close_callback = base::BindOnce(
      &TrackingProtectionNoticeService::OnNoticeClosed, base::Unretained(this),
      shown_when, browser->window()->GetFeaturePromoController());
  if (browser->window()->MaybeShowFeaturePromo(std::move(params))) {
    onboarding_service_->NoticeShown();
    // TODO(b/302008359) Emit metrics
  } else {
    // TODO(b/302008359) Emit metrics
  }
}

bool TrackingProtectionNoticeService::IsNoticeNeeded() {
  return onboarding_service_->ShouldShowOnboardingNotice();
}

bool TrackingProtectionNoticeService::ShouldTrackBrowser(Browser* browser) {
  return browser->profile() == profile_ &&
         browser->type() == Browser::TYPE_NORMAL;
}

void TrackingProtectionNoticeService::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed()) {
    return;
  }
  MaybeUpdateNoticeVisibility(selection.new_contents);
}

TrackingProtectionNoticeService::TabHelper::TabHelper(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      content::WebContentsUserData<TrackingProtectionNoticeService::TabHelper>(
          *web_contents) {}

TrackingProtectionNoticeService::TabHelper::~TabHelper() = default;

bool TrackingProtectionNoticeService::TabHelper::IsHelperNeeded(
    Profile* profile) {
  auto* notice_service =
      TrackingProtectionNoticeFactory::GetForProfile(profile);
  return notice_service && notice_service->IsNoticeNeeded();
}

void TrackingProtectionNoticeService::TabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle || !navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(GetWebContents().GetBrowserContext());

  TrackingProtectionNoticeFactory::GetForProfile(profile)
      ->MaybeUpdateNoticeVisibility(web_contents());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TrackingProtectionNoticeService::TabHelper);

}  // namespace privacy_sandbox
