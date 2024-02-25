// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_EXPERIMENT_EXPERIMENT_MANAGER_IMPL_H_
#define CHROME_BROWSER_TPCD_EXPERIMENT_EXPERIMENT_MANAGER_IMPL_H_

#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "chrome/browser/tpcd/experiment/experiment_manager.h"

class Profile;

namespace tpcd::experiment {

inline constexpr char kSyntheticTrialName[] = "ChromeTPCDExperiment";
inline constexpr char kSyntheticTrialInvalidGroupName[] = "invalid";

// Can only be used on the main thread.
class ExperimentManagerImpl : public ExperimentManager {
 public:
  static ExperimentManagerImpl* GetForProfile(Profile* profile);

  // The final decision is recorded in a local state pref. If this is called
  // after the final decision is made, the local state pref value takes
  // precedence and the callbacks are still performed.
  void SetClientEligibility(
      bool is_eligible,
      EligibilityDecisionCallback on_eligibility_decision_callback) override;

  std::optional<bool> IsClientEligible() const override;

  bool DidVersionChange() const override;

  void NotifyProfileTrackingProtectionOnboarded() override;

 protected:
  static ExperimentManagerImpl* GetInstance();

  ExperimentManagerImpl();
  ~ExperimentManagerImpl() override;

  // When both "disable_3p_cookies" and "need_onboarding_for_synthetic_trial"
  // feature params are true , the synthetic trial can be registered when the
  // client is either ineligible or onboarded. Otherwise, the synthetical trial
  // can be registered as long as the client eligibility is set.
  bool CanRegisterSyntheticTrial() const;

 private:
  friend base::NoDestructor<ExperimentManagerImpl>;

  bool did_version_change_ = false;

  bool client_is_eligible_ GUARDED_BY_CONTEXT(sequence_checker_) = true;
  std::vector<EligibilityDecisionCallback> callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);

  // Commit the computed eligibility to the local state pref, once the timer
  // expires. This function is called at most once per global runtime - never if
  // the local state pref already exists on startup.
  void CaptureEligibilityInLocalStatePref();
  // Register for the synthetic trial (or unregister using the "invalid" group).
  // Uses IsClientEligible() to determine eligibility, so the local state pref
  // must be set when this function is called.
  void MaybeUpdateSyntheticTrialRegistration();
};

}  // namespace tpcd::experiment

#endif  // CHROME_BROWSER_TPCD_EXPERIMENT_EXPERIMENT_MANAGER_IMPL_H_
