// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/idle_manager/arc_idle_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/components/arc/mojom/power.mojom.h"
#include "ash/components/arc/power/arc_power_bridge.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_power_instance.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/arc/idle_manager/arc_cpu_throttle_observer.h"
#include "chrome/browser/ash/arc/idle_manager/arc_display_power_observer.h"
#include "chrome/browser/ash/arc/idle_manager/arc_on_battery_observer.h"
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
    chromeos::PowerManagerClient::InitializeFake();

    // This is needed for setting up ArcPowerBridge.
    arc_service_manager_ = std::make_unique<ArcServiceManager>();

    // Order matters: TestingProfile must be after ArcServiceManager.
    testing_profile_ = std::make_unique<TestingProfile>();

    arc_idle_manager_ =
        ArcIdleManager::GetForBrowserContextForTesting(testing_profile_.get());
    arc_idle_manager_->set_delegate_for_testing(
        std::make_unique<TestDelegateImpl>(this));

    cpu_throttle_observer_ =
        arc_idle_manager_->GetObserverByName(kArcCpuThrottleObserverName);
    DCHECK(cpu_throttle_observer_);

    on_battery_observer_ =
        arc_idle_manager_->GetObserverByName(kArcOnBatteryObserverName);
    DCHECK(on_battery_observer_);

    display_power_observer_ =
        arc_idle_manager_->GetObserverByName(kArcDisplayPowerObserverName);
    DCHECK(display_power_observer_);

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

  ArcIdleManager* arc_instance_throttle() { return arc_idle_manager_; }

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

 private:
  class TestDelegateImpl : public ArcIdleManager::Delegate {
   public:
    explicit TestDelegateImpl(ArcIdleManagerTest* test) : test_(test) {}
    ~TestDelegateImpl() override = default;

    TestDelegateImpl(const TestDelegateImpl&) = delete;
    TestDelegateImpl& operator=(const TestDelegateImpl&) = delete;

    void SetInteractiveMode(ArcBridgeService* bridge, bool enable) override {
      // enable means "interactive enabled", so "true" is "not idle".
      if (enable) {
        ++(test_->interactive_enabled_counter_);
      } else {
        ++(test_->interactive_disabled_counter_);
      }
    }

    ArcIdleManagerTest* test_;
  };

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<TestingProfile> testing_profile_;

  std::unique_ptr<FakePowerInstance> power_instance_;

  ArcIdleManager* arc_idle_manager_;
  size_t interactive_enabled_counter_ = 0;
  size_t interactive_disabled_counter_ = 0;

  ash::ThrottleObserver* cpu_throttle_observer_;
  ash::ThrottleObserver* on_battery_observer_;
  ash::ThrottleObserver* display_power_observer_;
};

// Tests that ArcIdleManager can be constructed and destructed.

TEST_F(ArcIdleManagerTest, TestConstructDestruct) {}

// Tests that ArcIdleManager responds appropriately to various observers.
TEST_F(ArcIdleManagerTest, TestThrottleInstance) {
  // When no one blocks, it should enable idle;
  on_battery_observer()->SetActive(false);
  display_power_observer()->SetActive(false);
  cpu_throttle_observer()->SetActive(false);

  EXPECT_EQ(0U, interactive_enabled_counter());
  EXPECT_EQ(2U, interactive_disabled_counter());

  // Battery observer blocking should caused idle disabled.
  on_battery_observer()->SetActive(true);
  EXPECT_EQ(1U, interactive_enabled_counter());
  EXPECT_EQ(2U, interactive_disabled_counter());

  // Reset.
  on_battery_observer()->SetActive(false);
  EXPECT_EQ(1U, interactive_enabled_counter());
  EXPECT_EQ(3U, interactive_disabled_counter());

  // Display power blocking should caused idle disabled.
  display_power_observer()->SetActive(true);
  EXPECT_EQ(2U, interactive_enabled_counter());
  EXPECT_EQ(3U, interactive_disabled_counter());

  // Reset.
  display_power_observer()->SetActive(false);
  EXPECT_EQ(2U, interactive_enabled_counter());
  EXPECT_EQ(4U, interactive_disabled_counter());

  // CPU throttle blocking should caused idle disabled.
  cpu_throttle_observer()->SetActive(true);
  EXPECT_EQ(3U, interactive_enabled_counter());
  EXPECT_EQ(4U, interactive_disabled_counter());

  // Reset.
  cpu_throttle_observer()->SetActive(false);
  EXPECT_EQ(3U, interactive_enabled_counter());
  EXPECT_EQ(5U, interactive_disabled_counter());
}

}  // namespace arc
