// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_instance_throttle.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/mojom/power.mojom.h"
#include "ash/components/arc/power/arc_power_bridge.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "ash/components/arc/test/fake_power_instance.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "chrome/browser/ash/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/throttle_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/concierge/concierge_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "services/device/public/cpp/test/test_wake_lock_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcInstanceThrottleTest : public testing::Test {
 public:
  ArcInstanceThrottleTest() = default;
  ~ArcInstanceThrottleTest() override = default;

  ArcInstanceThrottleTest(const ArcInstanceThrottleTest&) = delete;
  ArcInstanceThrottleTest& operator=(const ArcInstanceThrottleTest&) = delete;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    // Need to initialize DBusThreadManager before ArcSessionManager's
    // constructor calls DBusThreadManager::Get().
    chromeos::DBusThreadManager::Initialize();
    chromeos::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));
    testing_profile_ = std::make_unique<TestingProfile>();

    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());

    ArcBootPhaseMonitorBridge::GetForBrowserContextForTesting(
        testing_profile_.get());
    arc_instance_throttle_ =
        ArcInstanceThrottle::GetForBrowserContextForTesting(
            testing_profile_.get());
    arc_instance_throttle_->set_delegate_for_testing(
        std::make_unique<TestDelegateImpl>(this));
  }

  void TearDown() override {
    DestroyPowerInstance();

    testing_profile_.reset();
    arc_session_manager_.reset();
    arc_service_manager_.reset();
    chromeos::ConciergeClient::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
  }

 protected:
  void CreatePowerInstance() {
    ArcPowerBridge* const power_bridge =
        ArcPowerBridge::GetForBrowserContextForTesting(testing_profile_.get());
    DCHECK(power_bridge);

    power_instance_ = std::make_unique<FakePowerInstance>();
    arc_bridge_service()->power()->SetInstance(power_instance_.get());
    WaitForInstanceReady(arc_bridge_service()->power());
  }

  void DestroyPowerInstance() {
    if (!power_instance_)
      return;
    arc_bridge_service()->power()->CloseInstance(power_instance_.get());
    power_instance_.reset();
  }

  sync_preferences::TestingPrefServiceSyncable* GetPrefs() {
    return testing_profile_->GetTestingPrefService();
  }

  ArcBridgeService* arc_bridge_service() {
    return arc_service_manager_->arc_bridge_service();
  }

  ArcInstanceThrottle* arc_instance_throttle() {
    return arc_instance_throttle_;
  }

  FakePowerInstance* power_instance() { return power_instance_.get(); }

  size_t disable_cpu_restriction_counter() const {
    return disable_cpu_restriction_counter_;
  }

  size_t enable_cpu_restriction_counter() const {
    return enable_cpu_restriction_counter_;
  }

 private:
  class TestDelegateImpl : public ArcInstanceThrottle::Delegate {
   public:
    explicit TestDelegateImpl(ArcInstanceThrottleTest* test) : test_(test) {}
    ~TestDelegateImpl() override = default;

    TestDelegateImpl(const TestDelegateImpl&) = delete;
    TestDelegateImpl& operator=(const TestDelegateImpl&) = delete;

    void SetCpuRestriction(CpuRestrictionState cpu_restriction_state) override {
      switch (cpu_restriction_state) {
        case CpuRestrictionState::CPU_RESTRICTION_FOREGROUND:
          ++(test_->disable_cpu_restriction_counter_);
          break;
        case CpuRestrictionState::CPU_RESTRICTION_BACKGROUND:
          ++(test_->enable_cpu_restriction_counter_);
          break;
      }
    }

    void RecordCpuRestrictionDisabledUMA(const std::string& observer_name,
                                         base::TimeDelta delta) override {}

    ArcInstanceThrottleTest* test_;
  };

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<FakePowerInstance> power_instance_;
  ArcInstanceThrottle* arc_instance_throttle_;
  size_t disable_cpu_restriction_counter_ = 0;
  size_t enable_cpu_restriction_counter_ = 0;
};

// Tests that ArcInstanceThrottle can be constructed and destructed.

TEST_F(ArcInstanceThrottleTest, TestConstructDestruct) {}

// Tests that ArcInstanceThrottle adjusts ARC CPU restriction
// when ThrottleInstance is called.
TEST_F(ArcInstanceThrottleTest, TestThrottleInstance) {
  arc_instance_throttle()->set_level_for_testing(
      ash::ThrottleObserver::PriorityLevel::LOW);
  EXPECT_EQ(1U, enable_cpu_restriction_counter());
  EXPECT_EQ(0U, disable_cpu_restriction_counter());

  // ArcInstanceThrottle level is already LOW, expect no change
  arc_instance_throttle()->set_level_for_testing(
      ash::ThrottleObserver::PriorityLevel::LOW);
  EXPECT_EQ(1U, enable_cpu_restriction_counter());
  EXPECT_EQ(0U, disable_cpu_restriction_counter());

  arc_instance_throttle()->set_level_for_testing(
      ash::ThrottleObserver::PriorityLevel::CRITICAL);
  EXPECT_EQ(1U, enable_cpu_restriction_counter());
  EXPECT_EQ(1U, disable_cpu_restriction_counter());

  arc_instance_throttle()->set_level_for_testing(
      ash::ThrottleObserver::PriorityLevel::LOW);
  EXPECT_EQ(2U, enable_cpu_restriction_counter());
  EXPECT_EQ(1U, disable_cpu_restriction_counter());
}

// Tests that power instance is correctly notified.
TEST_F(ArcInstanceThrottleTest, TestPowerNotificationEnabledByDefault) {
  // Set power instance and it should be automatically notified once connection
  // is made.
  CreatePowerInstance();
  EXPECT_EQ(1, power_instance()->cpu_restriction_state_count());
  EXPECT_EQ(mojom::CpuRestrictionState::CPU_RESTRICTION_BACKGROUND,
            power_instance()->last_cpu_restriction_state());

  arc_instance_throttle()->set_level_for_testing(
      ash::ThrottleObserver::PriorityLevel::CRITICAL);
  EXPECT_EQ(2, power_instance()->cpu_restriction_state_count());
  EXPECT_EQ(mojom::CpuRestrictionState::CPU_RESTRICTION_FOREGROUND,
            power_instance()->last_cpu_restriction_state());

  arc_instance_throttle()->set_level_for_testing(
      ash::ThrottleObserver::PriorityLevel::LOW);
  EXPECT_EQ(3, power_instance()->cpu_restriction_state_count());
  EXPECT_EQ(mojom::CpuRestrictionState::CPU_RESTRICTION_BACKGROUND,
            power_instance()->last_cpu_restriction_state());
}

// Tests that power instance notification is off by default.
TEST_F(ArcInstanceThrottleTest, TestPowerNotificationDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({},
                                       {arc::kEnableThrottlingNotification});
  // Set power instance and it should be automatically notified once connection
  // is made.
  CreatePowerInstance();
  arc_instance_throttle()->set_level_for_testing(
      ash::ThrottleObserver::PriorityLevel::CRITICAL);
  arc_instance_throttle()->set_level_for_testing(
      ash::ThrottleObserver::PriorityLevel::LOW);
  EXPECT_EQ(0, power_instance()->cpu_restriction_state_count());
}

}  // namespace arc
