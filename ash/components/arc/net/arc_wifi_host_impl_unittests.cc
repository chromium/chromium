// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/net/arc_wifi_host_impl.h"

#include <string>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/fake_network_state_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace arc {
namespace {
class ArcWifiHostImplTest : public testing::Test {
 public:
  ArcWifiHostImplTest(const ArcWifiHostImplTest&) = delete;
  ArcWifiHostImplTest& operator=(const ArcWifiHostImplTest&) = delete;

  ash::FakeNetworkStateHandler* GetStateHandler() {
    return static_cast<ash::FakeNetworkStateHandler*>(
        ash::NetworkHandler::Get()->network_state_handler());
  }

 protected:
  ArcWifiHostImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~ArcWifiHostImplTest() override = default;

  void SetUp() override {
    // Required for initializing NetworkHandler.
    ash::HermesProfileClient::InitializeFake();
    ash::HermesManagerClient::InitializeFake();
    ash::HermesEuiccClient::InitializeFake();

    // Required for initializingFakeShillManagerClient.
    ash::shill_clients::InitializeFakes();
    ash::ShillManagerClient::Get()
        ->GetTestInterface()
        ->SetupDefaultEnvironment();

    ash::NetworkHandler::Initialize();

    bridge_service_ = std::make_unique<ArcBridgeService>();
    service_ =
        std::make_unique<ArcWifiHostImpl>(nullptr, bridge_service_.get());
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    service_->Shutdown();
    ash::NetworkHandler::Shutdown();
    ash::HermesEuiccClient::Shutdown();
    ash::HermesManagerClient::Shutdown();
    ash::HermesProfileClient::Shutdown();
    ash::shill_clients::Shutdown();
  }

  ArcWifiHostImpl* service() { return service_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ArcBridgeService> bridge_service_;
  std::unique_ptr<ArcWifiHostImpl> service_;
};

TEST_F(ArcWifiHostImplTest, ToggleWifiEnabledState) {
  base::test::TestFuture<bool> future;
  // Enable WiFi and get WiFi state.
  service()->SetWifiEnabledState(true, future.GetCallback());
  ASSERT_TRUE(future.Take());
  service()->GetWifiEnabledState(future.GetCallback());
  ASSERT_TRUE(future.Take());

  // Disable WiFi and get WiFi state.
  service()->SetWifiEnabledState(false, future.GetCallback());
  ASSERT_TRUE(future.Take());
  service()->GetWifiEnabledState(future.GetCallback());
  ASSERT_FALSE(future.Take());
}

TEST_F(ArcWifiHostImplTest, OnConnectionReadyAndClosed) {
  GetStateHandler()->ClearObserverList();
  EXPECT_TRUE(GetStateHandler()->ObserverListEmpty());
  service()->OnConnectionReady();
  EXPECT_FALSE(GetStateHandler()->ObserverListEmpty());
  service()->OnConnectionClosed();
  EXPECT_TRUE(GetStateHandler()->ObserverListEmpty());
}

TEST_F(ArcWifiHostImplTest, OnShuttingDown) {
  GetStateHandler()->ClearObserverList();
  EXPECT_TRUE(GetStateHandler()->ObserverListEmpty());
  service()->OnConnectionReady();
  EXPECT_FALSE(GetStateHandler()->ObserverListEmpty());
  service()->OnShuttingDown();
  EXPECT_TRUE(GetStateHandler()->ObserverListEmpty());
}
}  // namespace
}  // namespace arc
