// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/pcie_peripheral/pcie_peripheral_manager.h"

#include <memory>

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
    PciePeripheralManager::Initialize(/*is_guest_session=*/false,
                                      /*is_pci_tunneling_allowed=*/false);
    manager_ = PciePeripheralManager::Get();

    manager_->AddObserver(&fake_observer_);
  }

  void TearDown() override {
    manager_->RemoveObserver(&fake_observer_);
    PciePeripheralManager::Shutdown();
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

 private:
  PciePeripheralManager* manager_ = nullptr;
  FakeObserver fake_observer_;
};

// TODO(jimmyxgong): Add more tests once the necessary D-bus clients are
// available.
TEST_F(PciePeripheralManagerTest, InitialTest) {
  EXPECT_EQ(0u, GetNumLimitedPerformanceObserverCalls());
  EXPECT_EQ(0u, GetNumGuestModeNotificationObserverCalls());
  EXPECT_FALSE(GetIsCurrentGuestDeviceTbtOnly());
}

}  // namespace ash
