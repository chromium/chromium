// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SUSPICIOUS_SITE_WARNINGS_SUSPICIOUS_SITE_CONTROLLER_DESKTOP_H_
#define CHROME_BROWSER_SAFE_BROWSING_SUSPICIOUS_SITE_WARNINGS_SUSPICIOUS_SITE_CONTROLLER_DESKTOP_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/safe_browsing/content/browser/async_check_tracker.h"
#include "components/safe_browsing/core/browser/suspicious_site_warning_allowlist.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace safe_browsing {

// A tab helper controller responsible for displaying a suspicious site warning
// bubble on Desktop for a specific navigation ID. Observes WebContents and
// AsyncCheckTracker to defer displaying the warning until Safe Browsing checks
// complete and the navigation commits, while prioritizing full-page
// interstitials.
class SuspiciousSiteControllerDesktop
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SuspiciousSiteControllerDesktop>,
      public AsyncCheckTracker::Observer {
 public:
  using WarningOutcome = safe_browsing::SuspiciousSiteWarningOutcome;
  using UserInteraction = safe_browsing::SuspiciousSiteWarningUserInteraction;

  SuspiciousSiteControllerDesktop(const SuspiciousSiteControllerDesktop&) =
      delete;
  SuspiciousSiteControllerDesktop& operator=(
      const SuspiciousSiteControllerDesktop&) = delete;

  ~SuspiciousSiteControllerDesktop() override;

  // Displays or stages the suspicious site warning bubble for the given
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

  // Button and link event handlers from the bubble UI:
  void OnBackToSafetyClicked();
  void OnMarkAsSafeClicked();
  void OnLearnMoreClicked();
  void OnBubbleDestroyed();

  // Shows the bubble view on the window.
  void ShowBubble();

  // Testing helpers:
  static void SetBubbleShownCallbackForTesting(base::OnceClosure callback);
  static void SetBubbleDestroyedCallbackForTesting(base::OnceClosure callback);

 private:
  friend class content::WebContentsUserData<SuspiciousSiteControllerDesktop>;
  friend class SuspiciousSiteControllerDesktopTest;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  explicit SuspiciousSiteControllerDesktop(content::WebContents* web_contents);

  // Validates all preconditions and triggers ShowBubble if all gates pass.
  void MaybeShowBubble();

  // Navigation ID of the main frame navigation that triggered the suspicious
  // site warning.
  std::optional<int64_t> navigation_id_;

  // Tracks whether the target navigation has committed.
  bool navigation_committed_ = false;

  // Whether the bubble display is currently suspended or waiting for checks to
  // finish.
  bool is_suspended_ = false;

  // Tracks whether the bubble has been shown on screen.
  bool has_shown_ = false;

  // Tracks the warning outcome decision as the user interacts with the warning.
  WarningOutcome warning_outcome_ = WarningOutcome::kUnknown;

  // Tracks if this controller is observing AsyncCheckTracker.
  base::ScopedObservation<AsyncCheckTracker, AsyncCheckTracker::Observer>
      async_check_observation_{this};

  // The suspicious URL currently added to the allowlist for this warning.
  GURL current_suspicious_url_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SUSPICIOUS_SITE_WARNINGS_SUSPICIOUS_SITE_CONTROLLER_DESKTOP_H_
