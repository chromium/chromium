// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_FRAME_THROTTLING_DELEGATE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_FRAME_THROTTLING_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"

namespace performance_manager {

class FakeFrameThrottlingDelegate
    : public performance_manager::user_tuning::BatterySaverModeManager::
          FrameThrottlingDelegate {
 public:
  void StartThrottlingAllFrameSinks() override;
  void StopThrottlingAllFrameSinks() override;

  explicit FakeFrameThrottlingDelegate(bool* throttling_enabled);
  ~FakeFrameThrottlingDelegate() override = default;

  raw_ptr<bool> throttling_enabled_;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_FRAME_THROTTLING_DELEGATE_H_
