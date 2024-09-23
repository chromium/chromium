// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_cloud_policy_client_factory_ash.h"

#include "base/test/task_environment.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

constexpr char kSerialNumberValue[] = "a_serial_value";
constexpr char kHardwareClassValue[] = "a_hardware_class";
constexpr char kBrandCodeValue[] = "a_brand_code";
constexpr char kAttestedDeviceIdValue[] = "an_attested_device_id";
constexpr char kEthernetMacAddressValidValue[] = "00:01:02:03:04:05";
constexpr char kEthernetMacAddressValidParsedValue[] = "000102030405";
constexpr char kEthernetMacAddressInvalidValue[] = "00|01|02|03|04|05";
constexpr char kDockMacAddressValidValue[] = "AA:BB:CC:DD:EE:FF";
constexpr char kDockMacAddressValidParsedValue[] = "AABBCCDDEEFF";
constexpr char kDockMacAddressInvalidValue[] = "AA:NO:CC:DD:EE:FF";
constexpr char kManufactureDateValue[] = "a_manufacture_date_value";

class DeviceCloudPolicyClientFactoryAshTest : public testing::Test {
 protected:
  // Required by `fake_service_`.
  base::test::SingleThreadTaskEnvironment task_environment_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  testing::StrictMock<MockJobCreationHandler> job_creation_handler_;
  FakeDeviceManagementService fake_service_{&job_creation_handler_};
  network::TestURLLoaderFactory fake_url_loader_factory_;
};

TEST_F(DeviceCloudPolicyClientFactoryAshTest, ValidStatistics) {
  fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                kSerialNumberValue);
  fake_statistics_provider_.SetMachineStatistic(ash::system::kHardwareClassKey,
                                                kHardwareClassValue);
  fake_statistics_provider_.SetMachineStatistic(ash::system::kRlzBrandCodeKey,
                                                kBrandCodeValue);
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kAttestedDeviceIdKey, kAttestedDeviceIdValue);
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kEthernetMacAddressKey, kEthernetMacAddressValidValue);
  fake_statistics_provider_.SetMachineStatistic(ash::system::kDockMacAddressKey,
                                                kDockMacAddressValidValue);
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kManufactureDateKey, kManufactureDateValue);

  std::unique_ptr<CloudPolicyClient> client = CreateDeviceCloudPolicyClientAsh(
      &fake_statistics_provider_, &fake_service_,
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &fake_url_loader_factory_),
      CloudPolicyClient::DeviceDMTokenCallback());

  EXPECT_EQ(client->machine_id(), kSerialNumberValue);
  EXPECT_EQ(client->machine_model(), kHardwareClassValue);
  EXPECT_EQ(client->brand_code(), kBrandCodeValue);
  EXPECT_EQ(client->attested_device_id(), kAttestedDeviceIdValue);
  EXPECT_EQ(client->ethernet_mac_address(),
            kEthernetMacAddressValidParsedValue);
  EXPECT_EQ(client->dock_mac_address(), kDockMacAddressValidParsedValue);
  EXPECT_EQ(client->manufacture_date(), kManufactureDateValue);
}

TEST_F(DeviceCloudPolicyClientFactoryAshTest, InvalidStatistics) {
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kEthernetMacAddressKey, kEthernetMacAddressInvalidValue);
  fake_statistics_provider_.SetMachineStatistic(ash::system::kDockMacAddressKey,
                                                kDockMacAddressInvalidValue);

  std::unique_ptr<CloudPolicyClient> client = CreateDeviceCloudPolicyClientAsh(
      &fake_statistics_provider_, &fake_service_,
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &fake_url_loader_factory_),
      CloudPolicyClient::DeviceDMTokenCallback());

  EXPECT_TRUE(client->machine_id().empty());
  EXPECT_TRUE(client->machine_model().empty());
  EXPECT_TRUE(client->brand_code().empty());
  EXPECT_TRUE(client->attested_device_id().empty());
  EXPECT_TRUE(client->ethernet_mac_address().empty());
  EXPECT_TRUE(client->dock_mac_address().empty());
  EXPECT_TRUE(client->manufacture_date().empty());
}

}  // namespace policy
