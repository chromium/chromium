// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REPUTATION_REPUTATION_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_REPUTATION_REPUTATION_WEB_CONTENTS_OBSERVER_H_

#include "base/callback_forward.h"
#include "chrome/browser/reputation/reputation_service.h"
#include "chrome/browser/reputation/safety_tip_ui.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

class Profile;

// Observes navigations and triggers a warning if a visited site is determined
// to be low-reputation as determined by heuristics or inclusion on
// pre-calculated lists.
class ReputationWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ReputationWebContentsObserver> {
 public:
  ~ReputationWebContentsObserver() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // Returns the info about the Safety Tip (if any) that was assigned to the
  // currently visible navigation entry. This field will be set even if the UI
  // was not actually shown because the feature was disabled.
  security_state::SafetyTipInfo GetSafetyTipInfoForVisibleNavigation() const;

  // Allows tests to register a callback to be called when the next reputation
  // check finishes.
  void RegisterReputationCheckCallbackForTesting(base::OnceClosure callback);

 private:
  friend class content::WebContentsUserData<ReputationWebContentsObserver>;

  explicit ReputationWebContentsObserver(content::WebContents* web_contents);

  // Possibly show a Safety Tip. Called on visibility changes and page load.
  void MaybeShowSafetyTip();

  // A ReputationCheckCallback. Called by the reputation service when a
  // reputation result is available.
  void HandleReputationCheckResult(ReputationCheckResult result);

  // A helper method that calls and resets
  // |reputation_check_callback_for_testing_| if it is set.
  void MaybeCallReputationCheckCallback();

  Profile* profile_;
  // Used to cache the last safety tip info (and associated navigation entry ID)
  // so that Page Info can fetch this information without performing a
  // reputation check. Resets type to kNone and safe_url to empty on new top
  // frame navigations. Set even if the feature to show the UI is disabled.
  security_state::SafetyTipInfo last_navigation_safety_tip_info_;

  int last_safety_tip_navigation_entry_id_ = 0;

  base::OnceClosure reputation_check_callback_for_testing_;

  base::WeakPtrFactory<ReputationWebContentsObserver> weak_factory_{this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_REPUTATION_REPUTATION_WEB_CONTENTS_OBSERVER_H_
