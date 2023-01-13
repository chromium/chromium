// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/memory_pressure_listener.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {

using MemoryPressureLevel = MemoryPressureListener::MemoryPressureLevel;

class MemoryPressureListenerTest : public testing::Test {
 public:
  MemoryPressureListenerTest()
      : task_environment_(test::TaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    listener_ = std::make_unique<MemoryPressureListener>(
        FROM_HERE, BindRepeating(&MemoryPressureListenerTest::OnMemoryPressure,
                                 Unretained(this)));
  }

  void TearDown() override {
    listener_.reset();
  }

 protected:
  void ExpectNotification(
      void (*notification_function)(MemoryPressureLevel),
      MemoryPressureLevel level) {
    EXPECT_CALL(*this, OnMemoryPressure(level)).Times(1);
    notification_function(level);
    RunLoop().RunUntilIdle();
  }

  void ExpectNoNotification(
      void (*notification_function)(MemoryPressureLevel),
      MemoryPressureLevel level) {
    EXPECT_CALL(*this, OnMemoryPressure(testing::_)).Times(0);
    notification_function(level);
    RunLoop().RunUntilIdle();
  }

 private:
  MOCK_METHOD1(OnMemoryPressure,
               void(MemoryPressureListener::MemoryPressureLevel));

  test::TaskEnvironment task_environment_;
  std::unique_ptr<MemoryPressureListener> listener_;
};

TEST_F(MemoryPressureListenerTest, NotifyMemoryPressure) {
  // Memory pressure notifications are not suppressed by default.
  EXPECT_FALSE(MemoryPressureListener::AreNotificationsSuppressed());
  ExpectNotification(&MemoryPressureListener::NotifyMemoryPressure,
                     MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE);
  ExpectNotification(&MemoryPressureListener::SimulatePressureNotification,
                     MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE);

  // Enable suppressing memory pressure notifications.
  MemoryPressureListener::SetNotificationsSuppressed(true);
  EXPECT_TRUE(MemoryPressureListener::AreNotificationsSuppressed());
  ExpectNoNotification(&MemoryPressureListener::NotifyMemoryPressure,
                       MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE);
  ExpectNotification(&MemoryPressureListener::SimulatePressureNotification,
                     MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE);

  // Disable suppressing memory pressure notifications.
  MemoryPressureListener::SetNotificationsSuppressed(false);
  EXPECT_FALSE(MemoryPressureListener::AreNotificationsSuppressed());
  ExpectNotification(&MemoryPressureListener::NotifyMemoryPressure,
                     MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL);
  ExpectNotification(&MemoryPressureListener::SimulatePressureNotification,
                     MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL);
}

}  // namespace base
