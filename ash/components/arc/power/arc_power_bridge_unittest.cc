// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/power/arc_power_bridge.h"

#include <utility>

#include "ash/components/arc/mojom/power.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_power_instance.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/patchpanel/fake_patchpanel_client.h"
#include "chromeos/ash/components/dbus/patchpanel/patchpanel_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/test/test_wake_lock_provider.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

class TestObserver : public ArcPowerBridge::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  // ArcPowerBridge::Observer:
  void OnWakefulnessChanged(mojom::WakefulnessMode mode) override {
    last_mode_ = mode;
    ++update_count_;
  }

  int GetUpdateCountAndReset() {
    const int update_count = update_count_;
    update_count_ = 0;
    return update_count;
  }

  mojom::WakefulnessMode last_mode() const { return last_mode_; }

 private:
  int update_count_ = 0;
  mojom::WakefulnessMode last_mode_ = mojom::WakefulnessMode::UNKNOWN;

  TestObserver(TestObserver const&) = delete;
  TestObserver& operator=(TestObserver const&) = delete;
};

}  // namespace

using device::mojom::WakeLockType;

class ArcPowerBridgeTest : public testing::Test {
 public:
  // Initial screen brightness percent for the Chrome OS power manager.
  static constexpr double kInitialBrightness = 100.0;

  ArcPowerBridgeTest() = default;

  ArcPowerBridgeTest(const ArcPowerBridgeTest&) = delete;
  ArcPowerBridgeTest& operator=(const ArcPowerBridgeTest&) = delete;

  ~ArcPowerBridgeTest() override = default;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    ash::PatchPanelClient::InitializeFake();
    power_manager_client()->set_screen_brightness_percent(kInitialBrightness);

    wake_lock_provider_ = std::make_unique<device::TestWakeLockProvider>();

    bridge_service_ = std::make_unique<ArcBridgeService>();
    power_bridge_ = std::make_unique<ArcPowerBridge>(nullptr /* context */,
                                                     bridge_service_.get());

    mojo::Remote<device::mojom::WakeLockProvider> remote_provider;
    wake_lock_provider_->BindReceiver(
        remote_provider.BindNewPipeAndPassReceiver());
    power_bridge_->SetWakeLockProviderForTesting(std::move(remote_provider));
    CreatePowerInstance();
  }

  void TearDown() override {
    DestroyPowerInstance();
    power_bridge_.reset();
    chromeos::PowerManagerClient::Shutdown();
    ash::PatchPanelClient::Shutdown();
  }

 protected:
  // Creates a FakePowerInstance for |bridge_service_|. This results in
  // ArcPowerBridge::OnInstanceReady() being called.
  void CreatePowerInstance() {
    power_instance_ = std::make_unique<FakePowerInstance>();
    bridge_service_->power()->SetInstance(power_instance_.get());
    WaitForInstanceReady(bridge_service_->power());
  }

  // Destroys the FakePowerInstance. This results in
  // ArcPowerBridge::OnInstanceClosed() being called.
  void DestroyPowerInstance() {
    if (!power_instance_)
      return;
    bridge_service_->power()->CloseInstance(power_instance_.get());
    power_instance_.reset();
  }

  // Acquires or releases a display wake lock of type |type|.
  void AcquireDisplayWakeLock(mojom::DisplayWakeLockType type) {
    power_bridge_->OnAcquireDisplayWakeLock(type);
    power_bridge_->FlushWakeLocksForTesting();
  }

  void ReleaseDisplayWakeLock(mojom::DisplayWakeLockType type) {
    power_bridge_->OnReleaseDisplayWakeLock(type);
    power_bridge_->FlushWakeLocksForTesting();
  }

  // Returns the number of active wake locks of type |type|.
  int GetActiveWakeLocks(WakeLockType type) {
    base::RunLoop run_loop;
    int result_count = 0;
    wake_lock_provider_->GetActiveWakeLocksForTests(
        type,
        base::BindOnce(
            [](base::RunLoop* run_loop, int* result_count, int32_t count) {
              *result_count = count;
              run_loop->Quit();
            },
            &run_loop, &result_count));
    run_loop.Run();
    return result_count;
  }

  chromeos::FakePowerManagerClient* power_manager_client() {
    return chromeos::FakePowerManagerClient::Get();
  }

  FakePowerInstance* power_instance() { return power_instance_.get(); }
  ArcPowerBridge* power_bridge() { return power_bridge_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<ArcBridgeService> bridge_service_;
  std::unique_ptr<FakePowerInstance> power_instance_;
  std::unique_ptr<ArcPowerBridge> power_bridge_;

  std::unique_ptr<device::TestWakeLockProvider> wake_lock_provider_;
};

TEST_F(ArcPowerBridgeTest, SuspendAndResume) {
  ASSERT_EQ(0, power_instance()->num_suspend());
  ASSERT_EQ(0, power_instance()->num_resume());

  // When powerd notifies Chrome that the system is about to suspend,
  // ArcPowerBridge should notify Android and take a suspend readiness callback
  // to defer the suspend operation.
  power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(1, power_instance()->num_suspend());
  EXPECT_EQ(0, power_instance()->num_resume());
  EXPECT_EQ(1,
            power_manager_client()->num_pending_suspend_readiness_callbacks());

  // Simulate Android acknowledging that it's ready for the system to suspend.
  power_instance()->GetSuspendCallback().Run();
  EXPECT_EQ(0,
            power_manager_client()->num_pending_suspend_readiness_callbacks());

  power_manager_client()->SendSuspendDone();
  EXPECT_EQ(1, power_instance()->num_suspend());
  EXPECT_EQ(1, power_instance()->num_resume());

  // We shouldn't crash if the instance isn't ready.
  DestroyPowerInstance();
  power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(0,
            power_manager_client()->num_pending_suspend_readiness_callbacks());
  power_manager_client()->SendSuspendDone();
}

TEST_F(ArcPowerBridgeTest, SetInteractive) {
  power_bridge()->OnPowerStateChanged(chromeos::DISPLAY_POWER_ALL_OFF);
  EXPECT_FALSE(power_instance()->interactive());

  int cnt1 =
      ash::FakePatchPanelClient::Get()->GetAndroidInteractiveStateNotifyCount();
  power_bridge()->OnPowerStateChanged(chromeos::DISPLAY_POWER_ALL_ON);
  EXPECT_TRUE(power_instance()->interactive());
  int cnt2 =
      ash::FakePatchPanelClient::Get()->GetAndroidInteractiveStateNotifyCount();
  // NotifyAndroidInteractiveState is expected to be called when ARC interactive
  // state changes.
  EXPECT_EQ(1, cnt2 - cnt1);

  // We shouldn't crash if the instance isn't ready.
  DestroyPowerInstance();
  power_bridge()->OnPowerStateChanged(chromeos::DISPLAY_POWER_ALL_OFF);
  int cnt3 =
      ash::FakePatchPanelClient::Get()->GetAndroidInteractiveStateNotifyCount();
  // NotifyAndroidInteractiveState is not supposed to be called when power
  // instance is not ready.
  EXPECT_EQ(cnt2, cnt3);
}

TEST_F(ArcPowerBridgeTest, ScreenBrightness) {
  // Let the initial GetScreenBrightnessPercent() task run.
  base::RunLoop().RunUntilIdle();
  EXPECT_DOUBLE_EQ(kInitialBrightness, power_instance()->screen_brightness());

  // Check that Chrome OS brightness changes are passed to Android.
  const double kUpdatedBrightness = 45.0;
  power_manager_client()->set_screen_brightness_percent(kUpdatedBrightness);
  power_manager::BacklightBrightnessChange change;
  change.set_percent(kUpdatedBrightness);
  change.set_cause(power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  power_manager_client()->SendScreenBrightnessChanged(change);
  EXPECT_DOUBLE_EQ(kUpdatedBrightness, power_instance()->screen_brightness());

  // Requests from Android should update the Chrome OS brightness.
  const double kAndroidBrightness = 70.0;
  power_bridge()->OnScreenBrightnessUpdateRequest(kAndroidBrightness);
  EXPECT_DOUBLE_EQ(kAndroidBrightness,
                   power_manager_client()->screen_brightness_percent());

  // To prevent battles between Chrome OS and Android, the updated brightness
  // shouldn't be passed to Android immediately, but it should be passed after
  // the timer fires.
  change.set_percent(kAndroidBrightness);
  power_manager_client()->SendScreenBrightnessChanged(change);
  EXPECT_DOUBLE_EQ(kUpdatedBrightness, power_instance()->screen_brightness());
  ASSERT_TRUE(power_bridge()->TriggerNotifyBrightnessTimerForTesting());
  EXPECT_DOUBLE_EQ(kAndroidBrightness, power_instance()->screen_brightness());
}

TEST_F(ArcPowerBridgeTest, PowerSupplyInfoChanged) {
  std::optional<power_manager::PowerSupplyProperties> prop =
      power_manager_client()->GetLastStatus();
  ASSERT_TRUE(prop.has_value());
  prop->set_battery_state(power_manager::PowerSupplyProperties::FULL);
  power_manager_client()->UpdatePowerProperties(prop.value());

  // Check that Chrome OS power changes are passed to Android.
  const int prev_call_count = power_instance()->num_power_supply_info();
  prop->set_battery_state(power_manager::PowerSupplyProperties::DISCHARGING);
  power_manager_client()->UpdatePowerProperties(prop.value());
  EXPECT_EQ(1 + prev_call_count, power_instance()->num_power_supply_info());
}

TEST_F(ArcPowerBridgeTest, DifferentWakeLocks) {
  AcquireDisplayWakeLock(mojom::DisplayWakeLockType::BRIGHT);
  EXPECT_EQ(1, GetActiveWakeLocks(WakeLockType::kPreventDisplaySleep));
  EXPECT_EQ(0,
            GetActiveWakeLocks(WakeLockType::kPreventDisplaySleepAllowDimming));

  AcquireDisplayWakeLock(mojom::DisplayWakeLockType::DIM);
  EXPECT_EQ(1, GetActiveWakeLocks(WakeLockType::kPreventDisplaySleep));
  EXPECT_EQ(1,
            GetActiveWakeLocks(WakeLockType::kPreventDisplaySleepAllowDimming));

  ReleaseDisplayWakeLock(mojom::DisplayWakeLockType::BRIGHT);
  EXPECT_EQ(0, GetActiveWakeLocks(WakeLockType::kPreventDisplaySleep));
  EXPECT_EQ(1,
            GetActiveWakeLocks(WakeLockType::kPreventDisplaySleepAllowDimming));

  ReleaseDisplayWakeLock(mojom::DisplayWakeLockType::DIM);
  EXPECT_EQ(0, GetActiveWakeLocks(WakeLockType::kPreventDisplaySleep));
  EXPECT_EQ(0,
            GetActiveWakeLocks(WakeLockType::kPreventDisplaySleepAllowDimming));
}

TEST_F(ArcPowerBridgeTest, ConsolidateWakeLocks) {
  AcquireDisplayWakeLock(mojom::DisplayWakeLockType::BRIGHT);
  EXPECT_EQ(1, GetActiveWakeLocks(WakeLockType::kPreventDisplaySleep));

  // Acquiring a second Android wake lock of the same time shouldn't result in a
  // second device service wake lock being requested.
  AcquireDisplayWakeLock(mojom::DisplayWakeLockType::BRIGHT);
  EXPECT_EQ(1, GetActiveWakeLocks(WakeLockType::kPreventDisplaySleep));

  ReleaseDisplayWakeLock(mojom::DisplayWakeLockType::BRIGHT);
  EXPECT_EQ(1, GetActiveWakeLocks(WakeLockType::kPreventDisplaySleep));

  // The device service wake lock should only be released when all Android wake
  // locks have been released.
  ReleaseDisplayWakeLock(mojom::DisplayWakeLockType::BRIGHT);
  EXPECT_EQ(0, GetActiveWakeLocks(WakeLockType::kPreventDisplaySleep));
}

TEST_F(ArcPowerBridgeTest, ReleaseWakeLocksWhenInstanceClosed) {
  AcquireDisplayWakeLock(mojom::DisplayWakeLockType::BRIGHT);
  ASSERT_EQ(1, GetActiveWakeLocks(WakeLockType::kPreventDisplaySleep));

  // If the instance is closed, all wake locks should be released.
  base::RunLoop run_loop;
  DestroyPowerInstance();
  run_loop.RunUntilIdle();
  EXPECT_EQ(0, GetActiveWakeLocks(WakeLockType::kPreventDisplaySleep));

  // Check that wake locks can be requested after the instance becomes ready
  // again.
  CreatePowerInstance();
  AcquireDisplayWakeLock(mojom::DisplayWakeLockType::BRIGHT);
  EXPECT_EQ(1, GetActiveWakeLocks(WakeLockType::kPreventDisplaySleep));
}

// Verifies that observer methods are called on power mode change.
TEST_F(ArcPowerBridgeTest, Observer) {
  TestObserver test_observer;
  power_bridge()->AddObserver(&test_observer);
  EXPECT_EQ(0, test_observer.GetUpdateCountAndReset());
  power_bridge()->OnWakefulnessChanged(mojom::WakefulnessMode::ASLEEP);
  EXPECT_EQ(mojom::WakefulnessMode::ASLEEP, test_observer.last_mode());
  EXPECT_EQ(1, test_observer.GetUpdateCountAndReset());
  // Observe is removed, no calls are expected.
  power_bridge()->RemoveObserver(&test_observer);
  power_bridge()->OnWakefulnessChanged(mojom::WakefulnessMode::AWAKE);
  EXPECT_EQ(mojom::WakefulnessMode::ASLEEP, test_observer.last_mode());
  EXPECT_EQ(0, test_observer.GetUpdateCountAndReset());
}

}  // namespace arc
