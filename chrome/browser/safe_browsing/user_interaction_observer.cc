// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/user_interaction_observer.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

using blink::WebInputEvent;

namespace {
// Id for extension that enables users to report sites to Safe Browsing.
const char kPreventElisionExtensionId[] = "jknemblkbdhdcpllfgbfekkdciegfboi";
}  // namespace

namespace safe_browsing {

const char kDelayedWarningsHistogram[] = "SafeBrowsing.DelayedWarnings.Event";
const char kDelayedWarningsTimeOnPageHistogram[] =
    "SafeBrowsing.DelayedWarnings.TimeOnPage";

const char kDelayedWarningsWithElisionDisabledHistogram[] =
    "SafeBrowsing.DelayedWarnings.Event_UrlElisionDisabled";
const char kDelayedWarningsTimeOnPageWithElisionDisabledHistogram[] =
    "SafeBrowsing.DelayedWarnings.TimeOnPage_UrlElisionDisabled";

namespace {
const char kWebContentsUserDataKey[] =
    "web_contents_safe_browsing_user_interaction_observer";

bool IsUrlElisionDisabled(Profile* profile,
                          const char* suspicious_site_reporter_extension_id) {
  if (profile &&
      profile->GetPrefs()->GetBoolean(omnibox::kPreventUrlElisionsInOmnibox)) {
    return true;
  }
#if BUILDFLAG(ENABLE_EXTENSIONS)
  DCHECK(suspicious_site_reporter_extension_id);
  if (profile && extensions::ExtensionRegistry::Get(profile)
                     ->enabled_extensions()
                     .Contains(suspicious_site_reporter_extension_id)) {
    return true;
  }
#endif
  return false;
}

}  // namespace

// static
const char* SafeBrowsingUserInteractionObserver::
    suspicious_site_reporter_extension_id_ = kPreventElisionExtensionId;

SafeBrowsingUserInteractionObserver::SafeBrowsingUserInteractionObserver(
    content::WebContents* web_contents,
    const security_interstitials::UnsafeResource& resource,
    bool is_main_frame,
    scoped_refptr<SafeBrowsingUIManager> ui_manager)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents),
      resource_(resource),
      ui_manager_(ui_manager),
      creation_time_(base::Time::Now()),
      clock_(base::DefaultClock::GetInstance()) {
  DCHECK(base::FeatureList::IsEnabled(kDelayedWarnings));
  key_press_callback_ =
      base::BindRepeating(&SafeBrowsingUserInteractionObserver::HandleKeyPress,
                          base::Unretained(this));
  mouse_event_callback_ = base::BindRepeating(
      &SafeBrowsingUserInteractionObserver::HandleMouseEvent,
      base::Unretained(this));
  // Pass a callback to the render widget host instead of implementing
  // WebContentsObserver::DidGetUserInteraction(). The reason for this is that
  // render widget host handles keyboard events earlier and the callback can
  // indicate that it wants the key press to be ignored.
  // (DidGetUserInteraction() can only observe and not cancel the event.)
  web_contents->GetRenderViewHost()->GetWidget()->AddKeyPressEventCallback(
      key_press_callback_);
  web_contents->GetRenderViewHost()->GetWidget()->AddMouseEventCallback(
      mouse_event_callback_);

  // Observe permission bubble events.
  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  if (permission_request_manager) {
    permission_request_manager->AddObserver(this);
  }
  RecordUMA(DelayedWarningEvent::kPageLoaded);
}

SafeBrowsingUserInteractionObserver::~SafeBrowsingUserInteractionObserver() {
  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  if (permission_request_manager) {
    permission_request_manager->RemoveObserver(this);
  }
  web_contents_->GetRenderViewHost()->GetWidget()->RemoveKeyPressEventCallback(
      key_press_callback_);
  web_contents_->GetRenderViewHost()->GetWidget()->RemoveMouseEventCallback(
      mouse_event_callback_);
}

// static
void SafeBrowsingUserInteractionObserver::CreateForWebContents(
    content::WebContents* web_contents,
    const security_interstitials::UnsafeResource& resource,
    bool is_main_frame,
    scoped_refptr<SafeBrowsingUIManager> ui_manager) {
  // This method is called for all unsafe resources on |web_contents|. Only
  // create an observer if there isn't one.
  // TODO(crbug.com/1057157): The observer should observe all unsafe resources
  // instead of the first one only.
  if (FromWebContents(web_contents)) {
    return;
  }
  DCHECK(!web_contents->IsPortal());
  auto observer = std::make_unique<SafeBrowsingUserInteractionObserver>(
      web_contents, resource, is_main_frame, ui_manager);
  web_contents->SetUserData(kWebContentsUserDataKey, std::move(observer));
}

// static
SafeBrowsingUserInteractionObserver*
SafeBrowsingUserInteractionObserver::FromWebContents(
    content::WebContents* web_contents) {
  return static_cast<SafeBrowsingUserInteractionObserver*>(
      web_contents->GetUserData(kWebContentsUserDataKey));
}

void SafeBrowsingUserInteractionObserver::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  // |old_host| can be nullptr if the old RVH was shut down.
  if (old_host)
    old_host->GetWidget()->RemoveKeyPressEventCallback(key_press_callback_);
  new_host->GetWidget()->AddKeyPressEventCallback(key_press_callback_);

  if (old_host)
    old_host->GetWidget()->RemoveMouseEventCallback(mouse_event_callback_);
  new_host->GetWidget()->AddMouseEventCallback(mouse_event_callback_);
}

void SafeBrowsingUserInteractionObserver::WebContentsDestroyed() {
  CleanUp();
  Detach();
}

void SafeBrowsingUserInteractionObserver::DidFinishNavigation(
    content::NavigationHandle* handle) {
  // Remove the observer on a top frame navigation to another page. The user is
  // now on another page so we don't need to wait for an interaction.
  if (!handle->IsInMainFrame() || handle->IsSameDocument()) {
    return;
  }
  // If this is the first navigation we are seeing, it must be the
  // navigation that caused this observer to be created.
  // As an example, if the user navigates to http://test.site, the order of
  // events are:
  // 1. SafeBrowsingUrlCheckerImpl detects that the URL should be blocked with
  //    an interstitial.
  // 2. It delays the interstitial and creates an instance of this class.
  // 3. DidFinishNavigation() of this class is called.
  //
  // This means that the first time we are here, we should ignore this event
  // because it's not an interesting navigation. We only want to handle the
  // navigations that follow.
  if (!initial_navigation_finished_) {
    initial_navigation_finished_ = true;
    return;
  }
  // If a download happens when an instance of this observer is attached to
  // the WebContents, DelayedNavigationThrottle cancels the download. As a
  // result, the page should remain unchanged on downloads. Record a metric and
  // ignore this cancelled navigation.
  if (handle->IsDownload()) {
    RecordUMA(DelayedWarningEvent::kDownloadCancelled);
    return;
  }
  Detach();
  // DO NOT add code past this point. |this| is destroyed.
}

void SafeBrowsingUserInteractionObserver::Detach() {
  if (!interstitial_shown_) {
    RecordUMA(DelayedWarningEvent::kWarningNotShown);
  }
  base::TimeDelta time_on_page = clock_->Now() - creation_time_;
  if (IsUrlElisionDisabled(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()),
          suspicious_site_reporter_extension_id_)) {
    base::UmaHistogramLongTimes(
        kDelayedWarningsTimeOnPageWithElisionDisabledHistogram, time_on_page);
  } else {
    base::UmaHistogramLongTimes(kDelayedWarningsTimeOnPageHistogram,
                                time_on_page);
  }
  web_contents()->RemoveUserData(kWebContentsUserDataKey);
  // DO NOT add code past this point. |this| is destroyed.
}

void SafeBrowsingUserInteractionObserver::DidToggleFullscreenModeForTab(
    bool entered_fullscreen,
    bool will_cause_resize) {
  // This class is only instantiated upon a navigation. If a page is in
  // fullscreen mode, any navigation away from it should exit fullscreen. This
  // means that this class is never instantiated while the current web contents
  // is in fullscreen mode, so |entered_fullscreen| should never be false when
  // this method is called for the first time. However, we don't know if it's
  // guaranteed for a page to not be in fullscreen upon navigation, so we just
  // ignore this event if the page exited fullscreen.
  if (!entered_fullscreen) {
    return;
  }
  // IMPORTANT: Store the web contents pointer in a temporary because |this| is
  // deleted after ShowInterstitial().
  content::WebContents* contents = web_contents();
  ShowInterstitial(DelayedWarningEvent::kWarningShownOnFullscreenAttempt);
  // Exit fullscreen only after navigating to the interstitial. We don't want to
  // interfere with an ongoing fullscreen request.
  contents->ExitFullscreen(will_cause_resize);
  // DO NOT add code past this point. |this| is destroyed.
}

void SafeBrowsingUserInteractionObserver::OnPaste() {
  ShowInterstitial(DelayedWarningEvent::kWarningShownOnPaste);
  // DO NOT add code past this point. |this| is destroyed.
}

void SafeBrowsingUserInteractionObserver::OnBubbleAdded() {
  // The page requested a permission that triggered a permission prompt. Deny
  // and show the interstitial.
  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  if (!permission_request_manager) {
    return;
  }
  permission_request_manager->Deny();
  ShowInterstitial(DelayedWarningEvent::kWarningShownOnPermissionRequest);
  // DO NOT add code past this point. |this| is destroyed.
}

void SafeBrowsingUserInteractionObserver::OnJavaScriptDialog() {
  ShowInterstitial(DelayedWarningEvent::kWarningShownOnJavaScriptDialog);
  // DO NOT add code past this point. |this| is destroyed.
}

void SafeBrowsingUserInteractionObserver::OnPasswordSaveOrAutofillDenied() {
  if (password_save_or_autofill_denied_metric_recorded_) {
    return;
  }
  password_save_or_autofill_denied_metric_recorded_ = true;
  RecordUMA(DelayedWarningEvent::kPasswordSaveOrAutofillDenied);
}

void SafeBrowsingUserInteractionObserver::OnDesktopCaptureRequest() {
  ShowInterstitial(DelayedWarningEvent::kWarningShownOnDesktopCaptureRequest);
  // DO NOT add code past this point. |this| is destroyed.
}

// static
void SafeBrowsingUserInteractionObserver::
    SetSuspiciousSiteReporterExtensionIdForTesting(const char* extension_id) {
  suspicious_site_reporter_extension_id_ = extension_id;
}

// static
void SafeBrowsingUserInteractionObserver::
    ResetSuspiciousSiteReporterExtensionIdForTesting() {
  suspicious_site_reporter_extension_id_ = kPreventElisionExtensionId;
}

void SafeBrowsingUserInteractionObserver::SetClockForTesting(
    base::Clock* clock) {
  clock_ = clock;
}

base::Time SafeBrowsingUserInteractionObserver::GetCreationTimeForTesting()
    const {
  return creation_time_;
}

void SafeBrowsingUserInteractionObserver::RecordUMA(DelayedWarningEvent event) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (IsUrlElisionDisabled(profile, suspicious_site_reporter_extension_id_)) {
    base::UmaHistogramEnumeration(kDelayedWarningsWithElisionDisabledHistogram,
                                  event);
  } else {
    base::UmaHistogramEnumeration(kDelayedWarningsHistogram, event);
  }
}

bool IsAllowedModifier(const content::NativeWebKeyboardEvent& event) {
  const int key_modifiers =
      event.GetModifiers() & blink::WebInputEvent::kKeyModifiers;
  // If the only modifier is shift, the user may be typing uppercase
  // letters.
  if (key_modifiers == WebInputEvent::kShiftKey) {
    return event.windows_key_code == ui::VKEY_SHIFT;
  }
  // Disallow CTRL+C and CTRL+V.
  if (key_modifiers == WebInputEvent::kControlKey &&
      (event.windows_key_code == ui::VKEY_C ||
       event.windows_key_code == ui::VKEY_V)) {
    return false;
  }
  return key_modifiers != 0;
}

bool SafeBrowsingUserInteractionObserver::HandleKeyPress(
    const content::NativeWebKeyboardEvent& event) {
  // Allow non-character keys such as ESC. These can be used to exit fullscreen,
  // for example.
  if (!event.IsCharacterKey() || event.is_browser_shortcut ||
      IsAllowedModifier(event)) {
    return false;
  }
  ShowInterstitial(DelayedWarningEvent::kWarningShownOnKeypress);
  // DO NOT add code past this point. |this| is destroyed.
  return true;
}

bool SafeBrowsingUserInteractionObserver::HandleMouseEvent(
    const blink::WebMouseEvent& event) {
  if (event.GetType() != blink::WebInputEvent::Type::kMouseDown) {
    return false;
  }
  // If warning isn't enabled for mouse clicks, still record the first time when
  // the user clicks.
  if (!kDelayedWarningsEnableMouseClicks.Get()) {
    if (!mouse_click_with_no_warning_recorded_) {
      RecordUMA(DelayedWarningEvent::kWarningNotTriggeredOnMouseClick);
      mouse_click_with_no_warning_recorded_ = true;
    }
    return false;
  }
  ShowInterstitial(DelayedWarningEvent::kWarningShownOnMouseClick);
  // DO NOT add code past this point. |this| is destroyed.
  return true;
}

void SafeBrowsingUserInteractionObserver::ShowInterstitial(
    DelayedWarningEvent event) {
  // Show the interstitial.
  DCHECK(!interstitial_shown_);
  interstitial_shown_ = true;
  CleanUp();
  RecordUMA(event);
  SafeBrowsingUIManager::StartDisplayingBlockingPage(ui_manager_, resource_);
  Detach();
  // DO NOT add code past this point. |this| is destroyed.
}

void SafeBrowsingUserInteractionObserver::CleanUp() {
  web_contents_->GetRenderViewHost()->GetWidget()->RemoveKeyPressEventCallback(
      key_press_callback_);
  web_contents_->GetRenderViewHost()->GetWidget()->RemoveMouseEventCallback(
      mouse_event_callback_);
}

}  // namespace safe_browsing
