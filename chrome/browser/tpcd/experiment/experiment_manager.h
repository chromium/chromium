// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_EXPERIMENT_EXPERIMENT_MANAGER_H_
#define CHROME_BROWSER_TPCD_EXPERIMENT_EXPERIMENT_MANAGER_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace tpcd::experiment {

// Can only be used on the main thread.
class ExperimentManager {
 public:
  using EligibilityDecisionCallback = base::OnceCallback<void(bool)>;

  static ExperimentManager* GetInstance();

  // Called by `EligibilityService` to tell the manager whether a profile is
  // eligible, with a callback to complete the profile-level work required once
  // the final decision is made. The final decision is recorded in a local state
  // pref. If this is called after the final decision is made, the local state
  // pref value takes precedence and the callbacks are still performed.
  void SetClientEligibility(
      bool is_eligible,
      EligibilityDecisionCallback on_eligibility_decision_callback);

  // Returns the final decision for client eligibility, if completed.
  // `absl::nullopt` will be returned if the final decision has not been made
  // yet.
  absl::optional<bool> IsClientEligible() const;

 protected:
  ExperimentManager();
  ~ExperimentManager();

 private:
  friend base::NoDestructor<ExperimentManager>;
  bool client_is_eligible_ GUARDED_BY_CONTEXT(sequence_checker_) = true;
  std::vector<EligibilityDecisionCallback> callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);

  void CaptureEligibilityInLocalStatePref();
};

}  // namespace tpcd::experiment

#endif  // CHROME_BROWSER_TPCD_EXPERIMENT_EXPERIMENT_MANAGER_H_
