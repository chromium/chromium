// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_EXPERIMENT_EXPERIMENT_MANAGER_H_
#define CHROME_BROWSER_TPCD_EXPERIMENT_EXPERIMENT_MANAGER_H_

namespace tpcd::experiment {

class ExperimentManager {
 public:
  ExperimentManager() = default;

  // Called by EligibilityService to tell the manager whether a profile is
  // eligible. Mode B also needs to know whether the profile has been onboarded
  // to the 3PCD UX, but Mode A does not.
  void SetClientEligibility(bool is_eligible, bool is_onboarded = false);

 private:
  // Mode B experiment will need to register the client for the synthetic trial
  // IFF the client is eligible and onboarded (TCPDExperimentState ==
  // kOnboardedEligible). Register the client for the correct arm of the
  // synthetic trial based on Finch feature params.
  void RegisterSyntheticTrial();

  // If a previously registered client becomes ineligible, unregister it from
  // the synthetic trial by registering it to a "invalidated" experiment arm.
  void UnregisterSyntheticTrial();

  // Check the client is eligible for 3PCD Experiments. Returns false when: the
  // client is <30 days old or Android users with a PWA installed.
  bool isClientEligible();
};

}  // namespace tpcd::experiment

#endif  // CHROME_BROWSER_TPCD_EXPERIMENT_EXPERIMENT_MANAGER_H_
