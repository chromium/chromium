// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/tracking_protection_notice_service.h"

#include <memory>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_notice_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/eligibility_service_factory.h"
#include "chrome/browser/tpcd/experiment/experiment_manager_impl.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_result.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_features.h"

namespace privacy_sandbox {

namespace {

using NoticeType = ::privacy_sandbox::TrackingProtectionOnboarding::NoticeType;
using NoticeAction =
    ::privacy_sandbox::TrackingProtectionOnboarding::NoticeAction;
using SurfaceType =
    ::privacy_sandbox::TrackingProtectionOnboarding::SurfaceType;

void CreateHistogramNoticeServiceEvent(
    TrackingProtectionNoticeService::TrackingProtectionNoticeServiceEvent
        event) {
  base::UmaHistogramEnumeration(
      "PrivacySandbox.TrackingProtection.NoticeServiceEvent", event);
}

NoticeAction ToNoticeAction(
    user_education::FeaturePromoClosedReason close_reason) {
  switch (close_reason) {
    case user_education::FeaturePromoClosedReason::kDismiss:
      return NoticeAction::kGotIt;
    case user_education::FeaturePromoClosedReason::kAction:
      return NoticeAction::kSettings;
    case user_education::FeaturePromoClosedReason::kCancel:
      return NoticeAction::kClosed;
    default:
      return NoticeAction::kOther;
  }
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

void TrackingProtectionNoticeService::Shutdown() {
  profile_ = nullptr;
  onboarding_service_ = nullptr;
  tracking_protection_notice_.reset();
  onboarding_observation_.Reset();
}

void TrackingProtectionNoticeService::InitializeTabStripTracker() {
  if (tab_strip_tracker_) {
    return;
  }
  tab_strip_tracker_ = std::make_unique<BrowserTabStripTracker>(this, this);
  tab_strip_tracker_->Init();
  base::UmaHistogramBoolean(
      "PrivacySandbox.TrackingProtection.NoticeService."
      "IsObservingTabStripModel",
      true);
}

void TrackingProtectionNoticeService::ResetTabStripTracker() {
  if (!tab_strip_tracker_) {
    return;
  }
  tab_strip_tracker_ = nullptr;
  base::UmaHistogramBoolean(
      "PrivacySandbox.TrackingProtection.NoticeService."
      "IsObservingTabStripModel",
      false);
}

bool TrackingProtectionNoticeService::SilentNotice::WasPromoPreviouslyDismissed(
    Browser* browser) {
  return false;
}

bool TrackingProtectionNoticeService::SilentNotice::MaybeShowPromo(
    Browser* browser) {
  return browser->window()->CanShowFeaturePromo(notice_->GetIPHFeature());
}

bool TrackingProtectionNoticeService::SilentNotice::IsPromoShowing(
    Browser* browser) {
  return false;
}

bool TrackingProtectionNoticeService::SilentNotice::HidePromo(
    Browser* browser) {
  return false;
}

bool TrackingProtectionNoticeService::VisibleNotice::
    WasPromoPreviouslyDismissed(Browser* browser) {
  auto promo_result =
      browser->window()->CanShowFeaturePromo(notice_->GetIPHFeature());
  return promo_result.failure().has_value() &&
         promo_result.failure().value() ==
             user_education::FeaturePromoResult::kPermanentlyDismissed;
}

bool TrackingProtectionNoticeService::VisibleNotice::MaybeShowPromo(
    Browser* browser) {
  base::Time shown_when = base::Time::Now();
  user_education::FeaturePromoParams params(notice_->GetIPHFeature());
  params.close_callback = base::BindOnce(
      &TrackingProtectionNoticeService::BaseIPHNotice::OnNoticeClosed,
      base::Unretained(notice_), shown_when,
      browser->window()->GetFeaturePromoController());
  return browser->window()->MaybeShowFeaturePromo(std::move(params));
}

bool TrackingProtectionNoticeService::VisibleNotice::IsPromoShowing(
    Browser* browser) {
  return browser->window()->IsFeaturePromoActive(notice_->GetIPHFeature());
}

bool TrackingProtectionNoticeService::VisibleNotice::HidePromo(
    Browser* browser) {
  return browser->window()->CloseFeaturePromo(
      notice_->GetIPHFeature(),
      user_education::EndFeaturePromoReason::kAbortPromo);
}

TrackingProtectionNoticeService::NoticeBehavior::NoticeBehavior(
    BaseIPHNotice* notice)
    : notice_(notice) {}

TrackingProtectionNoticeService::NoticeBehavior::~NoticeBehavior() = default;

TrackingProtectionNoticeService::BaseIPHNotice::BaseIPHNotice(
    Profile* profile,
    TrackingProtectionOnboarding* onboarding_service,
    TrackingProtectionNoticeService* notice_service)
    : profile_(profile),
      onboarding_service_(onboarding_service),
      notice_service_(notice_service) {}

TrackingProtectionNoticeService::BaseIPHNotice::~BaseIPHNotice() = default;

const base::Feature&
TrackingProtectionNoticeService::BaseIPHNotice::GetIPHFeature() {
  // TODO(crbug.com/341975190) Add other features for when 3PCD Full Launch is
  // supported.
  return feature_engagement::kIPHTrackingProtectionOnboardingFeature;
}

void TrackingProtectionNoticeService::BaseIPHNotice::
    MaybeUpdateNoticeVisibility(content::WebContents* web_content) {
  CreateHistogramNoticeServiceEvent(
      TrackingProtectionNoticeServiceEvent::kUpdateNoticeVisibility);
  if (!web_content) {
    return;
  }

  auto* browser = chrome::FindBrowserWithTab(web_content);

  if (!browser || !browser->window() || !browser->location_bar_model() ||
      !browser->tab_strip_model()) {
    return;
  }
  // Exclude Popups, PWAs and other non normal browsers.
  if (browser->type() != Browser::TYPE_NORMAL) {
    CreateHistogramNoticeServiceEvent(
        TrackingProtectionNoticeServiceEvent::kBrowserTypeNonNormal);
    return;
  }

  // If the notice should no longer be shown, then hide it and add metrics.
  if (notice_behavior() &&
      GetNoticeType() !=
          onboarding_service_->GetRequiredNotice(SurfaceType::kDesktop)) {
    if (notice_behavior()->IsPromoShowing(browser)) {
      CreateHistogramNoticeServiceEvent(
          TrackingProtectionNoticeServiceEvent::kNoticeShowingButShouldnt);
      notice_behavior()->HidePromo(browser);
    }
    return;
  }
  // If tab triggering the update isn't the active one, avoid triggering the
  // promo.
  // No additional checks on the window Active/Minimized, as the Promos can only
  // be shown on active windows.
  if (browser->tab_strip_model()->GetActiveWebContents() != web_content) {
    CreateHistogramNoticeServiceEvent(
        TrackingProtectionNoticeServiceEvent::kInactiveWebcontentUpdated);
    return;
  }

  // We should hide the notice at this point if the browser isn't eligible.
  // This would only be relevant if we've already had a chance to create a
  // notice_behavior, since a notice_behavior had to have been present for us to
  // be able to show a Promo.
  if (!IsLocationBarEligible(browser)) {
    if (notice_behavior()) {
      notice_behavior()->HidePromo(browser);
    }
    return;
  }

  // At this point, the update is happening in an active tab, Secure location,
  // with a visible LocationIcon. We should attempt to show the notice if it's
  // not already shown.
  if (notice_behavior() && notice_behavior()->IsPromoShowing(browser)) {
    CreateHistogramNoticeServiceEvent(
        TrackingProtectionNoticeServiceEvent::kNoticeAlreadyShowing);
    return;
  }

  // Safe to init the notice behavior at this point, since we know we're about
  // to show the notice.
  MaybeInitNoticeBehavior();

  // Check if the promo has previously been dismissed by the user. If so, Notify
  // the onboarding service that the promo was shown.
  if (notice_behavior() &&
      notice_behavior()->WasPromoPreviouslyDismissed(browser)) {
    onboarding_service_->NoticeShown(SurfaceType::kDesktop, GetNoticeType());
    CreateHistogramNoticeServiceEvent(
        TrackingProtectionNoticeServiceEvent::kPromoPreviouslyDismissed);
    return;
  }

  if (notice_behavior() && notice_behavior()->MaybeShowPromo(browser)) {
    onboarding_service_->NoticeShown(SurfaceType::kDesktop, GetNoticeType());
    CreateHistogramNoticeServiceEvent(
        TrackingProtectionNoticeServiceEvent::kNoticeRequestedAndShown);
  } else {
    CreateHistogramNoticeServiceEvent(
        TrackingProtectionNoticeServiceEvent::kNoticeRequestedButNotShown);
  }
}

void TrackingProtectionNoticeService::BaseIPHNotice::MaybeInitNoticeBehavior() {
  if (notice_behavior()) {
    return;
  }
  if (GetNoticeType() ==
      TrackingProtectionOnboarding::NoticeType::kModeBSilentOnboarding) {
    notice_behavior_ = std::make_unique<SilentNotice>(this);
    return;
  } else if (GetNoticeType() ==
             TrackingProtectionOnboarding::NoticeType::kModeBOnboarding) {
    notice_behavior_ = std::make_unique<VisibleNotice>(this);
    return;
  }
}

NoticeType TrackingProtectionNoticeService::BaseIPHNotice::GetNoticeType() {
  if (!notice_type_.has_value()) {
    notice_type_ =
        onboarding_service_->GetRequiredNotice(SurfaceType::kDesktop);
  }
  return *notice_type_;
}

void TrackingProtectionNoticeService::BaseIPHNotice::OnNoticeClosed(
    base::Time showed_when,
    user_education::FeaturePromoController* promo_controller) {
  if (!promo_controller) {
    return;
  }

  user_education::FeaturePromoClosedReason close_reason;
  bool has_been_dismissed =
      promo_controller->HasPromoBeenDismissed(GetIPHFeature(), &close_reason);
  if (!has_been_dismissed) {
    return;
  }
  onboarding_service_->NoticeActionTaken(SurfaceType::kDesktop, GetNoticeType(),
                                         ToNoticeAction(close_reason));
}

void TrackingProtectionNoticeService::OnShouldShowNoticeUpdated() {
  // We only start watching updates on TabStripTracker when we actually need
  // to show a notice. If we no longer need to show the notice, we stop watching
  // so we don't run logic unnecessarily.
  if (!onboarding_service_->ShouldRunUILogic(SurfaceType::kDesktop)) {
    ResetTabStripTracker();
    return;
  }
  tracking_protection_notice_ =
      std::make_unique<BaseIPHNotice>(profile_, onboarding_service_, this);
  InitializeTabStripTracker();
  return;
}

bool TrackingProtectionNoticeService::IsNoticeNeeded() {
  return onboarding_service_->ShouldRunUILogic(SurfaceType::kDesktop);
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
  if (!tracking_protection_notice_) {
    return;
  }
  tracking_protection_notice_->MaybeUpdateNoticeVisibility(
      selection.new_contents);
  CreateHistogramNoticeServiceEvent(
      TrackingProtectionNoticeService::TrackingProtectionNoticeServiceEvent::
          kActiveTabChanged);
  return;
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
  if (!notice_service) {
    return false;
  }
  return notice_service->IsNoticeNeeded();
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

  auto* notice_service =
      TrackingProtectionNoticeFactory::GetForProfile(profile);
  if (!notice_service->tracking_protection_notice_) {
    return;
  }
  notice_service->tracking_protection_notice_->MaybeUpdateNoticeVisibility(
      web_contents());
  CreateHistogramNoticeServiceEvent(
      TrackingProtectionNoticeService::TrackingProtectionNoticeServiceEvent::
          kNavigationFinished);
}

bool TrackingProtectionNoticeService::BaseIPHNotice::IsLocationBarEligible(
    Browser* browser) {
  bool is_secure = browser->location_bar_model()->GetSecurityLevel() ==
                   security_state::SECURE;
  if (!is_secure) {
    CreateHistogramNoticeServiceEvent(
        TrackingProtectionNoticeService::TrackingProtectionNoticeServiceEvent::
            kLocationIconNonSecure);
  }

  bool is_element_visible =
      ui::ElementTracker::GetElementTracker()->IsElementVisible(
          kLocationIconElementId, browser->window()->GetElementContext());
  if (!is_element_visible) {
    CreateHistogramNoticeServiceEvent(
        TrackingProtectionNoticeService::TrackingProtectionNoticeServiceEvent::
            kLocationIconNonVisible);
  }
  return is_secure && is_element_visible;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TrackingProtectionNoticeService::TabHelper);

}  // namespace privacy_sandbox
