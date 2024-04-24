// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <memory>

#include "base/system/system_monitor.h"

#include "base/run_loop.h"
#include "base/test/mock_devices_changed_observer.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class SystemMonitorTest : public testing::Test {
 public:
  SystemMonitorTest(const SystemMonitorTest&) = delete;
  SystemMonitorTest& operator=(const SystemMonitorTest&) = delete;

 protected:
  SystemMonitorTest() { system_monitor_ = std::make_unique<SystemMonitor>(); }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<SystemMonitor> system_monitor_;
};

TEST_F(SystemMonitorTest, DeviceChangeNotifications) {
  const int kObservers = 5;

  testing::Sequence mock_sequencer[kObservers];
  MockDevicesChangedObserver observers[kObservers];
  for (int index = 0; index < kObservers; ++index) {
    system_monitor_->AddDevicesChangedObserver(&observers[index]);

    EXPECT_CALL(observers[index],
                OnDevicesChanged(SystemMonitor::DEVTYPE_UNKNOWN))
        .Times(3)
        .InSequence(mock_sequencer[index]);
  }

  system_monitor_->ProcessDevicesChanged(SystemMonitor::DEVTYPE_UNKNOWN);
  RunLoop().RunUntilIdle();

  system_monitor_->ProcessDevicesChanged(SystemMonitor::DEVTYPE_UNKNOWN);
  system_monitor_->ProcessDevicesChanged(SystemMonitor::DEVTYPE_UNKNOWN);
  RunLoop().RunUntilIdle();
}

}  // namespace

}  // namespace base
