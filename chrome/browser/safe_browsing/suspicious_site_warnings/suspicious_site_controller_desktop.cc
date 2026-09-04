// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/suspicious_site_warnings/suspicious_site_controller_desktop.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/types/pass_key.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/suspicious_site_warnings/suspicious_site_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/browser/suspicious_site_warning_allowlist.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

namespace safe_browsing {
namespace {

void LogUserInteraction(SuspiciousSiteWarningUserInteraction interaction) {
  base::UmaHistogramEnumeration(
      "SafeBrowsing.SuspiciousSiteWarning.UserInteraction", interaction);
}

base::OnceClosure* GetShownCallback() {
  static base::NoDestructor<base::OnceClosure> callback;
  return callback.get();
}

base::OnceClosure* GetDestroyedCallback() {
  static base::NoDestructor<base::OnceClosure> callback;
  return callback.get();
}

}  // namespace

void ShowSuspiciousSiteWarning(content::WebContents* web_contents,
                               int64_t navigation_id) {
  SuspiciousSiteControllerDesktop::ShowForWebContents(web_contents,
                                                      navigation_id);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SuspiciousSiteControllerDesktop);

SuspiciousSiteControllerDesktop::SuspiciousSiteControllerDesktop(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SuspiciousSiteControllerDesktop>(
          *web_contents) {}

SuspiciousSiteControllerDesktop::~SuspiciousSiteControllerDesktop() {
  if (has_shown_) {
    base::UmaHistogramEnumeration(
        "SafeBrowsing.SuspiciousSiteWarning.WarningOutcome", warning_outcome_);
  }

  // Remove the active suspicious site entry from AllowlistUrlSet when this
  // controller is destroyed (e.g. when navigating away or closing the tab) so
  // that the security state is restored.
  if (!current_suspicious_url_.is_empty() && web_contents()) {
    SafeBrowsingService* sb_service =
        g_browser_process->safe_browsing_service();
    if (sb_service && sb_service->ui_manager()) {
      sb_service->ui_manager()->RemoveAllowlistUrlSetThreatType(
          base::PassKey<SuspiciousSiteControllerDesktop>(),
          current_suspicious_url_, navigation_id_, web_contents(),
          /*from_pending_only=*/true,
          SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE);
    }
  }
}

// static
void SuspiciousSiteControllerDesktop::ShowForWebContents(
    content::WebContents* web_contents,
    int64_t navigation_id) {
  if (FromWebContents(web_contents)) {
    web_contents->RemoveUserData(UserDataKey());
  }
  CreateForWebContents(web_contents);
  auto* controller = FromWebContents(web_contents);
  controller->navigation_id_ = navigation_id;
  controller->is_suspended_ = true;

  auto* tracker = AsyncCheckTracker::FromWebContents(web_contents);
  if (tracker && !controller->async_check_observation_.IsObserving()) {
    controller->async_check_observation_.Observe(tracker);
  }

  controller->MaybeShowBubble();
}

void SuspiciousSiteControllerDesktop::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // If this navigation failed or if a different navigation completed, clean up.
  if (navigation_handle->IsErrorPage() || !navigation_id_.has_value() ||
      navigation_handle->GetNavigationId() != navigation_id_.value()) {
    web_contents()->RemoveUserData(UserDataKey());
    return;
  }

  navigation_committed_ = true;
  MaybeShowBubble();
}

void SuspiciousSiteControllerDesktop::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::VISIBLE && is_suspended_) {
    MaybeShowBubble();
  }
}

void SuspiciousSiteControllerDesktop::OnAsyncSafeBrowsingCheckCompleted() {
  MaybeShowBubble();
}

void SuspiciousSiteControllerDesktop::
    OnAsyncSafeBrowsingCheckTrackerDestructed() {
  async_check_observation_.Reset();
}

void SuspiciousSiteControllerDesktop::MaybeShowBubble() {
  if (!web_contents() || !navigation_id_.has_value()) {
    return;
  }

  // 1. Interstitials take precedence: suppress and clean up if an interstitial
  // is pending/active.
  auto* interstitial_helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          web_contents());
  if (interstitial_helper &&
      interstitial_helper->HasPendingOrActiveInterstitial()) {
    web_contents()->RemoveUserData(UserDataKey());
    return;
  }

  // 2. Defer if background async Safe Browsing checks are still in-flight.
  auto* tracker = AsyncCheckTracker::FromWebContents(web_contents());
  if (tracker && tracker->IsNavigationPending(navigation_id_.value())) {
    is_suspended_ = true;
    return;
  }

  // 3. Defer if target navigation has not committed yet.
  if (!navigation_committed_) {
    if (tracker &&
        tracker->GetNavigationCommittedTimestamp(navigation_id_.value())
            .has_value()) {
      navigation_committed_ = true;
    } else {
      is_suspended_ = true;
      return;
    }
  }

  // 4. Suppress and clean up if the site has been allowlisted by the user.
  if (web_contents()->GetBrowserContext()) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    if (profile) {
      HostContentSettingsMap* hcsm =
          HostContentSettingsMapFactory::GetForProfile(profile);
      if (hcsm &&
          SuspiciousSiteWarningAllowlist(hcsm).IsSiteAllowedForHost(
              std::string(web_contents()->GetLastCommittedURL().host()))) {
        web_contents()->RemoveUserData(UserDataKey());
        return;
      }
    }
  }

  // 5. Defer if web contents is hidden.
  if (web_contents()->GetVisibility() != content::Visibility::VISIBLE) {
    is_suspended_ = true;
    return;
  }

  is_suspended_ = false;
  ShowBubble();
}

void SuspiciousSiteControllerDesktop::ShowBubble() {
  if (GetShownCallback() && !GetShownCallback()->is_null()) {
    std::move(*GetShownCallback()).Run();
  }

  SafeBrowsingService* sb_service = g_browser_process->safe_browsing_service();
  if (sb_service && sb_service->ui_manager()) {
    if (!current_suspicious_url_.is_empty()) {
      sb_service->ui_manager()->RemoveAllowlistUrlSetThreatType(
          base::PassKey<SuspiciousSiteControllerDesktop>(),
          current_suspicious_url_, navigation_id_, web_contents(),
          /*from_pending_only=*/true,
          SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE);
    }
  }

  current_suspicious_url_ = web_contents()->GetLastCommittedURL();

  if (sb_service && sb_service->ui_manager()) {
    sb_service->ui_manager()->AddToAllowlistUrlSet(
        current_suspicious_url_, navigation_id_, web_contents(),
        /*is_pending=*/true,
        SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE);
  }

  ShowSuspiciousSiteBubble(web_contents());

  if (!has_shown_) {
    LogUserInteraction(UserInteraction::kShown);
    has_shown_ = true;
  }
}

void SuspiciousSiteControllerDesktop::OnBackToSafetyClicked() {
  has_shown_ = true;
  warning_outcome_ = WarningOutcome::kAdhered;
  LogUserInteraction(UserInteraction::kBackToSafetyButton);

  content::WebContents* contents = web_contents();
  CHECK(contents);
  auto& controller = contents->GetController();
  if (controller.CanGoBack()) {
    controller.GoBack();
  } else {
    controller.LoadURLWithParams(content::NavigationController::LoadURLParams(
        GURL(chrome::kChromeUINewTabURL)));
  }

  contents->RemoveUserData(UserDataKey());
}

void SuspiciousSiteControllerDesktop::OnMarkAsSafeClicked() {
  has_shown_ = true;
  warning_outcome_ = WarningOutcome::kBypassed;
  LogUserInteraction(UserInteraction::kMarkAsSafe);

  if (web_contents()->GetBrowserContext()) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    if (profile) {
      HostContentSettingsMap* hcsm =
          HostContentSettingsMapFactory::GetForProfile(profile);
      if (hcsm) {
        SuspiciousSiteWarningAllowlist(hcsm).AllowSiteForHost(
            std::string(web_contents()->GetLastCommittedURL().host()));
      }
    }
  }

  web_contents()->RemoveUserData(UserDataKey());
}

void SuspiciousSiteControllerDesktop::OnLearnMoreClicked() {
  LogUserInteraction(UserInteraction::kLearnMore);

  if (web_contents()) {
    web_contents()->OpenURL(
        content::OpenURLParams(GURL(chrome::kSafeBrowsingHelpCenterURL),
                               content::Referrer(),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui::PAGE_TRANSITION_LINK, false),
        /*navigation_handle_callback=*/{});
  }
}

void SuspiciousSiteControllerDesktop::OnBubbleDestroyed() {
  LogUserInteraction(UserInteraction::kDestroyed);

  if (GetDestroyedCallback() && !GetDestroyedCallback()->is_null()) {
    std::move(*GetDestroyedCallback()).Run();
  }
}

// static
void SuspiciousSiteControllerDesktop::
    SetBubbleShownCallbackForTesting(  // IN-TEST
        base::OnceClosure callback) {
  *GetShownCallback() = std::move(callback);
}

// static
void SuspiciousSiteControllerDesktop::
    SetBubbleDestroyedCallbackForTesting(  // IN-TEST
        base::OnceClosure callback) {
  *GetDestroyedCallback() = std::move(callback);
}

}  // namespace safe_browsing
