// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_TETHER_FAKE_CRASH_RECOVERY_MANAGER_H_
#define ASH_COMPONENTS_TETHER_FAKE_CRASH_RECOVERY_MANAGER_H_

#include "ash/components/tether/crash_recovery_manager.h"
#include "base/callback.h"

namespace ash {

namespace tether {

// Test double for CrashRecoveryManager.
class FakeCrashRecoveryManager : public CrashRecoveryManager {
 public:
  FakeCrashRecoveryManager();

  FakeCrashRecoveryManager(const FakeCrashRecoveryManager&) = delete;
  FakeCrashRecoveryManager& operator=(const FakeCrashRecoveryManager&) = delete;

  ~FakeCrashRecoveryManager() override;

  base::OnceClosure TakeOnRestorationFinishedCallback() {
    return std::move(on_restoration_finished_callback_);
  }

  // CrashRecoveryManager:
  void RestorePreCrashStateIfNecessary(
      base::OnceClosure on_restoration_finished) override;

 private:
  base::OnceClosure on_restoration_finished_callback_;
};

}  // namespace tether

}  // namespace ash

#endif  // ASH_COMPONENTS_TETHER_FAKE_CRASH_RECOVERY_MANAGER_H_
