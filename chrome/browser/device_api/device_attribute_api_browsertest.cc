// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_api/device_attribute_api.h"

#include "base/test/gtest_tags.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "content/public/test/browser_test.h"

namespace {

constexpr char kAnnotatedAssetId[] = "annotated_asset_id";
constexpr char kAnnotatedLocation[] = "annotated_location";
constexpr char kDirectoryApiId[] = "directory_api_id";
constexpr char kHostname[] = "hostname";
constexpr char kSerialNumber[] = "serial_number";

}  // namespace

// This test class provides unset device policy values and statistic data used
// by device attributes APIs.
class DeviceAttributeAPIUnsetTest : public policy::DevicePolicyCrosBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();

    // Init machine statistic.
    fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                  std::string());
  }

  DeviceAttributeApi& device_attributes_api() { return device_attributes_api_; }

 private:
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  DeviceAttributeApiImpl device_attributes_api_;
};

IN_PROC_BROWSER_TEST_F(DeviceAttributeAPIUnsetTest, AllAttributes) {
  base::test::TestFuture<blink::mojom::DeviceAttributeResultPtr> future;

  device_attributes_api().GetDirectoryId(future.GetCallback());
  EXPECT_FALSE(future.Take()->get_attribute().has_value());

  device_attributes_api().GetAnnotatedAssetId(future.GetCallback());
  EXPECT_FALSE(future.Take()->get_attribute().has_value());

  device_attributes_api().GetAnnotatedLocation(future.GetCallback());
  EXPECT_FALSE(future.Take()->get_attribute().has_value());

  device_attributes_api().GetSerialNumber(future.GetCallback());
  EXPECT_FALSE(future.Take()->get_attribute().has_value());

  device_attributes_api().GetHostname(future.GetCallback());
  EXPECT_FALSE(future.Take()->get_attribute().has_value());
}

// This test class provides regular device policy values and statistic data used
// by device attributes APIs.
class DeviceAttributeAPITest : public policy::DevicePolicyCrosBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();

    // Init the device policy.
    device_policy()->SetDefaultSigningKey();
    device_policy()->policy_data().set_annotated_asset_id(kAnnotatedAssetId);
    device_policy()->policy_data().set_annotated_location(kAnnotatedLocation);
    device_policy()->policy_data().set_directory_api_id(kDirectoryApiId);
    enterprise_management::NetworkHostnameProto* proto =
        device_policy()->payload().mutable_network_hostname();
    proto->set_device_hostname_template(kHostname);
    device_policy()->Build();
    RefreshDevicePolicy();

    // Init machine statistic.
    fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                  kSerialNumber);
  }

  DeviceAttributeApi& device_attributes_api() { return device_attributes_api_; }

 private:
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  DeviceAttributeApiImpl device_attributes_api_;
};

IN_PROC_BROWSER_TEST_F(DeviceAttributeAPITest, AllAttributes) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-163d36ff-e640-48e1-a451-03e14c9e8874");

  base::test::TestFuture<blink::mojom::DeviceAttributeResultPtr> future;

  device_attributes_api().GetDirectoryId(future.GetCallback());
  EXPECT_EQ(future.Take()->get_attribute(), kDirectoryApiId);

  device_attributes_api().GetAnnotatedAssetId(future.GetCallback());
  EXPECT_EQ(future.Take()->get_attribute(), kAnnotatedAssetId);

  device_attributes_api().GetAnnotatedLocation(future.GetCallback());
  EXPECT_EQ(future.Take()->get_attribute(), kAnnotatedLocation);

  device_attributes_api().GetHostname(future.GetCallback());
  EXPECT_EQ(future.Take()->get_attribute(), kHostname);

  device_attributes_api().GetSerialNumber(future.GetCallback());
  EXPECT_EQ(future.Take()->get_attribute(), kSerialNumber);
}
