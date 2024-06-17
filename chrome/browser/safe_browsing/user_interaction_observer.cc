// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/user_interaction_observer.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/events/keycodes/keyboard_codes.h"

using blink::WebInputEvent;

namespace safe_browsing {

WEB_CONTENTS_USER_DATA_KEY_IMPL(SafeBrowsingUserInteractionObserver);

SafeBrowsingUserInteractionObserver::SafeBrowsingUserInteractionObserver(
    content::WebContents* web_contents,
    const security_interstitials::UnsafeResource& resource,
    scoped_refptr<SafeBrowsingUIManager> ui_manager)
    : content::WebContentsUserData<SafeBrowsingUserInteractionObserver>(
          *web_contents),
      content::WebContentsObserver(web_contents),
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
  // Pass a callback to the RenderWidgetHost instead of implementing
  // WebContentsObserver::DidGetUserInteraction(). The reason for this is that
  // RenderWidgetHost handles keyboard events earlier and the callback can
  // indicate that it wants the key press to be ignored.
  // (DidGetUserInteraction() can only observe and not cancel the event.)
  content::RenderWidgetHost* widget =
      web_contents->GetPrimaryMainFrame()->GetRenderWidgetHost();
  widget->AddKeyPressEventCallback(key_press_callback_);
  widget->AddMouseEventCallback(mouse_event_callback_);

  // Observe permission bubble events.
  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  if (permission_request_manager) {
    permission_request_manager->AddObserver(this);
  }
}

SafeBrowsingUserInteractionObserver::~SafeBrowsingUserInteractionObserver() {
  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  if (permission_request_manager) {
    permission_request_manager->RemoveObserver(this);
  }
  web_contents()
      ->GetPrimaryMainFrame()
      ->GetRenderWidgetHost()
      ->RemoveKeyPressEventCallback(key_press_callback_);
  web_contents()
      ->GetPrimaryMainFrame()
      ->GetRenderWidgetHost()
      ->RemoveMouseEventCallback(mouse_event_callback_);
}

// static
void SafeBrowsingUserInteractionObserver::CreateForWebContents(
    content::WebContents* web_contents,
    const security_interstitials::UnsafeResource& resource,
    scoped_refptr<SafeBrowsingUIManager> ui_manager) {
  // This method is called for all unsafe resources on |web_contents|. Only
  // create an observer if there isn't one.
  // TODO(crbug.com/40677238): The observer should observe all unsafe resources
  // instead of the first one only.
  content::WebContentsUserData<
      SafeBrowsingUserInteractionObserver>::CreateForWebContents(web_contents,
                                                                 resource,
                                                                 ui_manager);
}

void SafeBrowsingUserInteractionObserver::RenderFrameHostChanged(
    content::RenderFrameHost* old_frame,
    content::RenderFrameHost* new_frame) {
  // We currently only insert callbacks on the widget for the top-level main
  // frame.
  if (new_frame != web_contents()->GetPrimaryMainFrame())
    return;
  // The `old_frame` is null when the `new_frame` is the initial
  // RenderFrameHost, which we already attached to in the constructor.
  if (!old_frame)
    return;
  content::RenderWidgetHost* old_widget = old_frame->GetRenderWidgetHost();
  old_widget->RemoveKeyPressEventCallback(key_press_callback_);
  old_widget->RemoveMouseEventCallback(mouse_event_callback_);
  content::RenderWidgetHost* new_widget = new_frame->GetRenderWidgetHost();
  new_widget->AddKeyPressEventCallback(key_press_callback_);
  new_widget->AddMouseEventCallback(mouse_event_callback_);
}

void SafeBrowsingUserInteractionObserver::WebContentsDestroyed() {
  CleanUp();
  Detach();
}

void SafeBrowsingUserInteractionObserver::DidFinishNavigation(
    content::NavigationHandle* handle) {
  // Remove the observer on a top frame navigation to another page. The user is
  // now on another page so we don't need to wait for an interaction.
  // Note that the check for HasCommitted() occurs later in this method as we
  // want to handle downloads first.
  if (!handle->IsInPrimaryMainFrame() || handle->IsSameDocument()) {
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
    return;
  }
  // Now ignore other kinds of navigations that don't commit (e.g. 204 response
  // codes), since the page doesn't change.
  if (!handle->HasCommitted()) {
    return;
  }
  Detach();
  // DO NOT add code past this point. |this| is destroyed.
}

void SafeBrowsingUserInteractionObserver::Detach() {
  web_contents()->RemoveUserData(UserDataKey());
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

void SafeBrowsingUserInteractionObserver::OnPromptAdded() {
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
}

void SafeBrowsingUserInteractionObserver::OnDesktopCaptureRequest() {
  ShowInterstitial(DelayedWarningEvent::kWarningShownOnDesktopCaptureRequest);
  // DO NOT add code past this point. |this| is destroyed.
}

void SafeBrowsingUserInteractionObserver::SetClockForTesting(
    base::Clock* clock) {
  clock_ = clock;
}

base::Time SafeBrowsingUserInteractionObserver::GetCreationTimeForTesting()
    const {
  return creation_time_;
}

bool IsAllowedModifier(const input::NativeWebKeyboardEvent& event) {
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
    const input::NativeWebKeyboardEvent& event) {
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
  ui_manager_->StartDisplayingBlockingPage(resource_);
  Detach();
  // DO NOT add code past this point. |this| is destroyed.
}

void SafeBrowsingUserInteractionObserver::CleanUp() {
  content::RenderWidgetHost* widget =
      web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost();
  widget->RemoveKeyPressEventCallback(key_press_callback_);
  widget->RemoveMouseEventCallback(mouse_event_callback_);
}

}  // namespace safe_browsing
