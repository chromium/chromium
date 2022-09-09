// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_FRAME_THROTTLING_DELEGATE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_FRAME_THROTTLING_DELEGATE_H_

#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"

namespace performance_manager {

class FakeFrameThrottlingDelegate
    : public performance_manager::user_tuning::UserPerformanceTuningManager::
          FrameThrottlingDelegate {
 public:
  void StartThrottlingAllFrameSinks() override;
  void StopThrottlingAllFrameSinks() override;

  explicit FakeFrameThrottlingDelegate(bool* throttling_enabled);
  ~FakeFrameThrottlingDelegate() override = default;

  bool* throttling_enabled_;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_FAKE_FRAME_THROTTLING_DELEGATE_H_
