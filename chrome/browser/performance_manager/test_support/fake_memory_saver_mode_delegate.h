// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_MEMORY_SAVER_MODE_DELEGATE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_MEMORY_SAVER_MODE_DELEGATE_H_

#include <optional>

#include "base/time/time.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "components/performance_manager/public/user_tuning/prefs.h"

namespace performance_manager::user_tuning {

class FakeMemorySaverModeDelegate
    : public UserPerformanceTuningManager::MemorySaverModeDelegate {
 public:
  // overrides of methods in MemorySaverModeDelegate
  void ToggleMemorySaverMode(prefs::MemorySaverModeState state) override;
  void SetMode(prefs::MemorySaverModeAggressiveness mode) override;
  ~FakeMemorySaverModeDelegate() override = default;

  void ClearLastState();

  std::optional<prefs::MemorySaverModeState> GetLastState() const;

 private:
  std::optional<prefs::MemorySaverModeState> last_state_;
  std::optional<prefs::MemorySaverModeAggressiveness> mode_;
};

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_MEMORY_SAVER_MODE_DELEGATE_H_
