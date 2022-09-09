// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/test_support/test_user_performance_tuning_manager_environment.h"

#include "chrome/browser/performance_manager/test_support/fake_frame_throttling_delegate.h"
#include "chrome/browser/performance_manager/test_support/fake_high_efficiency_mode_toggle_delegate.h"
#include "chrome/browser/performance_manager/test_support/fake_power_monitor_source.h"
#include "components/prefs/pref_service.h"

namespace performance_manager::user_tuning {

TestUserPerformanceTuningManagerEnvironment::
    TestUserPerformanceTuningManagerEnvironment() = default;

TestUserPerformanceTuningManagerEnvironment::
    ~TestUserPerformanceTuningManagerEnvironment() = default;

void TestUserPerformanceTuningManagerEnvironment::SetUp(
    PrefService* local_state) {
  auto source = std::make_unique<FakePowerMonitorSource>();
  base::PowerMonitor::Initialize(std::move(source));

  manager_.reset(new user_tuning::UserPerformanceTuningManager(
      local_state, nullptr,
      std::make_unique<FakeFrameThrottlingDelegate>(&throttling_enabled_),
      std::make_unique<FakeHighEfficiencyModeToggleDelegate>()));
  manager_->Start();
}

void TestUserPerformanceTuningManagerEnvironment::TearDown() {
  base::PowerMonitor::ShutdownForTesting();
}

}  // namespace performance_manager::user_tuning
