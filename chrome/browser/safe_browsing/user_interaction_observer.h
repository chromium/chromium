// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_USER_INTERACTION_OBSERVER_H_
#define CHROME_BROWSER_SAFE_BROWSING_USER_INTERACTION_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/permissions/permission_request_manager.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace blink {
class WebMouseEvent;
}

namespace safe_browsing {

// Used for UMA. There may be more than one event per navigation (e.g.
// kAll and kWarningShownOnKeypress).
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DelayedWarningEvent {
  // User loaded a page with a delayed warning.
  kPageLoaded = 0,
  // User left the page and the warning was never shown.
  kWarningNotShown = 1,
  // User pressed a key and the warning was shown.
  kWarningShownOnKeypress = 2,
  // User clicked on the page at least once but the feature isn't configured to
  // show warnings on mouse clicks.
  kWarningNotTriggeredOnMouseClick = 3,
  // User clicked on the page and the warning was shown.
  kWarningShownOnMouseClick = 4,
  // The page tried to enter fullscreen mode.
  kWarningShownOnFullscreenAttempt = 5,
  // The page tried to initiate a download and we cancelled it. This doesn't
  // show an interstitial.
  kDownloadCancelled = 6,
  // The page triggered a permission request. It was denied and the warning was
  // shown.
  kWarningShownOnPermissionRequest = 7,
  // The page tried to display a JavaScript dialog (alert/confirm/prompt). It
  // was blocked and the warning was shown.
  kWarningShownOnJavaScriptDialog = 8,
  // The page was denied a password save or autofill request. This doesn't show
  // an interstitial and is recorded once per navigation.
  kPasswordSaveOrAutofillDenied = 9,
  // The page triggered a desktop capture request ("example.com wants to share
  // the contents of the screen"). It was denied and the warning was shown.
  kWarningShownOnDesktopCaptureRequest = 10,
  // User pasted something on the page and the warning was shown.
  kWarningShownOnPaste = 11,
  kMaxValue = kWarningShownOnPaste,
};

// Observes user interactions and shows an interstitial if necessary.
// Only created when an interstitial was about to be displayed but was delayed
// due to the Delayed Warnings experiment. Deleted once the interstitial is
// shown, or the tab is closed or navigated away.
class SafeBrowsingUserInteractionObserver
    : public content::WebContentsUserData<SafeBrowsingUserInteractionObserver>,
      public content::WebContentsObserver,
      public permissions::PermissionRequestManager::Observer {
 public:
  // Creates an observer for given |web_contents|. |resource| is the unsafe
  // resource for which a delayed interstitial will be displayed.
  // |ui_manager| is the UIManager that shows the actual warning.
  static void CreateForWebContents(
      content::WebContents* web_contents,
      const security_interstitials::UnsafeResource& resource,
      scoped_refptr<SafeBrowsingUIManager> ui_manager);

  ~SafeBrowsingUserInteractionObserver() override;

  // content::WebContentsObserver methods:
  void RenderFrameHostChanged(content::RenderFrameHost* old_frame,
                              content::RenderFrameHost* new_frame) override;
  void WebContentsDestroyed() override;
  void DidFinishNavigation(content::NavigationHandle* handle) override;
  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override;
  void OnPaste() override;

  // permissions::PermissionRequestManager::Observer methods:
  void OnPromptAdded() override;

  // Called by the JavaScript dialog manager when the current page is about to
  // show a JavaScript dialog (alert, confirm or prompt). Shows the
  // delayed interstitial immediately.
  void OnJavaScriptDialog();
  // Called when a password save or autofill request is denied to the current
  // page. Records a metric once per navigation.
  void OnPasswordSaveOrAutofillDenied();
  // Called by the desktop capture access handler when the current page requests
  // a desktop capture. Shows the delayed interstitial immediately.
  void OnDesktopCaptureRequest();

  void SetClockForTesting(base::Clock* clock);
  base::Time GetCreationTimeForTesting() const;

 private:
  friend class content::WebContentsUserData<
      SafeBrowsingUserInteractionObserver>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  // See CreateForWebContents() for parameters.
  SafeBrowsingUserInteractionObserver(
      content::WebContents* web_contents,
      const security_interstitials::UnsafeResource& resource,
      scoped_refptr<SafeBrowsingUIManager> ui_manager);

  bool HandleKeyPress(const input::NativeWebKeyboardEvent& event);
  bool HandleMouseEvent(const blink::WebMouseEvent& event);

  void ShowInterstitial(DelayedWarningEvent event);
  void CleanUp();
  void Detach();

  content::RenderWidgetHost::KeyPressEventCallback key_press_callback_;
  content::RenderWidgetHost::MouseEventCallback mouse_event_callback_;

  security_interstitials::UnsafeResource resource_;
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  bool interstitial_shown_ = false;
  bool mouse_click_with_no_warning_recorded_ = false;
  bool password_save_or_autofill_denied_metric_recorded_ = false;
  // This will be set to true if the initial navigation that caused this
  // observer to be created has finished. We need this extra bit because
  // observers can only detect download navigations in DidFinishNavigation.
  // However, this hook is also called for the initial navigation, so we ignore
  // it the first time the hook is called.
  bool initial_navigation_finished_ = false;

  // The time that this observer was created. Used for recording histograms.
  base::Time creation_time_;
  // This clock is used to record the delta from |creation_time_| when the
  // observer is detached, and can be injected by tests.
  raw_ptr<base::Clock> clock_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_USER_INTERACTION_OBSERVER_H_
