// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_SAFETY_TIP_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_LOOKALIKES_SAFETY_TIP_WEB_CONTENTS_OBSERVER_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/lookalikes/safety_tip_ui.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/lookalikes/safety_tip_message_delegate_android.h"
#endif

class Profile;

// Observes navigations and triggers a Safety Tip warning if a visited site is
// determined to be a lookalike.
class SafetyTipWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SafetyTipWebContentsObserver> {
 public:
  ~SafetyTipWebContentsObserver() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // Returns the info about the Safety Tip (if any) that was assigned to the
  // currently visible navigation entry. This field will be set even if the UI
  // was not actually shown because the feature was disabled.
  security_state::SafetyTipInfo GetSafetyTipInfoForVisibleNavigation() const;

  // Allows tests to register a callback to be called when the next safety tip
  // check finishes.
  void RegisterSafetyTipCheckCallbackForTesting(base::OnceClosure callback);

  // Allows tests to register a callback called when the warning closes.
  void RegisterSafetyTipCloseCallbackForTesting(base::OnceClosure callback);

  // Allows tests to see whether a safety tip check has already completed since
  // construction or last reset, and selectively register a callback if not.
  bool safety_tip_check_pending_for_testing() {
    return safety_tip_check_pending_for_testing_;
  }

  void reset_safety_tip_check_pending_for_testing() {
    safety_tip_check_pending_for_testing_ = true;
  }

 private:
  friend class content::WebContentsUserData<SafetyTipWebContentsObserver>;

  explicit SafetyTipWebContentsObserver(content::WebContents* web_contents);

  // Possibly show a Safety Tip. Called on visibility changes and page load.
  void MaybeShowSafetyTip(ukm::SourceId navigation_source_id,
                          bool called_from_visibility_check,
                          bool record_ukm_if_tip_not_shown);

  // A SafetyTipCheckCallback. Called by the safety tip service when a
  // safety tip result is available.
  void HandleSafetyTipCheckResult(ukm::SourceId navigation_source_id,
                                  bool called_from_visibility_check,
                                  bool record_ukm_if_tip_not_shown,
                                  SafetyTipCheckResult result);

  // A helper method that calls and resets
  // |safety_tip_check_callback_for_testing_| if it is set. Only flips
  // |safety_tip_check_pending_for_testing_| if |heuristics_checked| is set.
  void MaybeCallSafetyTipCheckCallback(bool heuristics_checked);

  // A helper method to handle finalizing a safety tip check. This method
  // records UKM data about triggered heuristics if |record_ukm| is true, and
  // calls MaybeCallSafetyTipCheckCallback.
  void FinalizeSafetyTipCheckWhenTipNotShown(
      bool record_ukm,
      SafetyTipCheckResult result,
      ukm::SourceId navigation_source_id);

  raw_ptr<Profile> profile_;

  // Used to cache the last safety tip info (and associated navigation entry ID)
  // so that Page Info can fetch this information without performing a
  // safety tip check. Resets type to kNone and safe_url to empty on new top
  // frame navigations. Set even if the feature to show the UI is disabled.
  security_state::SafetyTipInfo last_navigation_safety_tip_info_;
  int last_safety_tip_navigation_entry_id_ = 0;

  // The initiator origin and URL of the most recently committed navigation.
  // Presently, these are used in metrics to differentiate same-origin
  // navigations (i.e. when the user stays on a flagged page).
  std::optional<url::Origin> last_committed_initiator_origin_;
  GURL last_committed_url_;

  base::OnceClosure safety_tip_check_callback_for_testing_;
  // Whether or not heuristics have yet been checked yet.
  bool safety_tip_check_pending_for_testing_ = true;

  base::OnceClosure safety_tip_close_callback_for_testing_;

#if BUILDFLAG(IS_ANDROID)
  SafetyTipMessageDelegateAndroid delegate_;
#endif

  base::WeakPtrFactory<SafetyTipWebContentsObserver> weak_factory_{this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_LOOKALIKES_SAFETY_TIP_WEB_CONTENTS_OBSERVER_H_
