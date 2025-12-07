// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_SYNTHETIC_TRIAL_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_SYNTHETIC_TRIAL_MANAGER_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/variations/synthetic_trial_registry.h"

namespace glic {

class GlicSyntheticTrialManager {
 public:
  GlicSyntheticTrialManager();
  ~GlicSyntheticTrialManager();
  GlicSyntheticTrialManager(const GlicSyntheticTrialManager&) = delete;
  GlicSyntheticTrialManager& operator=(const GlicSyntheticTrialManager&) =
      delete;

  // Used by the web client to enroll Chrome in the specified synthetic trial
  // group. If a conflicting group is already registered by another profile,
  // then we instead register into a new group called `MultiProfileDetected` to
  // indicate the log file is corrupted. This is cleared when the user restarts
  // the browser.
  void SetSyntheticExperimentState(const std::string& trial_name,
                                   const std::string& group_name);

 private:
  std::map<std::string, std::string> synthetic_field_trial_groups_;
  std::map<std::string, std::string> staged_synthetic_field_trial_groups_;

  base::WeakPtrFactory<GlicSyntheticTrialManager> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_SYNTHETIC_TRIAL_MANAGER_H_
