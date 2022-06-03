// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/device_activity/device_activity_client.h"

#include "base/test/task_environment.h"
#include "base/timer/mock_timer.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/network/network_state_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace device_activity {
namespace {

constexpr char kWifiServiceGuid[] = "wifi_guid";

}  // namespace

class MockDeviceActivityClient : public DeviceActivityClient {
 public:
  explicit MockDeviceActivityClient(NetworkStateHandler* handler)
      : DeviceActivityClient(handler) {}

  // DeviceActivityClient:
  std::unique_ptr<base::RepeatingTimer> ConstructReportTimer() override {
    return std::make_unique<base::MockRepeatingTimer>();
  }

  // TODO(hirthanan): Use method when the state machine flows complete, in order
  // to test state transitions.
  void FireTimer() {
    base::MockRepeatingTimer* mock_timer =
        static_cast<base::MockRepeatingTimer*>(GetReportTimer());
    if (mock_timer->IsRunning())
      mock_timer->Fire();
  }
};

class DeviceActivityClientTest : public testing::Test {
 public:
  DeviceActivityClientTest()
      : network_state_test_helper_(/*use_default_devices_and_services=*/false) {
  }

  DeviceActivityClientTest(const DeviceActivityClientTest&) = delete;
  DeviceActivityClientTest& operator=(const DeviceActivityClientTest&) = delete;
  ~DeviceActivityClientTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    CreateWifiNetworkConfig();

    device_activity_client_ = std::make_unique<MockDeviceActivityClient>(
        network_state_test_helper_.network_state_handler());
  }

  void TearDown() override {}

  void CreateWifiNetworkConfig() {
    ASSERT_TRUE(wifi_network_service_path_.empty());

    std::stringstream ss;
    ss << "{"
       << "  \"GUID\": \"" << kWifiServiceGuid << "\","
       << "  \"Type\": \"" << shill::kTypeWifi << "\","
       << "  \"State\": \"" << shill::kStateOffline << "\""
       << "}";

    wifi_network_service_path_ =
        network_state_test_helper_.ConfigureService(ss.str());
  }

  // |network_state| is a shill network state, e.g. "shill::kStateIdle".
  void SetWifiNetworkState(std::string network_state) {
    network_state_test_helper_.SetServiceProperty(wifi_network_service_path_,
                                                  shill::kStateProperty,
                                                  base::Value(network_state));
    base::RunLoop().RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  NetworkStateTestHelper network_state_test_helper_;
  std::unique_ptr<DeviceActivityClient> device_activity_client_;
  std::string wifi_network_service_path_;
};

TEST_F(DeviceActivityClientTest, DefaultStateIsIdle) {
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, TimerRuns) {
  EXPECT_TRUE(device_activity_client_->GetReportTimer()->IsRunning());
  SetWifiNetworkState(shill::kStateOnline);
  EXPECT_TRUE(device_activity_client_->GetReportTimer()->IsRunning());
  SetWifiNetworkState(shill::kStateOffline);
  EXPECT_TRUE(device_activity_client_->GetReportTimer()->IsRunning());
}

}  // namespace device_activity
}  // namespace ash
