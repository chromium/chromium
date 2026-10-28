// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/metrics/network_connectivity_metrics_service.h"

#include "ash/constants/ash_pref_names.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

class NetworkConnectivityMetricsServiceTest : public testing::Test {
 public:
  NetworkConnectivityMetricsServiceTest() = default;

  NetworkConnectivityMetricsServiceTest(
      const NetworkConnectivityMetricsServiceTest&) = delete;
  NetworkConnectivityMetricsServiceTest& operator=(
      const NetworkConnectivityMetricsServiceTest&) = delete;

  void SetUp() override {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }
  void TearDown() override {
    histogram_tester_.reset();
    TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->RemoveUserPref(
        ash::prefs::kKioskMetrics);
  }
  NetworkStateHandler* network_state_handler() {
    return network_state_test_helper_.network_state_handler();
  }
  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

  std::optional<int> GetNetworkDropsFromLocalState() {
    return TestingBrowserProcess::GetGlobal()
        ->local_state()
        ->GetDict(ash::prefs::kKioskMetrics)
        .FindInt(kKioskNetworkDrops);
  }

  void SimulateConnectionFailure(const NetworkState* network,
                                 std::string error) {
    SetNetworkProperty(network->path(), shill::kStateProperty,
                       shill::kStateConfiguration);

    SetNetworkProperty(network->path(), shill::kErrorProperty, error);
    SetNetworkProperty(network->path(), shill::kStateProperty,
                       shill::kStateFailure);
  }

  void SimulateConnectionSuccess(const NetworkState* network) {
    SetNetworkProperty(network->path(), shill::kStateProperty,
                       shill::kStateOnline);
  }

  const NetworkState* CreateNetwork() {
    std::string service_path =
        network_state_test_helper_.ConfigureWiFi(shill::kStateReady);
    return network_state_handler()->GetNetworkState(service_path);
  }

 private:
  void SetNetworkProperty(const std::string& service_path,
                          const std::string& key,
                          const std::string& value) {
    network_state_test_helper_.SetServiceProperty(service_path, key,
                                                  base::Value(value));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  NetworkStateTestHelper network_state_test_helper_{
      /*use_default_devices_and_services=*/false};
};

TEST_F(NetworkConnectivityMetricsServiceTest, StartNotInitialized) {
  auto service = std::make_unique<NetworkConnectivityMetricsService>(
      TestingBrowserProcess::GetGlobal()->local_state(),
      network_state_handler());
  EXPECT_TRUE(network_state_handler()->HasObserver(service.get()));
  EXPECT_FALSE(service->is_online());
  EXPECT_EQ(std::optional<int>(0), GetNetworkDropsFromLocalState());
}

TEST_F(NetworkConnectivityMetricsServiceTest, StartOnlineGoOnline) {
  const auto* network = CreateNetwork();
  SimulateConnectionSuccess(network);
  auto service = std::make_unique<NetworkConnectivityMetricsService>(
      TestingBrowserProcess::GetGlobal()->local_state(),
      network_state_handler());
  EXPECT_TRUE(network_state_handler()->HasObserver(service.get()));
  EXPECT_TRUE(service->is_online());
  EXPECT_EQ(std::optional<int>(0), GetNetworkDropsFromLocalState());

  // Nothing changes when go online from online.
  SimulateConnectionSuccess(network);
  EXPECT_TRUE(service->is_online());
  EXPECT_EQ(std::optional<int>(0), GetNetworkDropsFromLocalState());
}

TEST_F(NetworkConnectivityMetricsServiceTest, StartOnlineGoOfflineDrop) {
  const auto* network = CreateNetwork();
  SimulateConnectionSuccess(network);
  auto service = std::make_unique<NetworkConnectivityMetricsService>(
      TestingBrowserProcess::GetGlobal()->local_state(),
      network_state_handler());
  EXPECT_TRUE(network_state_handler()->HasObserver(service.get()));
  EXPECT_TRUE(service->is_online());
  EXPECT_EQ(std::optional<int>(0), GetNetworkDropsFromLocalState());

  // Network connectivity drop.
  SimulateConnectionFailure(network, shill::kErrorUnknownFailure);
  EXPECT_FALSE(service->is_online());
  EXPECT_EQ(std::optional<int>(1), GetNetworkDropsFromLocalState());
}

TEST_F(NetworkConnectivityMetricsServiceTest, StartOfflineGoOffline) {
  const auto* network = CreateNetwork();
  SimulateConnectionFailure(network, shill::kErrorUnknownFailure);

  auto service = std::make_unique<NetworkConnectivityMetricsService>(
      TestingBrowserProcess::GetGlobal()->local_state(),
      network_state_handler());
  EXPECT_TRUE(network_state_handler()->HasObserver(service.get()));
  EXPECT_FALSE(service->is_online());
  EXPECT_EQ(std::optional<int>(0), GetNetworkDropsFromLocalState());

  // Number of drops does not change when go offline from offline.
  SimulateConnectionFailure(network, shill::kErrorUnknownFailure);
  EXPECT_FALSE(service->is_online());
  EXPECT_EQ(std::optional<int>(0), GetNetworkDropsFromLocalState());
}

TEST_F(NetworkConnectivityMetricsServiceTest, StartOfflineGoOnline) {
  const auto* network = CreateNetwork();
  SimulateConnectionFailure(network, shill::kErrorUnknownFailure);

  auto service = std::make_unique<NetworkConnectivityMetricsService>(
      TestingBrowserProcess::GetGlobal()->local_state(),
      network_state_handler());
  EXPECT_TRUE(network_state_handler()->HasObserver(service.get()));
  EXPECT_FALSE(service->is_online());
  EXPECT_EQ(std::optional<int>(0), GetNetworkDropsFromLocalState());

  // Number of drops does not change when go online from offline.
  SimulateConnectionSuccess(network);
  EXPECT_TRUE(service->is_online());
  EXPECT_EQ(std::optional<int>(0), GetNetworkDropsFromLocalState());
}

TEST_F(NetworkConnectivityMetricsServiceTest, LogAndReportNetworkDrops) {
  constexpr size_t kMaxNetworkDrops = 5;

  auto service = std::make_unique<NetworkConnectivityMetricsService>(
      TestingBrowserProcess::GetGlobal()->local_state(),
      network_state_handler());
  EXPECT_TRUE(network_state_handler()->HasObserver(service.get()));

  const auto* network = CreateNetwork();
  // Disconnect / connect networks kMaxNetworkDrops times.
  for (size_t network_drops = 1; network_drops <= kMaxNetworkDrops;
       network_drops++) {
    SimulateConnectionSuccess(network);
    EXPECT_TRUE(service->is_online());
    EXPECT_EQ(std::optional<int>(network_drops - 1),
              GetNetworkDropsFromLocalState());
    SimulateConnectionFailure(network, shill::kErrorUnknownFailure);
    EXPECT_FALSE(service->is_online());
    EXPECT_EQ(std::optional<int>(network_drops),
              GetNetworkDropsFromLocalState());
  }

  // Check network-drops from Local State gets reported once the next kiosk
  // session starts.
  service = std::make_unique<NetworkConnectivityMetricsService>(
      TestingBrowserProcess::GetGlobal()->local_state(),
      network_state_handler());
  histogram_tester()->ExpectBucketCount(kKioskNetworkDropsPerSessionHistogram,
                                        kMaxNetworkDrops, 1);
}

}  // namespace ash
