// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_HIGH_EFFICIENCY_MODE_TOGGLE_DELEGATE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_HIGH_EFFICIENCY_MODE_TOGGLE_DELEGATE_H_

#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"

namespace performance_manager {

class FakeHighEfficiencyModeToggleDelegate
    : public performance_manager::user_tuning::UserPerformanceTuningManager::
          HighEfficiencyModeToggleDelegate {
 public:
  void ToggleHighEfficiencyMode(bool enabled) override;
  ~FakeHighEfficiencyModeToggleDelegate() override = default;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_HIGH_EFFICIENCY_MODE_TOGGLE_DELEGATE_H_
