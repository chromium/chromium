// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_throttling_observer.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace test {

class NetworkThrottlingObserverTest : public ::testing::Test {
 public:
  NetworkThrottlingObserverTest() {
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    local_state_->registry()->RegisterDictionaryPref(
        prefs::kNetworkThrottlingEnabled);
    observer_ = std::make_unique<NetworkThrottlingObserver>(local_state_.get());
  }

  NetworkThrottlingObserverTest(const NetworkThrottlingObserverTest&) = delete;
  NetworkThrottlingObserverTest& operator=(
      const NetworkThrottlingObserverTest&) = delete;

  ~NetworkThrottlingObserverTest() override {
    observer_.reset();
    local_state_.reset();
  }

  TestingPrefServiceSimple* local_state() { return local_state_.get(); }

  const ShillManagerClient::NetworkThrottlingStatus&
  GetNetworkThrottlingStatus() {
    return network_handler_test_helper_.manager_test()
        ->GetNetworkThrottlingStatus();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  NetworkHandlerTestHelper network_handler_test_helper_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  std::unique_ptr<NetworkThrottlingObserver> observer_;
};

TEST_F(NetworkThrottlingObserverTest, ThrottlingChangeCallsShill) {
  // Test that a change in the throttling policy value leads to
  // shill_manager_client being called.
  constexpr bool enabled = true;
  constexpr uint32_t upload_rate = 1200;
  constexpr uint32_t download_rate = 2000;
  auto updated_throttling_policy =
      base::Value::Dict()
          .Set("enabled", enabled)
          .Set("upload_rate_kbits", static_cast<int>(upload_rate))
          .Set("download_rate_kbits", static_cast<int>(download_rate));

  // Make sure throttling is disabled just before setting preferece.
  EXPECT_FALSE(GetNetworkThrottlingStatus().enabled);

  // Setting the preference should update the network throttling.
  local_state()->SetDict(prefs::kNetworkThrottlingEnabled,
                         std::move(updated_throttling_policy));
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
}  // namespace ash
