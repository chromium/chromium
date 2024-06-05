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

void CreateHistogramNoticeServiceEvent(
    TrackingProtectionOnboarding::NoticeType type,
    TrackingProtectionNoticeService::TrackingProtectionMetricsNoticeEvent
        event) {
  switch (type) {
    case TrackingProtectionOnboarding::NoticeType::kNone:
      break;
    case TrackingProtectionOnboarding::NoticeType::kOnboarding:
      base::UmaHistogramEnumeration(
          "PrivacySandbox.TrackingProtection.Onboarding.NoticeServiceEvent",
          event);
      break;
    case TrackingProtectionOnboarding::NoticeType::kOffboarding:
      break;
    case TrackingProtectionOnboarding::NoticeType::kSilentOnboarding:
      base::UmaHistogramEnumeration(
          "PrivacySandbox.TrackingProtection.SilentOnboarding."
          "NoticeServiceEvent",
          event);
      break;
  }
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
  onboarding_notice_.reset();
  silent_onboarding_notice_.reset();
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

TrackingProtectionNoticeService::BaseIPHNotice::BaseIPHNotice(
    Profile* profile,
    TrackingProtectionOnboarding* onboarding_service,
    TrackingProtectionNoticeService* notice_service)
    : profile_(profile),
      onboarding_service_(onboarding_service),
      notice_service_(notice_service) {}

TrackingProtectionNoticeService::BaseIPHNotice::~BaseIPHNotice() = default;

void TrackingProtectionNoticeService::BaseIPHNotice::
    MaybeUpdateNoticeVisibility(content::WebContents* web_content) {
  CreateHistogramNoticeServiceEvent(
      GetNoticeType(),
      TrackingProtectionMetricsNoticeEvent::kUpdateNoticeVisibility);
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
        GetNoticeType(),
        TrackingProtectionMetricsNoticeEvent::kBrowserTypeNonNormal);

    return;
  }

  // If the notice should no longer be shown, then hide it and add metrics.
  if (onboarding_service_->GetRequiredNotice() != GetNoticeType()) {
    if (IsPromoShowing(browser)) {
      CreateHistogramNoticeServiceEvent(
          GetNoticeType(),
          TrackingProtectionMetricsNoticeEvent::kNoticeShowingButShouldnt);
      HidePromo(browser);
    }
    return;
  }

  // If tab triggering the update isn't the active one, avoid triggering the
  // promo.
  // No additional checks on the window Active/Minimized, as the Promos can only
  // be shown on active windows.
  if (browser->tab_strip_model()->GetActiveWebContents() != web_content) {
    CreateHistogramNoticeServiceEvent(
        GetNoticeType(),
        TrackingProtectionMetricsNoticeEvent::kInactiveWebcontentUpdated);
    return;
  }

  // Check if the promo has previously been dismissed by the user. If so, Notify
  // the onboarding service that the promo was shown.
  if (WasPromoPreviouslyDismissed(browser)) {
    onboarding_service_->NoticeShown(GetNoticeType());
    CreateHistogramNoticeServiceEvent(
        GetNoticeType(),
        TrackingProtectionMetricsNoticeEvent::kPromoPreviouslyDismissed);
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
    CreateHistogramNoticeServiceEvent(
        GetNoticeType(),
        TrackingProtectionMetricsNoticeEvent::kNoticeAlreadyShowing);
    return;
  }

  if (MaybeShowPromo(browser)) {
    onboarding_service_->NoticeShown(GetNoticeType());
    CreateHistogramNoticeServiceEvent(
        GetNoticeType(),
        TrackingProtectionMetricsNoticeEvent::kNoticeRequestedAndShown);
  } else {
    CreateHistogramNoticeServiceEvent(
        GetNoticeType(),
        TrackingProtectionMetricsNoticeEvent::kNoticeRequestedButNotShown);
  }
}

bool TrackingProtectionNoticeService::BaseIPHNotice::
    WasPromoPreviouslyDismissed(Browser* browser) {
  auto promo_result = browser->window()->CanShowFeaturePromo(GetIPHFeature());
  return promo_result.failure().has_value() &&
         promo_result.failure().value() ==
             user_education::FeaturePromoResult::kPermanentlyDismissed;
}

bool TrackingProtectionNoticeService::BaseIPHNotice::MaybeShowPromo(
    Browser* browser) {
  base::Time shown_when = base::Time::Now();
  user_education::FeaturePromoParams params(GetIPHFeature());
  params.close_callback = base::BindOnce(
      &TrackingProtectionNoticeService::BaseIPHNotice::OnNoticeClosed,
      base::Unretained(this), shown_when,
      browser->window()->GetFeaturePromoController());
  return browser->window()->MaybeShowFeaturePromo(std::move(params));
}

void TrackingProtectionNoticeService::BaseIPHNotice::HidePromo(
    Browser* browser) {
  browser->window()->CloseFeaturePromo(
      GetIPHFeature(), user_education::EndFeaturePromoReason::kAbortPromo);
}

bool TrackingProtectionNoticeService::BaseIPHNotice::IsPromoShowing(
    Browser* browser) {
  return browser->window()->IsFeaturePromoActive(GetIPHFeature());
}

TrackingProtectionNoticeService::OnboardingNotice::OnboardingNotice(
    Profile* profile,
    TrackingProtectionOnboarding* onboarding_service,
    TrackingProtectionNoticeService* notice_service)
    : BaseIPHNotice(profile, onboarding_service, notice_service) {
  CreateHistogramNoticeServiceEvent(
      GetNoticeType(),
      TrackingProtectionMetricsNoticeEvent::kNoticeObjectCreated);
}

NoticeType TrackingProtectionNoticeService::OnboardingNotice::GetNoticeType() {
  return NoticeType::kOnboarding;
}

const base::Feature&
TrackingProtectionNoticeService::OnboardingNotice::GetIPHFeature() {
  return feature_engagement::kIPHTrackingProtectionOnboardingFeature;
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
  onboarding_service_->NoticeActionTaken(GetNoticeType(),
                                         ToNoticeAction(close_reason));
}

TrackingProtectionNoticeService::SilentOnboardingNotice::SilentOnboardingNotice(
    Profile* profile,
    TrackingProtectionOnboarding* onboarding_service,
    TrackingProtectionNoticeService* notice_service)
    : BaseIPHNotice(profile, onboarding_service, notice_service) {
  CreateHistogramNoticeServiceEvent(
      GetNoticeType(),
      TrackingProtectionMetricsNoticeEvent::kNoticeObjectCreated);
}

NoticeType
TrackingProtectionNoticeService::SilentOnboardingNotice::GetNoticeType() {
  return NoticeType::kSilentOnboarding;
}

const base::Feature&
TrackingProtectionNoticeService::SilentOnboardingNotice::GetIPHFeature() {
  return feature_engagement::kIPHTrackingProtectionOnboardingFeature;
}

bool TrackingProtectionNoticeService::SilentOnboardingNotice::
    WasPromoPreviouslyDismissed(Browser* browser) {
  return false;
}

bool TrackingProtectionNoticeService::SilentOnboardingNotice::IsPromoShowing(
    Browser* browser) {
  return false;
}

bool TrackingProtectionNoticeService::SilentOnboardingNotice::MaybeShowPromo(
    Browser* browser) {
  // Check whether the onboarding promo can be shown but not actually showing
  // the promo.
  return browser->window()->CanShowFeaturePromo(GetIPHFeature());
}

void TrackingProtectionNoticeService::SilentOnboardingNotice::HidePromo(
    Browser* browser) {}

void TrackingProtectionNoticeService::SilentOnboardingNotice::OnNoticeClosed(
    base::Time showed_when,
    user_education::FeaturePromoController* promo_controller) {
  NOTREACHED_IN_MIGRATION();
}

void TrackingProtectionNoticeService::OnShouldShowNoticeUpdated() {
  // We only start watching updates on TabStripTracker when we actually need
  // to show a notice. If we no longer need to show the notice, we stop watching
  // so we don't run logic unnecessarily.
  switch (onboarding_service_->GetRequiredNotice()) {
    case NoticeType::kNone:
      ResetTabStripTracker();
      return;
    case NoticeType::kOnboarding:
      onboarding_notice_ = std::make_unique<OnboardingNotice>(
          profile_, onboarding_service_, this);
      InitializeTabStripTracker();
      return;
    case TrackingProtectionOnboarding::NoticeType::kOffboarding:
      return;
    case TrackingProtectionOnboarding::NoticeType::kSilentOnboarding:
      silent_onboarding_notice_ = std::make_unique<SilentOnboardingNotice>(
          profile_, onboarding_service_, this);
      InitializeTabStripTracker();
      return;
  }
}

bool TrackingProtectionNoticeService::IsNoticeNeeded() {
  return onboarding_service_->GetRequiredNotice() != NoticeType::kNone;
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
  if (onboarding_notice_) {
    onboarding_notice_->MaybeUpdateNoticeVisibility(selection.new_contents);
    CreateHistogramNoticeServiceEvent(
        TrackingProtectionOnboarding::NoticeType::kOnboarding,
        TrackingProtectionNoticeService::TrackingProtectionMetricsNoticeEvent::
            kActiveTabChanged);
  } else if (silent_onboarding_notice_) {
    silent_onboarding_notice_->MaybeUpdateNoticeVisibility(
        selection.new_contents);
    CreateHistogramNoticeServiceEvent(
        TrackingProtectionOnboarding::NoticeType::kSilentOnboarding,
        TrackingProtectionNoticeService::TrackingProtectionMetricsNoticeEvent::
            kActiveTabChanged);
  }
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
  if (notice_service->onboarding_notice_) {
    notice_service->onboarding_notice_->MaybeUpdateNoticeVisibility(
        web_contents());
    CreateHistogramNoticeServiceEvent(
        TrackingProtectionOnboarding::NoticeType::kOnboarding,
        TrackingProtectionNoticeService::TrackingProtectionMetricsNoticeEvent::
            kNavigationFinished);
  } else if (notice_service->silent_onboarding_notice_) {
    notice_service->silent_onboarding_notice_->MaybeUpdateNoticeVisibility(
        web_contents());
    CreateHistogramNoticeServiceEvent(
        TrackingProtectionOnboarding::NoticeType::kSilentOnboarding,
        TrackingProtectionNoticeService::TrackingProtectionMetricsNoticeEvent::
            kNavigationFinished);
  }
}

bool TrackingProtectionNoticeService::BaseIPHNotice::IsLocationBarEligible(
    Browser* browser) {
  bool is_secure = browser->location_bar_model()->GetSecurityLevel() ==
                   security_state::SECURE;
  if (!is_secure) {
    CreateHistogramNoticeServiceEvent(
        GetNoticeType(),
        TrackingProtectionNoticeService::TrackingProtectionMetricsNoticeEvent::
            kLocationIconNonSecure);
  }

  bool is_element_visible =
      ui::ElementTracker::GetElementTracker()->IsElementVisible(
          kLocationIconElementId, browser->window()->GetElementContext());
  if (!is_element_visible) {
    CreateHistogramNoticeServiceEvent(
        GetNoticeType(),
        TrackingProtectionNoticeService::TrackingProtectionMetricsNoticeEvent::
            kLocationIconNonVisible);
  }
  return is_secure && is_element_visible;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TrackingProtectionNoticeService::TabHelper);

}  // namespace privacy_sandbox
