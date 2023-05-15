// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_HIGH_EFFICIENCY_MODE_DELEGATE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_HIGH_EFFICIENCY_MODE_DELEGATE_H_

#include "base/time/time.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager::user_tuning {

class FakeHighEfficiencyModeDelegate
    : public UserPerformanceTuningManager::HighEfficiencyModeDelegate {
 public:
  // overrides of methods in HighEfficiencyModeDelegate
  void ToggleHighEfficiencyMode(prefs::HighEfficiencyModeState state) override;
  void SetTimeBeforeDiscard(base::TimeDelta time_before_discard) override;
  ~FakeHighEfficiencyModeDelegate() override = default;

  void ClearLastState();

  absl::optional<prefs::HighEfficiencyModeState> GetLastState() const;

  absl::optional<base::TimeDelta> GetLastTimeBeforeDiscard() const;

 private:
  absl::optional<base::TimeDelta> last_time_before_discard_;
  absl::optional<prefs::HighEfficiencyModeState> last_state_;
};

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_HIGH_EFFICIENCY_MODE_DELEGATE_H_
