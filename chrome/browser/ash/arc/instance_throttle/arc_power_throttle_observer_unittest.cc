// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_power_throttle_observer.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/concierge/concierge_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/arc/mojom/anr.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/session/arc_service_manager.h"
#include "components/arc/test/arc_util_test_support.h"
#include "components/arc/test/fake_arc_session.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcPowerThrottleObserverTest : public testing::Test {
 public:
  ArcPowerThrottleObserverTest() = default;
  ~ArcPowerThrottleObserverTest() override = default;

  ArcPowerThrottleObserverTest(const ArcPowerThrottleObserverTest&) = delete;
  ArcPowerThrottleObserverTest& operator=(const ArcPowerThrottleObserverTest&) =
      delete;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    // Need to initialize DBusThreadManager before ArcSessionManager's
    // constructor calls DBusThreadManager::Get().
    chromeos::DBusThreadManager::Initialize();
    chromeos::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
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
  observer.StartObserving(
      profile(),
      base::BindRepeating([](int* counter) { (*counter)++; }, &call_count));

  EXPECT_EQ(0, call_count);
  EXPECT_FALSE(observer.active());

  observer.OnPreAnr(mojom::AnrType::SERVICE);
  EXPECT_EQ(1, call_count);
  EXPECT_TRUE(observer.active());

  // One more notification does not change state.
  observer.OnPreAnr(mojom::AnrType::PROCESS);
  EXPECT_EQ(1, call_count);
  EXPECT_TRUE(observer.active());

  // Duration of temporary lifting CPU restrictions is not yet over.
  task_environment().FastForwardBy(base::Milliseconds(9000));
  EXPECT_EQ(1, call_count);
  EXPECT_TRUE(observer.active());

  // Wait a bit more, duration of temporary lifting CPU restrictions is over
  // now.
  task_environment().FastForwardBy(base::Milliseconds(1000));
  EXPECT_EQ(2, call_count);
  EXPECT_FALSE(observer.active());

  // Wait much longer, the last state should not change.
  task_environment().FastForwardBy(base::Milliseconds(10000));
  EXPECT_EQ(2, call_count);
  EXPECT_FALSE(observer.active());

  // Timer is not fired after stopping observing.
  observer.OnPreAnr(mojom::AnrType::SERVICE);
  EXPECT_EQ(3, call_count);
  EXPECT_TRUE(observer.active());

  observer.StopObserving();
  task_environment().FastForwardBy(base::Milliseconds(11000));
  EXPECT_EQ(3, call_count);
  EXPECT_TRUE(observer.active());
}

}  // namespace arc
