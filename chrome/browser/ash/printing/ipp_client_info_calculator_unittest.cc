// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/ipp_client_info_calculator.h"

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/core/device_attributes_fake.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "printing/mojom/print.mojom-forward.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::printing {

namespace {

using ::printing::mojom::IppClientInfo;
using ::printing::mojom::IppClientInfoPtr;

constexpr char kLsbRelease[] =
    "CHROMEOS_RELEASE_NAME=Chrome OS\n"
    "CHROMEOS_RELEASE_VERSION=15183.69.3\n";

void ExpectClientInfoEqual(const IppClientInfo& lhs, const IppClientInfo& rhs) {
  EXPECT_EQ(lhs.client_name, rhs.client_name);
  EXPECT_EQ(lhs.client_type, rhs.client_type);
  EXPECT_EQ(lhs.client_patches, rhs.client_patches);
  EXPECT_EQ(lhs.client_string_version, rhs.client_string_version);
  EXPECT_EQ(lhs.client_version, rhs.client_version);
}

// Test fixture to test `IppClientInfoCalculator` with fake ChromeOS version,
// device attributes and `DevicePrintingClientNameTemplate` policy.
class IppClientInfoCalculatorTest : public testing::Test {
 public:
  IppClientInfoCalculatorTest() {
    auto fake_device_attibutes =
        std::make_unique<policy::FakeDeviceAttributes>();
    device_attributes_ = fake_device_attibutes.get();
    client_info_calculator_ = IppClientInfoCalculator::CreateForTesting(
        std::move(fake_device_attibutes), "42");
  }

  void SetClientNameTemplatePolicy(std::string_view value) {
    testing_cros_settings_.device_settings()->Set(
        kDevicePrintingClientNameTemplate, base::Value(value));
  }

  policy::FakeDeviceAttributes* DeviceAttributes() {
    return device_attributes_;
  }

  IppClientInfoPtr GetOsInfo() const {
    return client_info_calculator_->GetOsInfo();
  }

  IppClientInfoPtr GetDeviceInfo() const {
    return client_info_calculator_->GetDeviceInfo();
  }

 private:
  base::test::ScopedChromeOSVersionInfo cros_version_info_{kLsbRelease,
                                                           base::Time()};
  ScopedTestingCrosSettings testing_cros_settings_;
  raw_ptr<policy::FakeDeviceAttributes, DanglingUntriaged> device_attributes_;
  std::unique_ptr<IppClientInfoCalculator> client_info_calculator_;
};

TEST_F(IppClientInfoCalculatorTest, OsInfo) {
  const IppClientInfoPtr os_info = GetOsInfo();
  const IppClientInfo expected(IppClientInfo::ClientType::kOperatingSystem,
                               "ChromeOS", "15183.69.3", "42", std::nullopt);
  ASSERT_TRUE(os_info);
  ExpectClientInfoEqual(*os_info, expected);
}

TEST_F(IppClientInfoCalculatorTest, DeviceInfoUnsetPolicy) {
  const IppClientInfoPtr device_info = GetDeviceInfo();
  ASSERT_FALSE(device_info);
}

TEST_F(IppClientInfoCalculatorTest, DeviceInfoSimplePolicy) {
  SetClientNameTemplatePolicy("chromebook");

  const IppClientInfoPtr device_info = GetDeviceInfo();
  const IppClientInfo expected(IppClientInfo::ClientType::kOther, "chromebook",
                               std::nullopt, "", std::nullopt);
  ASSERT_TRUE(device_info);
  ExpectClientInfoEqual(*device_info, expected);
}

TEST_F(IppClientInfoCalculatorTest, DeviceInfoPolicyWithVariables) {
  DeviceAttributes()->SetFakeDeviceAssetId("asset-id");
  DeviceAttributes()->SetFakeDirectoryApiId("1234-abcd");
  DeviceAttributes()->SetFakeDeviceSerialNumber("serial");
  DeviceAttributes()->SetFakeDeviceAnnotatedLocation("location");
  SetClientNameTemplatePolicy(
      "chromebook_${DEVICE_ASSET_ID}_${DEVICE_DIRECTORY_API_ID}_${DEVICE_"
      "SERIAL_NUMBER}_${DEVICE_ANNOTATED_LOCATION}");

  const IppClientInfoPtr device_info = GetDeviceInfo();
  const IppClientInfo expected(IppClientInfo::ClientType::kOther,
                               "chromebook_asset-id_1234-abcd_serial_location",
                               std::nullopt, "", std::nullopt);
  ASSERT_TRUE(device_info);
  ExpectClientInfoEqual(*device_info, expected);
}

TEST_F(IppClientInfoCalculatorTest, DeviceInfoPolicyChange) {
  {
    const IppClientInfoPtr device_info = GetDeviceInfo();
    ASSERT_FALSE(device_info);
  }
  {
    SetClientNameTemplatePolicy("initial");
    const IppClientInfoPtr device_info = GetDeviceInfo();
    const IppClientInfo expected(IppClientInfo::ClientType::kOther, "initial",
                                 std::nullopt, "", std::nullopt);
    ASSERT_TRUE(device_info);
    ExpectClientInfoEqual(*device_info, expected);
  }

  {
    SetClientNameTemplatePolicy("changed");
    const IppClientInfoPtr device_info = GetDeviceInfo();
    const IppClientInfo expected(IppClientInfo::ClientType::kOther, "changed",
                                 std::nullopt, "", std::nullopt);
    ASSERT_TRUE(device_info);
    ExpectClientInfoEqual(*device_info, expected);
  }
}

}  // namespace

}  // namespace ash::printing
