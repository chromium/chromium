// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_EXPERIMENT_EXPERIMENT_MANAGER_H_
#define CHROME_BROWSER_TPCD_EXPERIMENT_EXPERIMENT_MANAGER_H_

#include <optional>

#include "base/functional/callback_forward.h"

namespace tpcd::experiment {

class ExperimentManager {
 public:
  using EligibilityDecisionCallback = base::OnceCallback<void(bool)>;

  ExperimentManager() = default;
  virtual ~ExperimentManager() = default;

  // Called by `EligibilityService` to tell the manager whether a profile is
  // eligible, with a callback to complete the profile-level work required once
  // the final decision is made.
  virtual void SetClientEligibility(
      bool is_eligible,
      EligibilityDecisionCallback on_eligibility_decision_callback) = 0;

  // Returns the final decision for client eligibility, if completed.
  // `std::nullopt` will be returned if the final decision has not been made
  // yet.
  virtual std::optional<bool> IsClientEligible() const = 0;

  // Returns whether the experiment version has changed.
  virtual bool DidVersionChange() const = 0;

  // Notifies the manager that a profile has onboarded tracking protection.
  virtual void NotifyProfileTrackingProtectionOnboarded() = 0;
};

}  // namespace tpcd::experiment

#endif  // CHROME_BROWSER_TPCD_EXPERIMENT_EXPERIMENT_MANAGER_H_
