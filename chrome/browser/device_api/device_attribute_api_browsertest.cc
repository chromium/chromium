// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_api/device_attribute_api.h"

#include "base/test/gtest_tags.h"
#include "build/chromeos_buildflags.h"
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
    fake_statistics_provider_.SetMachineStatistic(
        ash::system::kSerialNumberKeyForTest, std::string());
  }

 private:
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

IN_PROC_BROWSER_TEST_F(DeviceAttributeAPIUnsetTest, AllAttributes) {
  device_attribute_api::GetDirectoryId(
      base::BindOnce([](blink::mojom::DeviceAttributeResultPtr result) {
        EXPECT_FALSE(result->get_attribute().has_value());
      }));

  device_attribute_api::GetAnnotatedAssetId(
      base::BindOnce([](blink::mojom::DeviceAttributeResultPtr result) {
        EXPECT_FALSE(result->get_attribute().has_value());
      }));

  device_attribute_api::GetAnnotatedLocation(
      base::BindOnce([](blink::mojom::DeviceAttributeResultPtr result) {
        EXPECT_FALSE(result->get_attribute().has_value());
      }));

  device_attribute_api::GetSerialNumber(
      base::BindOnce([](blink::mojom::DeviceAttributeResultPtr result) {
        EXPECT_FALSE(result->get_attribute().has_value());
      }));

  device_attribute_api::GetHostname(
      base::BindOnce([](blink::mojom::DeviceAttributeResultPtr result) {
        EXPECT_FALSE(result->get_attribute().has_value());
      }));

  base::RunLoop().RunUntilIdle();
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
    fake_statistics_provider_.SetMachineStatistic(
        ash::system::kSerialNumberKeyForTest, kSerialNumber);
  }

 private:
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

IN_PROC_BROWSER_TEST_F(DeviceAttributeAPITest, AllAttributes) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-163d36ff-e640-48e1-a451-03e14c9e8874");

  device_attribute_api::GetDirectoryId(
      base::BindOnce([](blink::mojom::DeviceAttributeResultPtr result) {
        EXPECT_EQ(result->get_attribute(), kDirectoryApiId);
      }));

  device_attribute_api::GetAnnotatedAssetId(
      base::BindOnce([](blink::mojom::DeviceAttributeResultPtr result) {
        EXPECT_EQ(result->get_attribute(), kAnnotatedAssetId);
      }));

  device_attribute_api::GetAnnotatedLocation(
      base::BindOnce([](blink::mojom::DeviceAttributeResultPtr result) {
        EXPECT_EQ(result->get_attribute(), kAnnotatedLocation);
      }));

  device_attribute_api::GetHostname(
      base::BindOnce([](blink::mojom::DeviceAttributeResultPtr result) {
        EXPECT_EQ(result->get_attribute(), kHostname);
      }));

  device_attribute_api::GetSerialNumber(
      base::BindOnce([](blink::mojom::DeviceAttributeResultPtr result) {
        EXPECT_EQ(result->get_attribute(), kSerialNumber);
      }));

  base::RunLoop().RunUntilIdle();
}
