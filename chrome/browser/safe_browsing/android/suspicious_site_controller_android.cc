// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/suspicious_site_controller_android.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/android/safe_browsing_referring_app_bridge_android.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/android/safe_browsing/suspicious_site_dialog_view_android.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/browser/suspicious_site_warning_allowlist.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"
#include "url/origin.h"

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

const char* UserChoiceToString(
    SuspiciousSiteControllerAndroid::UserChoice choice) {
  switch (choice) {
    case SuspiciousSiteControllerAndroid::UserChoice::kMarkAsSafe:
      return "mark_as_safe";
    case SuspiciousSiteControllerAndroid::UserChoice::kBackToSafety:
      return "back_to_safety";
    case SuspiciousSiteControllerAndroid::UserChoice::kDismiss:
      return "dismiss";
    case SuspiciousSiteControllerAndroid::UserChoice::kManualNavigation:
      return "manual_navigation";
  }
  NOTREACHED();
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

  // Reset closing state for the current dialog presentation.
  is_closing_ = false;

  ui::WindowAndroid* window_android = web_contents()->GetTopLevelNativeWindow();
  if (!window_android) {
    is_suspended_ = true;
    return;
  }

  dialog_shown_time_ = base::TimeTicks::Now();
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
  }

  // Populate current_suspicious_url_ for the active warning session.
  current_suspicious_url_ = web_contents()->GetLastCommittedURL();

  // Pre-fetch repeat visit count right after populating
  // current_suspicious_url_.
  FetchRepeatVisitCount();

  if (sb_service && sb_service->ui_manager()) {
    // Add the suspicious site URL to AllowlistUrlSet with pending=true.
    // AllowlistUrlSet stores the threat type for WebContents, enabling
    // ChromeSecurityStateTabHelper to return
    // MALICIOUS_CONTENT_STATUS_WARNABLE_SUSPICIOUS_SITE so the red warning icon
    // remains active in the Omnibox and Page Info even if the dialog is closed.
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
  if (!has_shown_) {
    base::UmaHistogramEnumeration(
        "SafeBrowsing.SuspiciousSiteWarning.UserInteraction",
        UserInteraction::kShown);
    has_shown_ = true;
  }
}

void SuspiciousSiteControllerAndroid::CloseDialog(
    ui::ModalDialogWrapper::DismissalCause dismissal_cause) {
  if (is_closing_) {
    return;
  }

  // Prevent false telemetry/HaTS triggers from phantom interactions while
  // hidden, but allow terminal system events to properly clean up backgrounded
  // tabs.
  if (is_suspended_ &&
      dismissal_cause !=
          ui::ModalDialogWrapper::DismissalCause::TAB_DESTROYED &&
      dismissal_cause !=
          ui::ModalDialogWrapper::DismissalCause::WEB_CONTENTS_DESTROYED) {
    return;
  }

  if (GetDismissedCallback() && !GetDismissedCallback()->is_null()) {
    std::move(*GetDismissedCallback()).Run();
  }

  switch (dismissal_cause) {
    case ui::ModalDialogWrapper::DismissalCause::NAVIGATE_BACK:
      HandleBackNavigation(UserInteraction::kSystemBack);
      return;
    case ui::ModalDialogWrapper::DismissalCause::NAVIGATE:
      is_closing_ = true;
      warning_outcome_ = WarningOutcome::kAdhered;
      base::UmaHistogramEnumeration(
          "SafeBrowsing.SuspiciousSiteWarning.UserInteraction",
          UserInteraction::kManualNavigation);
      MaybeTriggerHatsSurvey(UserChoice::kManualNavigation);
      is_suspended_ = true;
      break;
    case ui::ModalDialogWrapper::DismissalCause::TAB_SWITCHED:
    case ui::ModalDialogWrapper::DismissalCause::DIALOG_INTERACTION_DEFERRED:
    case ui::ModalDialogWrapper::DismissalCause::ACTIVITY_DESTROYED:
      if (!dialog_shown_time_.is_null()) {
        dialog_state_.accumulated_visible_time +=
            base::TimeTicks::Now() - dialog_shown_time_;
        dialog_shown_time_ = base::TimeTicks();
      }
      is_suspended_ = true;
      break;
    case ui::ModalDialogWrapper::DismissalCause::TAB_DESTROYED:
    case ui::ModalDialogWrapper::DismissalCause::WEB_CONTENTS_DESTROYED:
      is_closing_ = true;
      warning_outcome_ = WarningOutcome::kAdhered;
      base::UmaHistogramEnumeration(
          "SafeBrowsing.SuspiciousSiteWarning.UserInteraction",
          UserInteraction::kCloseTab);
      is_suspended_ = true;
      break;
    case ui::ModalDialogWrapper::DismissalCause::TOUCH_OUTSIDE:
    case ui::ModalDialogWrapper::DismissalCause::ACTION_ON_CONTENT:
      is_closing_ = true;
      warning_outcome_ = WarningOutcome::kBypassed;
      base::UmaHistogramEnumeration(
          "SafeBrowsing.SuspiciousSiteWarning.UserInteraction",
          UserInteraction::kDismissed);
      MaybeTriggerHatsSurvey(UserChoice::kDismiss);
      is_suspended_ = false;
      break;
    default:
      is_closing_ = true;
      is_suspended_ = false;
      break;
  }

  dialog_view_.reset();
}

void SuspiciousSiteControllerAndroid::HandleBackNavigation(
    UserInteraction interaction_type) {
  if (is_closing_) {
    return;
  }
  is_closing_ = true;
  has_shown_ = true;
  warning_outcome_ = WarningOutcome::kAdhered;
  dialog_view_.reset();

  base::UmaHistogramEnumeration(
      "SafeBrowsing.SuspiciousSiteWarning.UserInteraction", interaction_type);

  MaybeTriggerHatsSurvey(UserChoice::kBackToSafety);

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
  if (is_closing_) {
    return;
  }
  is_closing_ = true;
  has_shown_ = true;
  warning_outcome_ = WarningOutcome::kBypassed;
  dialog_view_.reset();

  base::UmaHistogramEnumeration(
      "SafeBrowsing.SuspiciousSiteWarning.UserInteraction",
      UserInteraction::kMarkAsSafe);

  MaybeTriggerHatsSurvey(UserChoice::kMarkAsSafe);

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

void SuspiciousSiteControllerAndroid::FetchRepeatVisitCount() {
  if (dialog_state_.has_fetched_history || !web_contents() ||
      !web_contents()->GetBrowserContext()) {
    return;
  }
  GURL site_url = web_contents()->GetLastCommittedURL();
  if (!site_url.is_valid()) {
    return;
  }

  dialog_state_.has_fetched_history = true;
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (!profile) {
    return;
  }
  // Use IMPLICIT_ACCESS because this history query is an automated background
  // feature check for survey telemetry rather than a direct user interaction
  // with the History UI.
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::IMPLICIT_ACCESS);
  if (history_service) {
    history_service->GetVisibleVisitCountToHost(
        site_url,
        base::BindOnce(&SuspiciousSiteControllerAndroid::OnGetVisibleVisitCount,
                       weak_ptr_factory_.GetWeakPtr()),
        &history_task_tracker_);
  }
}

void SuspiciousSiteControllerAndroid::OnGetVisibleVisitCount(
    history::VisibleVisitCountToHostResult result) {
  if (result.success && result.count > 1) {
    dialog_state_.repeat_visit = true;
  }
}

void SuspiciousSiteControllerAndroid::MaybeTriggerHatsSurvey(
    UserChoice user_choice) {
  if (!base::FeatureList::IsEnabled(
          safe_browsing::kSuspiciousSiteWarningSurvey)) {
    return;
  }
  if (!web_contents() || !web_contents()->GetBrowserContext()) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (!profile) {
    return;
  }
  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile, /*create_if_necessary=*/true);
  if (!hats_service) {
    return;
  }

  base::TimeDelta total_visible_time = dialog_state_.accumulated_visible_time;
  if (!dialog_shown_time_.is_null()) {
    total_visible_time += base::TimeTicks::Now() - dialog_shown_time_;
  }
  std::string visible_time_str =
      base::StringPrintf("%.2f", total_visible_time.InSecondsF());

  GURL referrer_gurl;
  if (auto* entry = web_contents()->GetController().GetLastCommittedEntry()) {
    referrer_gurl = entry->GetReferrer().url;
  }

  std::string referring_app;
  internal::ReferringAppInfo referring_app_info =
      GetReferringAppInfo(web_contents(), /*get_webapk_info=*/false);
  if (referring_app_info.has_referring_app()) {
    referring_app = referring_app_info.referring_app_name;
  }

  std::string referrer_origin;
  if (referrer_gurl.is_valid()) {
    url::Origin origin = url::Origin::Create(referrer_gurl);
    if (!origin.opaque()) {
      referrer_origin = origin.Serialize();
    }
  }

  std::string site_origin =
      current_suspicious_url_.is_valid()
          ? url::Origin::Create(current_suspicious_url_).Serialize()
          : "";

  SurveyBitsData bits_data = {
      {"did_proceed", warning_outcome_ == WarningOutcome::kBypassed},
      {"learn_more_clicked", dialog_state_.learn_more_clicked},
      {"repeat_visit", dialog_state_.repeat_visit}};
  SurveyStringData string_data = {
      {"site_origin", site_origin},
      {"user_choice", UserChoiceToString(user_choice)},
      {"time_prompt_visible", visible_time_str},
      {"referrer_origin", referrer_origin},
      {"referring_app", referring_app}};

  std::string trigger_id =
      warning_outcome_ == WarningOutcome::kBypassed
          ? safe_browsing::kSuspiciousSiteWarningSurveyProceedTriggerId.Get()
          : safe_browsing::kSuspiciousSiteWarningSurveyHeedTriggerId.Get();

  HatsService::SurveyOptions survey_options(
      /*custom_invitation=*/
      l10n_util::GetStringUTF16(IDS_SUSPICIOUS_SITE_HATS_CUSTOM_INVITATION));
  hats_service->LaunchSurveyForWebContents(
      kHatsSurveyTriggerSuspiciousSiteWarning, web_contents(), bits_data,
      string_data, /*success_callback=*/base::DoNothing(),
      /*failure_callback=*/base::DoNothing(),
      trigger_id.empty() ? std::nullopt : std::make_optional(trigger_id),
      survey_options);
}

void SuspiciousSiteControllerAndroid::OnHelpCenterLinkClicked() {
  if (!dialog_state_.learn_more_clicked) {
    dialog_state_.learn_more_clicked = true;
    base::UmaHistogramEnumeration(
        "SafeBrowsing.SuspiciousSiteWarning.UserInteraction",
        UserInteraction::kLearnMore);
  }
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
