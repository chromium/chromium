// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/pcie_peripheral/pcie_peripheral_manager.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chromeos/dbus/typecd/fake_typecd_client.h"
#include "chromeos/dbus/typecd/typecd_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class FakeObserver : public PciePeripheralManager::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_limited_performance_notification_calls() const {
    return num_limited_performance_notification_calls_;
  }

  size_t num_guest_notification_calls() const {
    return num_guest_notification_calls_;
  }

  bool is_current_guest_device_tbt_only() const {
    return is_current_guest_device_tbt_only_;
  }

  // PciePeripheralManager::Observer:
  void OnLimitedPerformancePeripheralReceived() override {
    ++num_limited_performance_notification_calls_;
  }

  void OnGuestModeNotificationReceived(bool is_thunderbolt_only) override {
    is_current_guest_device_tbt_only_ = is_thunderbolt_only;
    ++num_guest_notification_calls_;
  }

 private:
  size_t num_limited_performance_notification_calls_ = 0u;
  size_t num_guest_notification_calls_ = 0u;
  bool is_current_guest_device_tbt_only_ = false;
};

class PciePeripheralManagerTest : public testing::Test {
 protected:
  PciePeripheralManagerTest() = default;
  PciePeripheralManagerTest(const PciePeripheralManagerTest&) = delete;
  PciePeripheralManagerTest& operator=(const PciePeripheralManagerTest&) =
      delete;
  ~PciePeripheralManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    chromeos::TypecdClient::InitializeFake();
    fake_typecd_client_ =
        static_cast<chromeos::FakeTypecdClient*>(chromeos::TypecdClient::Get());
  }

  void InitializeManager(bool is_guest_session,
                         bool is_pcie_tunneling_allowed) {
    PciePeripheralManager::Initialize(is_guest_session,
                                      is_pcie_tunneling_allowed);
    manager_ = PciePeripheralManager::Get();

    manager_->AddObserver(&fake_observer_);
  }

  void TearDown() override {
    manager_->RemoveObserver(&fake_observer_);
    PciePeripheralManager::Shutdown();
    chromeos::TypecdClient::Shutdown();
  }

  chromeos::FakeTypecdClient* fake_typecd_client() {
    return fake_typecd_client_;
  }

  size_t GetNumLimitedPerformanceObserverCalls() {
    return fake_observer_.num_limited_performance_notification_calls();
  }

  size_t GetNumGuestModeNotificationObserverCalls() {
    return fake_observer_.num_guest_notification_calls();
  }

  bool GetIsCurrentGuestDeviceTbtOnly() {
    return fake_observer_.is_current_guest_device_tbt_only();
  }

  base::HistogramTester histogram_tester_;

 private:
  chromeos::FakeTypecdClient* fake_typecd_client_;
  PciePeripheralManager* manager_ = nullptr;
  FakeObserver fake_observer_;
};

TEST_F(PciePeripheralManagerTest, InitialTest) {
  InitializeManager(/*is_guest_profile=*/false,
                    /*is_pcie_tunneling_allowed=*/false);
  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
}

TEST_F(PciePeripheralManagerTest, LimitedPerformanceNotification) {
  InitializeManager(/*is_guest_profile=*/false,
                    /*is_pcie_tunneling_allowed=*/false);

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kAltModeFallbackDueToPciguard,
      0);

  // Simulate emitting D-Bus signal.
  fake_typecd_client()->EmitThunderboltDeviceConnectedSignal(
      /*is_thunderbolt_only=*/false);
  // pcie tunneling is not allowed and a alt-mode device has been plugged in.
  // Expect the notification observer to be called.
  EXPECT_EQ(1u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kAltModeFallbackDueToPciguard,
      1);
}

TEST_F(PciePeripheralManagerTest, NoNotificationShown) {
  InitializeManager(/*is_guest_profile=*/false,
                    /*is_pcie_tunneling_allowed=*/true);

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kTBTSupportedAndAllowed,
      0);

  // Simulate emitting D-Bus signal.
  fake_typecd_client()->EmitThunderboltDeviceConnectedSignal(
      /*is_thunderbolt_only=*/false);
  // Pcie tunneling allowed, we do not show any notifications for this case.
  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kTBTSupportedAndAllowed,
      1);

  // Simulate emitting a new D-Bus signal, this time with |is_thunderbolt_only|
  // set to true.
  fake_typecd_client()->EmitThunderboltDeviceConnectedSignal(
      /*is_thunderbolt_only=*/true);
  // Pcie tunneling allowed, we do not show any notifications for this case.
  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  // No observer was called, therefore don't expect this to be updated.
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kTBTSupportedAndAllowed,
      2);
}

TEST_F(PciePeripheralManagerTest, TBTOnlyAndBlockedByPciguard) {
  InitializeManager(/*is_guest_profile=*/false,
                    /*is_pcie_tunneling_allowed=*/false);

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kTBTOnlyAndBlockedByPciguard,
      0);

  // Simulate emitting D-Bus signal.
  fake_typecd_client()->EmitThunderboltDeviceConnectedSignal(
      /*is_thunderbolt_only=*/true);
  // Pcie tunneling allowed, we do not show any notifications for this case.
  EXPECT_EQ(1u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kTBTOnlyAndBlockedByPciguard,
      1);
}

TEST_F(PciePeripheralManagerTest, GuestNotificationLimitedPerformance) {
  InitializeManager(/*is_guest_profile=*/true,
                    /*is_pcie_tunneling_allowed=*/false);

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kAltModeFallbackInGuestSession,
      0);

  // Simulate emitting D-Bus signal.
  fake_typecd_client()->EmitThunderboltDeviceConnectedSignal(
      /*is_thunderbolt_only=*/false);
  // Pcie tunneling not allowed and user is in guest session. The device
  // supports an alt-mode, expect the notification observer to be called.
  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(1u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kAltModeFallbackInGuestSession,
      1);
}

TEST_F(PciePeripheralManagerTest, GuestNotificationRestricted) {
  InitializeManager(/*is_guest_profile=*/true,
                    /*is_pcie_tunneling_allowed=*/false);

  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kTBTOnlyAndBlockedInGuestSession,
      0);

  // Simulate emitting D-Bus signal.
  fake_typecd_client()->EmitThunderboltDeviceConnectedSignal(
      /*is_thunderbolt_only=*/true);
  // Pcie tunneling not allowed and user is in guest session. The device
  // does not support alt-mode, expect the notification observer to be called.
  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(1u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_TRUE(GetIsCurrentGuestDeviceTbtOnly());
  histogram_tester_.ExpectBucketCount(
      "Ash.PciePeripheral.ConnectivityResults",
      PciePeripheralManager::PciePeripheralConnectivityResults::
          kTBTOnlyAndBlockedInGuestSession,
      1);
}

}  // namespace ash
