// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/net/network_throttling_observer.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/network/network_state_handler.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace test {

class NetworkThrottlingObserverTest : public ::testing::Test {
 public:
  NetworkThrottlingObserverTest() {
    DBusThreadManager::Initialize();
    network_state_handler_ = NetworkStateHandler::InitializeForTest();
    NetworkHandler::Initialize();
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    local_state_->registry()->RegisterDictionaryPref(
        prefs::kNetworkThrottlingEnabled);
    observer_ = std::make_unique<NetworkThrottlingObserver>(local_state_.get());
  }

  ~NetworkThrottlingObserverTest() override {
    network_state_handler_->Shutdown();
    observer_.reset();
    local_state_.reset();
    network_state_handler_.reset();
    NetworkHandler::Shutdown();
    DBusThreadManager::Shutdown();
  }

  TestingPrefServiceSimple* local_state() { return local_state_.get(); }

  const ShillManagerClient::NetworkThrottlingStatus&
  GetNetworkThrottlingStatus() {
    return DBusThreadManager::Get()
        ->GetShillManagerClient()
        ->GetTestInterface()
        ->GetNetworkThrottlingStatus();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  std::unique_ptr<NetworkThrottlingObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(NetworkThrottlingObserverTest);
};

TEST_F(NetworkThrottlingObserverTest, ThrottlingChangeCallsShill) {
  // Test that a change in the throttling policy value leads to
  // shill_manager_client being called.
  base::DictionaryValue updated_throttling_policy;
  constexpr bool enabled = true;
  constexpr uint32_t upload_rate = 1200;
  constexpr uint32_t download_rate = 2000;
  updated_throttling_policy.SetBoolean("enabled", enabled);
  updated_throttling_policy.SetInteger("upload_rate_kbits", upload_rate);
  updated_throttling_policy.SetInteger("download_rate_kbits", download_rate);

  // Make sure throttling is disabled just before setting preferece.
  EXPECT_FALSE(GetNetworkThrottlingStatus().enabled);

  // Setting the preference should update the network throttling.
  local_state()->Set(prefs::kNetworkThrottlingEnabled,
                     updated_throttling_policy);
  base::RunLoop().RunUntilIdle();
  {
    const auto& status = GetNetworkThrottlingStatus();
    EXPECT_TRUE(status.enabled);
    EXPECT_EQ(upload_rate, status.upload_rate_kbits);
    EXPECT_EQ(download_rate, status.download_rate_kbits);
  }

  // Clearing the preference should disable throttling
  local_state()->ClearPref(prefs::kNetworkThrottlingEnabled);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetNetworkThrottlingStatus().enabled);
}

}  // namespace test
}  // namespace chromeos
