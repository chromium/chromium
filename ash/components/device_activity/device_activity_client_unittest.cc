// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/device_activity/device_activity_client.h"

#include "ash/components/device_activity/device_activity_controller.h"
#include "ash/components/device_activity/fresnel_pref_names.h"
#include "base/test/task_environment.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/network/network_state_test_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash {
namespace device_activity {
namespace {

constexpr char kWifiServiceGuid[] = "wifi_guid";

constexpr char kFakePsmDeviceActiveSecret[] = "FAKE_PSM_DEVICE_ACTIVE_SECRET";

}  // namespace

class MockDeviceActivityClient : public DeviceActivityClient {
 public:
  MockDeviceActivityClient(
      NetworkStateHandler* handler,
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : DeviceActivityClient(handler,
                             local_state,
                             url_loader_factory,
                             kFakePsmDeviceActiveSecret) {}

  MOCK_METHOD(void, TransitionToHealthCheck, (), (override));
  MOCK_METHOD(void, TransitionToCheckMembershipOprf, (), (override));
  MOCK_METHOD(
      void,
      TransitionToCheckMembershipQuery,
      (const private_membership::rlwe::PrivateMembershipRlweOprfResponse&
           oprf_response),
      (override));
  MOCK_METHOD(void, TransitionToCheckIn, (), (override));
};

class DeviceActivityClientTest : public testing::Test {
 public:
  DeviceActivityClientTest() = default;
  DeviceActivityClientTest(const DeviceActivityClientTest&) = delete;
  DeviceActivityClientTest& operator=(const DeviceActivityClientTest&) = delete;
  ~DeviceActivityClientTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    network_state_test_helper_ = std::make_unique<NetworkStateTestHelper>(
        /*use_default_devices_and_services=*/false);

    CreateWifiNetworkConfig();

    // Initialize local state prefs used by device_activity_client class.
    DeviceActivityController::RegisterPrefs(local_state_.registry());

    shared_url_loader_factory_ =
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>();

    device_activity_client_ = std::make_unique<MockDeviceActivityClient>(
        network_state_test_helper_->network_state_handler(), &local_state_,
        shared_url_loader_factory_);
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
        network_state_test_helper_->ConfigureService(ss.str());
  }

  // |network_state| is a shill network state, e.g. "shill::kStateIdle".
  void SetWifiNetworkState(std::string network_state) {
    network_state_test_helper_->SetServiceProperty(wifi_network_service_path_,
                                                   shill::kStateProperty,
                                                   base::Value(network_state));
    base::RunLoop().RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<NetworkStateTestHelper> network_state_test_helper_;
  TestingPrefServiceSimple local_state_;
  scoped_refptr<network::TestSharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<DeviceActivityClient> device_activity_client_;
  std::string wifi_network_service_path_;
};

TEST_F(DeviceActivityClientTest, DefaultStateIsIdle) {
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, DefaultTimeUnixEpoch) {
  EXPECT_EQ(
      local_state_.GetTime(prefs::kDeviceActiveLastKnownDailyPingTimestamp),
      base::Time::UnixEpoch());
}

TEST_F(DeviceActivityClientTest, DefaultTimerIsRunning) {
  EXPECT_TRUE(device_activity_client_->GetReportTimer()->IsRunning());
}

}  // namespace device_activity
}  // namespace ash
