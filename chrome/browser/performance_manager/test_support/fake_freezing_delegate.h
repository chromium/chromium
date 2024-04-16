// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_FREEZING_DELEGATE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_FREEZING_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"

namespace performance_manager {

class FakeFreezingDelegate
    : public performance_manager::user_tuning::BatterySaverModeManager::
          FreezingDelegate {
 public:
   void ToggleFreezingOnBatterySaverMode(bool is_enabled) override;

  explicit FakeFreezingDelegate(bool* freezing_on_battery_saver_enabled);
  ~FakeFreezingDelegate() override = default;

  raw_ptr<bool> freezing_on_battery_saver_enabled_;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_FREEZING_DELEGATE_H_
