// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_EXPERIMENT_EXPERIMENT_MANAGER_IMPL_H_
#define CHROME_BROWSER_TPCD_EXPERIMENT_EXPERIMENT_MANAGER_IMPL_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "chrome/browser/tpcd/experiment/experiment_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  absl::optional<bool> IsClientEligible() const override;

 protected:
  static ExperimentManagerImpl* GetInstance();

  ExperimentManagerImpl();
  ~ExperimentManagerImpl() override;

 private:
  friend base::NoDestructor<ExperimentManagerImpl>;

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
  void UpdateSyntheticTrialRegistration();
};

}  // namespace tpcd::experiment

#endif  // CHROME_BROWSER_TPCD_EXPERIMENT_EXPERIMENT_MANAGER_IMPL_H_
