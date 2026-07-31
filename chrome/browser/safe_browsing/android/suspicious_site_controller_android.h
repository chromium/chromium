// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ANDROID_SUSPICIOUS_SITE_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_SAFE_BROWSING_ANDROID_SUSPICIOUS_SITE_CONTROLLER_ANDROID_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/safe_browsing/content/browser/async_check_tracker.h"
#include "components/safe_browsing/core/browser/suspicious_site_warning_allowlist.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/android/modal_dialog_wrapper.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace safe_browsing {

class SuspiciousSiteDialogViewAndroid;

// A tab helper controller responsible for displaying a suspicious site warning
// dialog on Android for a specific navigation ID. Observes WebContents and
// AsyncCheckTracker to defer displaying the warning until Safe Browsing checks
// complete and the navigation commits, while prioritizing full-page
// interstitials.
class SuspiciousSiteControllerAndroid
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SuspiciousSiteControllerAndroid>,
      public AsyncCheckTracker::Observer {
 public:
  using WarningOutcome = safe_browsing::SuspiciousSiteWarningOutcome;

  SuspiciousSiteControllerAndroid(const SuspiciousSiteControllerAndroid&) =
      delete;
  SuspiciousSiteControllerAndroid& operator=(
      const SuspiciousSiteControllerAndroid&) = delete;

  ~SuspiciousSiteControllerAndroid() override;

  // Displays or stages the suspicious site warning dialog for the given
  // navigation.
  static void ShowForWebContents(content::WebContents* web_contents,
                                 int64_t navigation_id);

  // WebContentsObserver overrides:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // AsyncCheckTracker::Observer overrides:
  void OnAsyncSafeBrowsingCheckCompleted() override;
  void OnAsyncSafeBrowsingCheckTrackerDestructed() override;

  // Called by the dialog view when the dialog is dismissed.
  void CloseDialog(ui::ModalDialogWrapper::DismissalCause dismissal_cause);

  // Button and link event handlers from the dialog UI:
  void OnGoBackButtonClicked();
  void OnContinueButtonClicked();
  void OnHelpCenterLinkClicked();

  // Text getters for the dialog UI:
  std::u16string GetPrimaryButtonText() const;
  std::u16string GetSecondaryButtonText() const;
  std::u16string GetTitle() const;
  std::u16string GetWarningDetailText() const;

  // Creates and renders the dialog view on the window.
  void ShowDialog();

  // Testing helpers:
  static void SetDialogShownCallbackForTesting(base::OnceClosure callback);
  static void SetDialogDismissedCallbackForTesting(base::OnceClosure callback);

 private:
  friend class content::WebContentsUserData<SuspiciousSiteControllerAndroid>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  explicit SuspiciousSiteControllerAndroid(content::WebContents* web_contents);

  // Validates all preconditions and triggers ShowDialog if all gates pass.
  void MaybeShowDialog();

  // Navigation ID of the main frame navigation that triggered the suspicious
  // site warning. This is std::nullopt when the controller is first created,
  // before ShowForWebContents() is called for a specific navigation.
  std::optional<int64_t> navigation_id_;

  // Tracks whether the target navigation has committed.
  bool navigation_committed_ = false;

  // Whether the dialog display is currently suspended or waiting for checks to
  // finish.
  bool is_suspended_ = false;

  // Tracks whether the dialog has been shown on screen.
  bool has_shown_ = false;

  // Tracks the warning outcome decision as the user interacts with the warning.
  WarningOutcome warning_outcome_ = WarningOutcome::kUnknown;

  // Tracks if this controller is observing AsyncCheckTracker.
  bool is_observing_async_check_tracker_ = false;

  // The suspicious URL currently added to the allowlist for this warning.
  GURL current_suspicious_url_;

  // View bridge for the Android modal dialog.
  std::unique_ptr<SuspiciousSiteDialogViewAndroid> dialog_view_;

  base::WeakPtrFactory<SuspiciousSiteControllerAndroid> weak_ptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_ANDROID_SUSPICIOUS_SITE_CONTROLLER_ANDROID_H_
