// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/idle_manager/arc_idle_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/mojom/power.mojom.h"
#include "ash/components/arc/power/arc_power_bridge.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_power_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ash/arc/idle_manager/arc_background_service_observer.h"
#include "chrome/browser/ash/arc/idle_manager/arc_cpu_throttle_observer.h"
#include "chrome/browser/ash/arc/idle_manager/arc_display_power_observer.h"
#include "chrome/browser/ash/arc/idle_manager/arc_on_battery_observer.h"
#include "chrome/browser/ash/arc/idle_manager/arc_window_observer.h"
#include "chrome/browser/ash/arc/util/arc_window_watcher.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcIdleManagerTest : public testing::Test {
 public:
  ArcIdleManagerTest() = default;

  ArcIdleManagerTest(const ArcIdleManagerTest&) = delete;
  ArcIdleManagerTest& operator=(const ArcIdleManagerTest&) = delete;

  ~ArcIdleManagerTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kEnableArcIdleManager,
        {{kEnableArcIdleManagerIgnoreBatteryForPLT.name, "false"}});
    chromeos::PowerManagerClient::InitializeFake();

    // This is needed for setting up ArcPowerBridge.
    arc_service_manager_ = std::make_unique<ArcServiceManager>();

    // Order matters: TestingProfile must be after ArcServiceManager.
    testing_profile_ = std::make_unique<TestingProfile>();

    arc_window_watcher_ = std::make_unique<ash::ArcWindowWatcher>();

    arc_idle_manager_ =
        ArcIdleManager::GetForBrowserContextForTesting(testing_profile_.get());
    arc_idle_manager_->set_delegate_for_testing(
        std::make_unique<TestDelegateImpl>(this));

    cpu_throttle_observer_ =
        arc_idle_manager_->GetObserverByName(kArcCpuThrottleObserverName);
    DCHECK(cpu_throttle_observer_);

    on_battery_observer_ =
        arc_idle_manager_->GetObserverByName(kArcOnBatteryObserverName);
    // Observer exist when ignore battery is disabled.
    DCHECK(kEnableArcIdleManagerIgnoreBatteryForPLT.Get() ==
           !on_battery_observer_);

    display_power_observer_ =
        arc_idle_manager_->GetObserverByName(kArcDisplayPowerObserverName);
    DCHECK(display_power_observer_);

    background_service_observer_ =
        arc_idle_manager_->GetObserverByName(kArcBackgroundServiceObserverName);

    arc_window_observer_ =
        arc_idle_manager_->GetObserverByName(kArcWindowObserverName);
    DCHECK(arc_window_observer_);

    // Make sure the next SetActive() call calls into TestDelegateImpl. This
    // is necessary because ArcIdleManager's constructor may initialize the
    // variable (and call the default delegate for production) before doing
    // set_delegate_for_testing(). If that happens, SetActive() might not call
    // the test delegate as expected.
    arc_idle_manager_->reset_should_throttle_for_testing();

    CreatePowerInstance();
  }

  void TearDown() override {
    DestroyPowerInstance();
    arc_window_watcher_.reset();
    testing_profile_.reset();
    arc_service_manager_.reset();
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

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

  ArcBridgeService* arc_bridge_service() {
    return arc_service_manager_->arc_bridge_service();
  }

  ArcIdleManager* arc_idle_manager() { return arc_idle_manager_; }

  FakePowerInstance* power_instance() { return power_instance_.get(); }

  size_t interactive_enabled_counter() const {
    return interactive_enabled_counter_;
  }

  size_t interactive_disabled_counter() const {
    return interactive_disabled_counter_;
  }

  ash::ThrottleObserver* cpu_throttle_observer() {
    return cpu_throttle_observer_;
  }
  ash::ThrottleObserver* on_battery_observer() { return on_battery_observer_; }
  ash::ThrottleObserver* display_power_observer() {
    return display_power_observer_;
  }
  ash::ThrottleObserver* arc_window_observer() { return arc_window_observer_; }
  ash::ThrottleObserver* background_service_observer() {
    return background_service_observer_;
  }

 private:
  class TestDelegateImpl : public ArcIdleManager::Delegate {
   public:
    explicit TestDelegateImpl(ArcIdleManagerTest* test) : test_(test) {}
    ~TestDelegateImpl() override = default;

    TestDelegateImpl(const TestDelegateImpl&) = delete;
    TestDelegateImpl& operator=(const TestDelegateImpl&) = delete;

    void SetIdleState(ArcPowerBridge* arc_power_bridge,
                      ArcBridgeService* bridge,
                      bool enable) override {
      // enable means "interactive enabled", so "true" is "not idle".
      if (enable) {
        ++(test_->interactive_enabled_counter_);
      } else {
        ++(test_->interactive_disabled_counter_);
      }
    }

    raw_ptr<ArcIdleManagerTest> test_;
  };

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<TestingProfile> testing_profile_;

  std::unique_ptr<FakePowerInstance> power_instance_;
  std::unique_ptr<ash::ArcWindowWatcher> arc_window_watcher_;

  raw_ptr<ArcIdleManager, DanglingUntriaged> arc_idle_manager_;
  size_t interactive_enabled_counter_ = 0;
  size_t interactive_disabled_counter_ = 0;

  raw_ptr<ash::ThrottleObserver, DanglingUntriaged> cpu_throttle_observer_;
  raw_ptr<ash::ThrottleObserver, DanglingUntriaged> on_battery_observer_;
  raw_ptr<ash::ThrottleObserver, DanglingUntriaged> display_power_observer_;
  raw_ptr<ash::ThrottleObserver, DanglingUntriaged> arc_window_observer_;
  raw_ptr<ash::ThrottleObserver, DanglingUntriaged>
      background_service_observer_;
};

// Tests that ArcIdleManager can be constructed and destructed.
TEST_F(ArcIdleManagerTest, TestConstructDestruct) {}

// Tests that powerbridge early death causes no DCHECKs in observer list.
TEST_F(ArcIdleManagerTest, TestEarlyPowerBridgeDeath) {
  arc_idle_manager()->OnWillDestroyArcPowerBridge();
}

// Tests that ArcIdleManager responds appropriately to various observers.
TEST_F(ArcIdleManagerTest, TestThrottleInstance) {
  // When no one blocks, it should enable idle;
  on_battery_observer()->SetActive(false);
  display_power_observer()->SetActive(false);
  cpu_throttle_observer()->SetActive(false);
  background_service_observer()->SetActive(false);
  arc_window_observer()->SetActive(false);

  task_environment()->FastForwardBy(
      base::Milliseconds(kEnableArcIdleManagerDelayMs.Get()));

  EXPECT_EQ(0U, interactive_enabled_counter());
  EXPECT_EQ(2U, interactive_disabled_counter());

  // Battery observer blocking should caused idle disabled.
  on_battery_observer()->SetActive(true);
  EXPECT_EQ(1U, interactive_enabled_counter());
  EXPECT_EQ(2U, interactive_disabled_counter());

  // Reset.
  on_battery_observer()->SetActive(false);
  task_environment()->FastForwardBy(
      base::Milliseconds(kEnableArcIdleManagerDelayMs.Get()));
  EXPECT_EQ(1U, interactive_enabled_counter());
  EXPECT_EQ(3U, interactive_disabled_counter());

  // Display power blocking should caused idle disabled.
  display_power_observer()->SetActive(true);
  EXPECT_EQ(2U, interactive_enabled_counter());
  EXPECT_EQ(3U, interactive_disabled_counter());

  // Reset.
  display_power_observer()->SetActive(false);
  task_environment()->FastForwardBy(
      base::Milliseconds(kEnableArcIdleManagerDelayMs.Get()));
  EXPECT_EQ(2U, interactive_enabled_counter());
  EXPECT_EQ(4U, interactive_disabled_counter());

  // CPU throttle blocking should caused idle disabled.
  cpu_throttle_observer()->SetActive(true);
  EXPECT_EQ(3U, interactive_enabled_counter());
  EXPECT_EQ(4U, interactive_disabled_counter());

  // Reset.
  cpu_throttle_observer()->SetActive(false);
  task_environment()->FastForwardBy(
      base::Milliseconds(kEnableArcIdleManagerDelayMs.Get()));
  EXPECT_EQ(3U, interactive_enabled_counter());
  EXPECT_EQ(5U, interactive_disabled_counter());

  // ARC background service active caused idle disabled.
  background_service_observer()->SetActive(true);
  EXPECT_EQ(4U, interactive_enabled_counter());
  EXPECT_EQ(5U, interactive_disabled_counter());

  // Reset.
  background_service_observer()->SetActive(false);
  task_environment()->FastForwardBy(
      base::Milliseconds(kEnableArcIdleManagerDelayMs.Get()));
  EXPECT_EQ(4U, interactive_enabled_counter());
  EXPECT_EQ(6U, interactive_disabled_counter());

  // Window Observer active should cause idle disabled.
  arc_window_observer()->SetActive(true);
  EXPECT_EQ(5U, interactive_enabled_counter());
  EXPECT_EQ(6U, interactive_disabled_counter());

  // ResumeVm event when not idle causes additional idle-disable event.
  arc_idle_manager()->OnVmResumed();
  EXPECT_EQ(6U, interactive_enabled_counter());
  EXPECT_EQ(6U, interactive_disabled_counter());

  // Reset.
  arc_window_observer()->SetActive(false);
  task_environment()->FastForwardBy(
      base::Milliseconds(kEnableArcIdleManagerDelayMs.Get()));
  EXPECT_EQ(6U, interactive_enabled_counter());
  EXPECT_EQ(7U, interactive_disabled_counter());

  // ResumeVm event when idle does not generate switch events.
  arc_idle_manager()->OnVmResumed();
  EXPECT_EQ(6U, interactive_enabled_counter());
  EXPECT_EQ(7U, interactive_disabled_counter());
}

// Tests that ArcIdleManager records the screen off time metric correctly.
TEST_F(ArcIdleManagerTest, TestScreenOffTimerMetrics) {
  // When no one blocks, it should enable idle (screen off).

  on_battery_observer()->SetActive(false);
  display_power_observer()->SetActive(false);
  cpu_throttle_observer()->SetActive(false);
  background_service_observer()->SetActive(false);
  arc_window_observer()->SetActive(false);

  // Count time from here.
  base::ScopedMockElapsedTimersForTest mock_elapsed_timers;
  base::HistogramTester histogram_tester;

  task_environment()->FastForwardBy(
      base::Milliseconds(kEnableArcIdleManagerDelayMs.Get()));

  histogram_tester.ExpectUniqueTimeSample(
      "Arc.IdleManager.ScreenOffTime",
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime, 0);

  // Battery observer blocking should caused idle disabled (screen back on).
  on_battery_observer()->SetActive(true);
  EXPECT_EQ(1U, interactive_enabled_counter());
  EXPECT_EQ(2U, interactive_disabled_counter());

  histogram_tester.ExpectUniqueTimeSample(
      "Arc.IdleManager.ScreenOffTime",
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime, 1);

  // Fake a disconnection (akin to a crash of SystemServer)
  arc_idle_manager()->OnConnectionClosed();
  // we are NOT throttled, shouldn't see any change

  histogram_tester.ExpectUniqueTimeSample(
      "Arc.IdleManager.ScreenOffTime",
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime, 1);

  // State change while we are not watching.
  on_battery_observer()->SetActive(false);

  // Fake systemserver coming back
  arc_idle_manager()->OnConnectionReady();

  histogram_tester.ExpectUniqueTimeSample(
      "Arc.IdleManager.ScreenOffTime",
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime, 1);

  // Again, fake a disconnection (akin to a crash of SystemServer)
  arc_idle_manager()->OnConnectionClosed();

  // This time, we should see a counter bump, as disconnection happened
  // while we were throttled.
  histogram_tester.ExpectUniqueTimeSample(
      "Arc.IdleManager.ScreenOffTime",
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime, 2);
}

}  // namespace arc
