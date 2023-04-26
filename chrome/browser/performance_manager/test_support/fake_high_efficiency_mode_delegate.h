// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_HIGH_EFFICIENCY_MODE_DELEGATE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_HIGH_EFFICIENCY_MODE_DELEGATE_H_

#include "base/time/time.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"

namespace performance_manager {

class FakeHighEfficiencyModeDelegate
    : public performance_manager::user_tuning::UserPerformanceTuningManager::
          HighEfficiencyModeDelegate {
 public:
  // overrides of methods in HighEfficiencyModeDelegate
  void ToggleHighEfficiencyMode(bool enabled) override;
  void SetTimeBeforeDiscard(base::TimeDelta time_before_discard) override;
  ~FakeHighEfficiencyModeDelegate() override = default;

  base::TimeDelta GetLastTimeBeforeDiscard();

 private:
  base::TimeDelta last_time_before_discard = base::TimeDelta::Max();
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_HIGH_EFFICIENCY_MODE_DELEGATE_H_
