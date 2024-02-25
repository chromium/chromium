// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_EXPERIMENT_MOCK_EXPERIMENT_MANAGER_H_
#define CHROME_BROWSER_TPCD_EXPERIMENT_MOCK_EXPERIMENT_MANAGER_H_

#include <optional>

#include "base/functional/callback.h"
#include "chrome/browser/tpcd/experiment/experiment_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace tpcd::experiment {

class MockExperimentManager : public ExperimentManager {
 public:
  MockExperimentManager();
  ~MockExperimentManager() override;

  MOCK_METHOD(void,
              SetClientEligibility,
              (bool, EligibilityDecisionCallback),
              (override));
  MOCK_METHOD(std::optional<bool>, IsClientEligible, (), (const, override));
  MOCK_METHOD(bool, DidVersionChange, (), (const, override));
  MOCK_METHOD(void, NotifyProfileTrackingProtectionOnboarded, (), (override));
};

}  // namespace tpcd::experiment

#endif  // CHROME_BROWSER_TPCD_EXPERIMENT_MOCK_EXPERIMENT_MANAGER_H_
