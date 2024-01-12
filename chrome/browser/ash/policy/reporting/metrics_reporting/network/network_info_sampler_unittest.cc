// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_info_sampler.h"

#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace reporting {
namespace {

constexpr char kEid0[] = "1234";
constexpr char kEid1[] = "5678";
constexpr char kEthernetPath[] = "ethernet/path";
constexpr char kEthernetMac[] = "ethernet_mac";
constexpr char kWifiPath[] = "wifi/path";
constexpr char kWifiMac[] = "wifi_mac";
constexpr char kCellularPath[] = "cellular/path";
constexpr char kMeid[] = "12343";
constexpr char kImei[] = "5689";
constexpr char kIccid[] = "9876563";
constexpr char kMdn[] = "134345";

class NetworkInfoSamplerTest : public ::testing::Test {
 protected:
  NetworkInfoSamplerTest() = default;

  NetworkInfoSamplerTest(const NetworkInfoSamplerTest&) = delete;
  NetworkInfoSamplerTest& operator=(const NetworkInfoSamplerTest&) = delete;

  ~NetworkInfoSamplerTest() override = default;

  void SetUp() override {
    device_client_ = network_handler_test_helper_.device_test();
    device_client_->ClearDevices();
    base::RunLoop().RunUntilIdle();
  }

  raw_ptr<::ash::ShillDeviceClient::TestInterface, DanglingUntriaged>
      device_client_;

 private:
  base::test::TaskEnvironment task_environment_;

  ::ash::NetworkHandlerTestHelper network_handler_test_helper_;
};

TEST_F(NetworkInfoSamplerTest, AllTypes) {
  ::ash::HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
      dbus::ObjectPath("path0"), kEid0, true, 1);
  ::ash::HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
      dbus::ObjectPath("path1"), kEid1, true, 2);

  device_client_->AddDevice(kEthernetPath, shill::kTypeEthernet, "ethernet");
  device_client_->SetDeviceProperty(kEthernetPath, shill::kAddressProperty,
                                    base::Value(kEthernetMac),
                                    /*notify_changed=*/true);

  device_client_->AddDevice(kWifiPath, shill::kTypeWifi, "wifi");
  device_client_->SetDeviceProperty(kWifiPath, shill::kAddressProperty,
                                    base::Value(kWifiMac),
                                    /*notify_changed=*/true);

  device_client_->AddDevice(kCellularPath, shill::kTypeCellular, "cellular");
  device_client_->SetDeviceProperty(kCellularPath, shill::kMeidProperty,
                                    base::Value(kMeid),
                                    /*notify_changed=*/true);
  device_client_->SetDeviceProperty(kCellularPath, shill::kImeiProperty,
                                    base::Value(kImei),
                                    /*notify_changed=*/true);
  device_client_->SetDeviceProperty(kCellularPath, shill::kIccidProperty,
                                    base::Value(kIccid),
                                    /*notify_changed=*/true);
  device_client_->SetDeviceProperty(kCellularPath, shill::kMdnProperty,
                                    base::Value(kMdn),
                                    /*notify_changed=*/true);

  base::RunLoop().RunUntilIdle();

  MetricData result;
  NetworkInfoSampler sampler;
  sampler.MaybeCollect(
      base::BindLambdaForTesting([&](std::optional<MetricData> metric_data) {
        ASSERT_TRUE(metric_data.has_value());
        result = std::move(metric_data.value());
      }));

  ASSERT_TRUE(result.has_info_data());
  ASSERT_TRUE(result.info_data().has_networks_info());
  ASSERT_EQ(result.info_data().networks_info().network_interfaces_size(), 3);
  // Ethernet.
  EXPECT_EQ(result.info_data().networks_info().network_interfaces(0).type(),
            NetworkDeviceType::ETHERNET_DEVICE);
  EXPECT_EQ(
      result.info_data().networks_info().network_interfaces(0).mac_address(),
      kEthernetMac);
  EXPECT_EQ(
      result.info_data().networks_info().network_interfaces(0).device_path(),
      kEthernetPath);
  EXPECT_FALSE(
      result.info_data().networks_info().network_interfaces(0).has_meid());
  EXPECT_FALSE(
      result.info_data().networks_info().network_interfaces(0).has_imei());
  EXPECT_FALSE(
      result.info_data().networks_info().network_interfaces(0).has_iccid());
  EXPECT_FALSE(
      result.info_data().networks_info().network_interfaces(0).has_mdn());
  EXPECT_TRUE(
      result.info_data().networks_info().network_interfaces(0).eids().empty());
  // Wifi.
  EXPECT_EQ(result.info_data().networks_info().network_interfaces(1).type(),
            NetworkDeviceType::WIFI_DEVICE);
  EXPECT_EQ(
      result.info_data().networks_info().network_interfaces(1).mac_address(),
      kWifiMac);
  EXPECT_EQ(
      result.info_data().networks_info().network_interfaces(1).device_path(),
      kWifiPath);
  EXPECT_FALSE(
      result.info_data().networks_info().network_interfaces(1).has_meid());
  EXPECT_FALSE(
      result.info_data().networks_info().network_interfaces(1).has_imei());
  EXPECT_FALSE(
      result.info_data().networks_info().network_interfaces(1).has_iccid());
  EXPECT_FALSE(
      result.info_data().networks_info().network_interfaces(1).has_mdn());
  EXPECT_TRUE(
      result.info_data().networks_info().network_interfaces(1).eids().empty());
  // Cellular.
  EXPECT_EQ(result.info_data().networks_info().network_interfaces(2).type(),
            NetworkDeviceType::CELLULAR_DEVICE);
  EXPECT_EQ(result.info_data().networks_info().network_interfaces(2).meid(),
            kMeid);
  EXPECT_EQ(result.info_data().networks_info().network_interfaces(2).imei(),
            kImei);
  EXPECT_EQ(result.info_data().networks_info().network_interfaces(2).iccid(),
            kIccid);
  EXPECT_EQ(result.info_data().networks_info().network_interfaces(2).mdn(),
            kMdn);
  EXPECT_EQ(
      result.info_data().networks_info().network_interfaces(2).device_path(),
      kCellularPath);
  EXPECT_FALSE(result.info_data()
                   .networks_info()
                   .network_interfaces(2)
                   .has_mac_address());
  ASSERT_EQ(
      result.info_data().networks_info().network_interfaces(2).eids_size(), 2);
  EXPECT_EQ(result.info_data().networks_info().network_interfaces(2).eids(0),
            kEid0);
  EXPECT_EQ(result.info_data().networks_info().network_interfaces(2).eids(1),
            kEid1);
}

TEST_F(NetworkInfoSamplerTest, NoDevices) {
  bool callback_called = false;
  NetworkInfoSampler sampler;
  sampler.MaybeCollect(
      base::BindLambdaForTesting([&](std::optional<MetricData> metric_data) {
        ASSERT_FALSE(metric_data.has_value());
        callback_called = true;
      }));

  ASSERT_TRUE(callback_called);
}

}  // namespace
}  // namespace reporting
