// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REPUTATION_REPUTATION_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_REPUTATION_REPUTATION_WEB_CONTENTS_OBSERVER_H_

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/reputation/reputation_service.h"
#include "chrome/browser/reputation/safety_tip_ui.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/reputation/safety_tip_message_delegate_android.h"
#endif

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

  // Allows tests to register a callback called when the warning closes.
  void RegisterSafetyTipCloseCallbackForTesting(base::OnceClosure callback);

  // Allows tests to see whether a reputation check has already completed since
  // construction or last reset, and selectively register a callback if not.
  bool reputation_check_pending_for_testing() {
    return reputation_check_pending_for_testing_;
  }

  void reset_reputation_check_pending_for_testing() {
    reputation_check_pending_for_testing_ = true;
  }

 private:
  friend class content::WebContentsUserData<ReputationWebContentsObserver>;

  explicit ReputationWebContentsObserver(content::WebContents* web_contents);

  // Possibly show a Safety Tip. Called on visibility changes and page load.
  void MaybeShowSafetyTip(ukm::SourceId navigation_source_id,
                          bool called_from_visibility_check,
                          bool record_ukm_if_tip_not_shown);

  // A ReputationCheckCallback. Called by the reputation service when a
  // reputation result is available.
  void HandleReputationCheckResult(ukm::SourceId navigation_source_id,
                                   bool called_from_visibility_check,
                                   bool record_ukm_if_tip_not_shown,
                                   ReputationCheckResult result);

  // A helper method that calls and resets
  // |reputation_check_callback_for_testing_| if it is set. Only flips
  // |reputation_check_pending_for_testing_| if |heuristics_checked| is set.
  void MaybeCallReputationCheckCallback(bool heuristics_checked);

  // A helper method to handle finalizing a reputation check. This method
  // records UKM data about triggered heuristics if |record_ukm| is true, and
  // calls MaybeCallReputationCheckCallback.
  void FinalizeReputationCheckWhenTipNotShown(
      bool record_ukm,
      ReputationCheckResult result,
      ukm::SourceId navigation_source_id);

  raw_ptr<Profile> profile_;

  // Used to cache the last safety tip info (and associated navigation entry ID)
  // so that Page Info can fetch this information without performing a
  // reputation check. Resets type to kNone and safe_url to empty on new top
  // frame navigations. Set even if the feature to show the UI is disabled.
  security_state::SafetyTipInfo last_navigation_safety_tip_info_;
  int last_safety_tip_navigation_entry_id_ = 0;

  // The initiator origin and URL of the most recently committed navigation.
  // Presently, these are used in metrics to differentiate same-origin
  // navigations (i.e. when the user stays on a flagged page).
  absl::optional<url::Origin> last_committed_initiator_origin_;
  GURL last_committed_url_;

  base::OnceClosure reputation_check_callback_for_testing_;
  // Whether or not heuristics have yet been checked yet.
  bool reputation_check_pending_for_testing_;

  base::OnceClosure safety_tip_close_callback_for_testing_;

#if BUILDFLAG(IS_ANDROID)
  SafetyTipMessageDelegateAndroid delegate_;
#endif

  base::WeakPtrFactory<ReputationWebContentsObserver> weak_factory_{this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_REPUTATION_REPUTATION_WEB_CONTENTS_OBSERVER_H_
