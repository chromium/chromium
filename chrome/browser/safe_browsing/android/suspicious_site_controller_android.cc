// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/suspicious_site_controller_android.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/android/safe_browsing/suspicious_site_dialog_view_android.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/browser/suspicious_site_warning_allowlist.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace safe_browsing {
namespace {

base::OnceClosure* GetShownCallback() {
  static base::NoDestructor<base::OnceClosure> callback;
  return callback.get();
}

base::OnceClosure* GetDismissedCallback() {
  static base::NoDestructor<base::OnceClosure> callback;
  return callback.get();
}

}  // namespace

WEB_CONTENTS_USER_DATA_KEY_IMPL(SuspiciousSiteControllerAndroid);

SuspiciousSiteControllerAndroid::SuspiciousSiteControllerAndroid(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SuspiciousSiteControllerAndroid>(
          *web_contents) {
  CHECK(web_contents->GetPrimaryMainFrame());
}

SuspiciousSiteControllerAndroid::~SuspiciousSiteControllerAndroid() {
  if (web_contents() && is_observing_async_check_tracker_) {
    auto* tracker = AsyncCheckTracker::FromWebContents(web_contents());
    if (tracker) {
      tracker->RemoveObserver(this);
    }
  }

  // If the dialog was shown, log the tracked warning outcome directly.
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
          current_suspicious_url_, navigation_id_, web_contents(),
          /*from_pending_only=*/true,
          SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE);
    }
  }
}

// static
void SuspiciousSiteControllerAndroid::ShowForWebContents(
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
  if (tracker && !controller->is_observing_async_check_tracker_) {
    tracker->AddObserver(controller);
    controller->is_observing_async_check_tracker_ = true;
  }

  controller->MaybeShowDialog();
}

void SuspiciousSiteControllerAndroid::DidFinishNavigation(
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
  MaybeShowDialog();
}

void SuspiciousSiteControllerAndroid::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::VISIBLE && is_suspended_) {
    MaybeShowDialog();
  }
}

void SuspiciousSiteControllerAndroid::OnAsyncSafeBrowsingCheckCompleted() {
  MaybeShowDialog();
}

void SuspiciousSiteControllerAndroid::
    OnAsyncSafeBrowsingCheckTrackerDestructed() {
  // AsyncCheckTracker is being destroyed; no action required.
}

void SuspiciousSiteControllerAndroid::MaybeShowDialog() {
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

  // 5. Defer if native Android window is not attached or web contents is
  // hidden.
  ui::WindowAndroid* window_android = web_contents()->GetTopLevelNativeWindow();
  if (!window_android ||
      web_contents()->GetVisibility() != content::Visibility::VISIBLE) {
    is_suspended_ = true;
    return;
  }

  is_suspended_ = false;
  ShowDialog();
}

void SuspiciousSiteControllerAndroid::ShowDialog() {
  if (GetShownCallback() && !GetShownCallback()->is_null()) {
    std::move(*GetShownCallback()).Run();
  }

  CHECK(web_contents());
  ui::WindowAndroid* window_android = web_contents()->GetTopLevelNativeWindow();
  if (!window_android) {
    is_suspended_ = true;
    return;
  }

  has_shown_ = true;
  SafeBrowsingService* sb_service = g_browser_process->safe_browsing_service();
  if (sb_service && sb_service->ui_manager()) {
    // If a previous suspicious site URL was registered for this tab, clear it
    // first before setting the new one.
    if (!current_suspicious_url_.is_empty()) {
      sb_service->ui_manager()->RemoveAllowlistUrlSetThreatType(
          current_suspicious_url_, navigation_id_, web_contents(),
          /*from_pending_only=*/true,
          SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE);
    }
    // Add the suspicious site URL to AllowlistUrlSet with pending=true.
    // AllowlistUrlSet stores the threat type for WebContents, enabling
    // ChromeSecurityStateTabHelper to return
    // MALICIOUS_CONTENT_STATUS_WARNABLE_SUSPICIOUS_SITE so the red warning icon
    // remains active in the Omnibox and Page Info even if the dialog is closed.
    current_suspicious_url_ = web_contents()->GetLastCommittedURL();
    sb_service->ui_manager()->AddToAllowlistUrlSet(
        current_suspicious_url_, navigation_id_, web_contents(),
        /*is_pending=*/true,
        SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE);
  }
  dialog_view_.reset();
  dialog_view_ = std::make_unique<SuspiciousSiteDialogViewAndroid>(*this);
  // TODO(crbug.com/532598569): Investigate if destroying an existing dialog,
  // creating a new one, and displaying it causes UI flicker.
  dialog_view_->Show(*window_android);
}

void SuspiciousSiteControllerAndroid::CloseDialog(
    ui::ModalDialogWrapper::DismissalCause dismissal_cause) {
  if (GetDismissedCallback() && !GetDismissedCallback()->is_null()) {
    std::move(*GetDismissedCallback()).Run();
  }

  if (dismissal_cause ==
      ui::ModalDialogWrapper::DismissalCause::NAVIGATE_BACK) {
    OnGoBackButtonClicked();
  } else if (dismissal_cause ==
                 ui::ModalDialogWrapper::DismissalCause::NAVIGATE ||
             dismissal_cause ==
                 ui::ModalDialogWrapper::DismissalCause::TAB_SWITCHED ||
             dismissal_cause ==
                 ui::ModalDialogWrapper::DismissalCause::ACTIVITY_DESTROYED ||
             dismissal_cause == ui::ModalDialogWrapper::DismissalCause::
                                    DIALOG_INTERACTION_DEFERRED) {
    is_suspended_ = true;
    dialog_view_.reset();
  } else if (dismissal_cause ==
                 ui::ModalDialogWrapper::DismissalCause::TOUCH_OUTSIDE ||
             dismissal_cause == ui::ModalDialogWrapper::DismissalCause::
                                    POSITIVE_BUTTON_CLICKED ||
             dismissal_cause ==
                 ui::ModalDialogWrapper::DismissalCause::ACTION_ON_CONTENT) {
    warning_outcome_ = WarningOutcome::kBypassed;
    is_suspended_ = false;
    dialog_view_.reset();
  } else {
    is_suspended_ = false;
    dialog_view_.reset();
  }
}

void SuspiciousSiteControllerAndroid::OnGoBackButtonClicked() {
  has_shown_ = true;
  warning_outcome_ = WarningOutcome::kAdhered;
  dialog_view_.reset();

  content::WebContents* contents = web_contents();
  CHECK(contents);
  auto& controller = contents->GetController();
  if (controller.CanGoBack()) {
    controller.GoBack();
  } else {
    controller.LoadURLWithParams(content::NavigationController::LoadURLParams(
        GURL(chrome::kChromeUINewTabURL)));
  }

  // NOTE: Calling RemoveUserData synchronously destroys this object, so there
  // must be no member accesses after this point.
  contents->RemoveUserData(UserDataKey());
}

void SuspiciousSiteControllerAndroid::OnContinueButtonClicked() {
  has_shown_ = true;
  warning_outcome_ = WarningOutcome::kBypassed;
  dialog_view_.reset();
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
  // NOTE: Calling RemoveUserData synchronously destroys this object, so there
  // must be no member accesses after this point.
  web_contents()->RemoveUserData(UserDataKey());
}

void SuspiciousSiteControllerAndroid::OnHelpCenterLinkClicked() {
  content::WebContents* contents = web_contents();
  CHECK(contents);
  content::OpenURLParams params(
      GURL(chrome::kUnsafeSiteWarningHelpCenterURL), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      /*is_renderer_initiated=*/false);

  contents->OpenURL(params, /*navigation_handle_callback=*/{});
}

std::u16string SuspiciousSiteControllerAndroid::GetPrimaryButtonText() const {
  return l10n_util::GetStringUTF16(IDS_SUSPICIOUS_SITE_BACK_TO_SAFETY);
}

std::u16string SuspiciousSiteControllerAndroid::GetSecondaryButtonText() const {
  return l10n_util::GetStringUTF16(IDS_SUSPICIOUS_SITE_PROCEED_BUTTON);
}

std::u16string SuspiciousSiteControllerAndroid::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_SUSPICIOUS_SITE_TITLE);
}

std::u16string SuspiciousSiteControllerAndroid::GetWarningDetailText() const {
  return l10n_util::GetStringUTF16(IDS_SUSPICIOUS_SITE_MESSAGE);
}

// static
void SuspiciousSiteControllerAndroid::SetDialogShownCallbackForTesting(
    base::OnceClosure callback) {
  *GetShownCallback() = std::move(callback);
}

// static
void SuspiciousSiteControllerAndroid::SetDialogDismissedCallbackForTesting(
    base::OnceClosure callback) {
  *GetDismissedCallback() = std::move(callback);
}

}  // namespace safe_browsing
