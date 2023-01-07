// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_power_throttle_observer.h"

#include "ash/components/arc/mojom/anr.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

void TestCallback(int* counter,
                  int* active_counter,
                  const ash::ThrottleObserver* self) {
  (*counter)++;
  if (self->active())
    (*active_counter)++;
}

}  // namespace

class ArcPowerThrottleObserverTest : public testing::Test {
 public:
  ArcPowerThrottleObserverTest() = default;
  ~ArcPowerThrottleObserverTest() override = default;

  ArcPowerThrottleObserverTest(const ArcPowerThrottleObserverTest&) = delete;
  ArcPowerThrottleObserverTest& operator=(const ArcPowerThrottleObserverTest&) =
      delete;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    service_manager_ = std::make_unique<ArcServiceManager>();
    session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));
    testing_profile_ = std::make_unique<TestingProfile>();

    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());

    ArcPowerBridge* const power_bridge =
        ArcPowerBridge::GetForBrowserContextForTesting(testing_profile_.get());
    DCHECK(power_bridge);
  }

  void TearDown() override {
    testing_profile_.reset();
    session_manager_.reset();
    service_manager_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

 protected:
  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }
  TestingProfile* profile() { return testing_profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<ArcServiceManager> service_manager_;
  std::unique_ptr<ArcSessionManager> session_manager_;
  std::unique_ptr<TestingProfile> testing_profile_;
};

TEST_F(ArcPowerThrottleObserverTest, Default) {
  ArcPowerThrottleObserver observer;
  int call_count = 0;
  int active_count = 0;
  observer.StartObserving(
      profile(),
      base::BindRepeating(&TestCallback, &call_count, &active_count));
  EXPECT_EQ(0, call_count);
  EXPECT_EQ(0, active_count);
  EXPECT_FALSE(observer.active());

  observer.OnPreAnr(mojom::AnrType::CONTENT_PROVIDER);
  EXPECT_EQ(1, call_count);
  EXPECT_EQ(1, active_count);
  EXPECT_TRUE(observer.active());

  // One more notification does not change state.
  observer.OnPreAnr(mojom::AnrType::PROCESS);
  EXPECT_EQ(2, call_count);
  EXPECT_EQ(2, active_count);
  EXPECT_TRUE(observer.active());

  // Duration of temporary lifting CPU restrictions is not yet over.
  task_environment().FastForwardBy(base::Milliseconds(9000));
  EXPECT_EQ(2, call_count);
  EXPECT_EQ(2, active_count);
  EXPECT_TRUE(observer.active());

  // Wait a bit more, duration of temporary lifting CPU restrictions is over
  // now.
  task_environment().FastForwardBy(base::Milliseconds(1000));
  EXPECT_EQ(3, call_count);
  EXPECT_EQ(2, active_count);
  EXPECT_FALSE(observer.active());

  // Wait much longer, the last state should not change.
  task_environment().FastForwardBy(base::Milliseconds(10000));
  EXPECT_EQ(3, call_count);
  EXPECT_EQ(2, active_count);
  EXPECT_FALSE(observer.active());

  // Timer is not fired after stopping observing.
  observer.OnPreAnr(mojom::AnrType::CONTENT_PROVIDER);
  EXPECT_EQ(4, call_count);
  EXPECT_EQ(3, active_count);
  EXPECT_TRUE(observer.active());

  observer.StopObserving();
  task_environment().FastForwardBy(base::Milliseconds(11000));
  EXPECT_EQ(4, call_count);
  EXPECT_EQ(3, active_count);
  EXPECT_TRUE(observer.active());
}

// Test verifies that new preANR extends active time of the lock in the case
// new preANR type requires more time for handling.
TEST_F(ArcPowerThrottleObserverTest, ActiveTimeExtended) {
  ArcPowerThrottleObserver observer;
  int call_count = 0;
  int active_count = 0;
  observer.StartObserving(
      profile(),
      base::BindRepeating(&TestCallback, &call_count, &active_count));

  observer.OnPreAnr(mojom::AnrType::PROCESS);
  EXPECT_EQ(1, call_count);
  EXPECT_EQ(1, active_count);
  EXPECT_TRUE(observer.active());

  task_environment().FastForwardBy(base::Milliseconds(5000));
  // Timer not yet fired
  EXPECT_TRUE(observer.active());

  observer.OnPreAnr(mojom::AnrType::FOREGROUND_SERVICE);
  EXPECT_EQ(2, call_count);
  EXPECT_EQ(2, active_count);
  EXPECT_TRUE(observer.active());

  task_environment().FastForwardBy(base::Milliseconds(19000));
  // Without FOREGROUND_SERVICE preANR timer would be already fired.
  // So it is still active now.
  EXPECT_EQ(2, call_count);
  EXPECT_EQ(2, active_count);
  EXPECT_TRUE(observer.active());

  task_environment().FastForwardBy(base::Milliseconds(1000));
  EXPECT_EQ(3, call_count);
  EXPECT_EQ(2, active_count);
  EXPECT_FALSE(observer.active());
}

// Test verifies that new preANR does not change active time of the lock in
// the case new preANR type requires less time that currently remained from the
// previous preANR handling.
TEST_F(ArcPowerThrottleObserverTest, ActiveTimePreserved) {
  ArcPowerThrottleObserver observer;
  int call_count = 0;
  int active_count = 0;
  observer.StartObserving(
      profile(),
      base::BindRepeating(&TestCallback, &call_count, &active_count));

  observer.OnPreAnr(mojom::AnrType::FOREGROUND_SERVICE);
  EXPECT_EQ(1, call_count);
  EXPECT_EQ(1, active_count);
  EXPECT_TRUE(observer.active());

  task_environment().FastForwardBy(base::Milliseconds(9000));
  // Timer not yet fired
  EXPECT_TRUE(observer.active());

  // Process has shorter active time. It should not change the previous timeout.
  observer.OnPreAnr(mojom::AnrType::PROCESS);
  EXPECT_EQ(2, call_count);
  EXPECT_EQ(2, active_count);
  EXPECT_TRUE(observer.active());

  task_environment().FastForwardBy(base::Milliseconds(10000));
  EXPECT_EQ(2, call_count);
  EXPECT_EQ(2, active_count);
  EXPECT_TRUE(observer.active());

  // Only now the lock becomes inactive.
  task_environment().FastForwardBy(base::Milliseconds(1000));
  EXPECT_EQ(3, call_count);
  EXPECT_EQ(2, active_count);
  EXPECT_FALSE(observer.active());
}

TEST_F(ArcPowerThrottleObserverTest, Broadcast) {
  ArcPowerThrottleObserver observer;
  int call_count = 0;
  int active_count = 0;
  observer.StartObserving(
      profile(),
      base::BindRepeating(&TestCallback, &call_count, &active_count));

  observer.OnPreAnr(mojom::AnrType::BROADCAST);
  EXPECT_EQ(1, call_count);
  EXPECT_EQ(1, active_count);
  EXPECT_TRUE(observer.active());

  task_environment().FastForwardBy(base::Milliseconds(9999));
  EXPECT_EQ(1, call_count);
  EXPECT_EQ(1, active_count);
  EXPECT_TRUE(observer.active());

  // Only now the lock becomes inactive.
  task_environment().FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(2, call_count);
  EXPECT_EQ(1, active_count);
  EXPECT_FALSE(observer.active());
}

}  // namespace arc
