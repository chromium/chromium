// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_CHILD_PROCESS_TUNING_DELEGATE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_CHILD_PROCESS_TUNING_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"

namespace performance_manager {

class FakeChildProcessTuningDelegate
    : public user_tuning::BatterySaverModeManager::ChildProcessTuningDelegate {
 public:
  explicit FakeChildProcessTuningDelegate(bool* child_process_tuning_enabled);
  ~FakeChildProcessTuningDelegate() override;

  void SetBatterySaverModeForAllChildProcessHosts(bool enabled) override;

  raw_ptr<bool> child_process_tuning_enabled_;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_CHILD_PROCESS_TUNING_DELEGATE_H_
