// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_CONTROLLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_CONTROLLER_H_

#include "components/contextual_tasks/public/contextual_tasks_service.h"

namespace contextual_tasks {

// Represents the eligibility status for contextual tasks features.
// This is used to determine if any backend is available and if the feature
// is enabled.
struct FeatureEligibility {
  // Whether the contextual tasks feature flag is enabled.
  bool contextual_tasks_enabled;
  // Whether the AIM backend is eligible for use.
  bool aim_eligible;

  bool IsEligible() { return contextual_tasks_enabled && aim_eligible; }
};

class ContextualTasksContextController
    : public contextual_tasks::ContextualTasksService {
 public:
  ~ContextualTasksContextController() override;

  // Returns whether there are any available backends that are eligible for use.
  virtual FeatureEligibility GetFeatureEligibility() = 0;

 protected:
  ContextualTasksContextController();
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_CONTROLLER_H_
