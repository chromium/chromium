// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/run_loop.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_attributes_impl.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_store_ash.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::InvokeWithoutArgs;

namespace policy {

namespace {

constexpr char kFakeDomain[] = "fake.domain.acme.corp";
constexpr char kFakeDisplayDomain[] = "acme.corp";
constexpr char kFakeSSOProfile[] = "fake sso profile";
constexpr char kFakeAssetId[] = "fake asset ID";
constexpr char kFakeSerialNumber[] = "fake serial number";
constexpr char kFakeMachineName[] = "fake machine name";
constexpr char kFakeAnnotatedLocation[] = "fake annotated location";
constexpr char kFakeHostname[] = "fake-hostname";
constexpr char kFakeDirectoryApiID[] = "fake directory API ID";
constexpr char kFakeObfuscatedCustomerID[] = "fake obfuscated customer ID";
constexpr char kFakeLogoURL[] = "www.fakelogo.com/url";
constexpr char kFakeDeviceID[] = "fake device ID";

}  // namespace

class DeviceAttributesTest : public DevicePolicyCrosBrowserTest {
 public:
  DeviceAttributesTest() {
    device_state_.set_skip_initial_policy_setup(true);
    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kValid);
  }

  ~DeviceAttributesTest() override = default;

 protected:
  ash::StubInstallAttributes* stub_install_attributes() {
    return install_attributes_.Get();
  }

  DeviceAttributesImpl attributes_;
  ash::ScopedStubInstallAttributes install_attributes_;
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

IN_PROC_BROWSER_TEST_F(DeviceAttributesTest, ReturnsAttributes) {
  // Attributes are empty before setting device policy and install attributes.
  EXPECT_EQ("", attributes_.GetEnterpriseEnrollmentDomain());
  EXPECT_EQ("", attributes_.GetEnterpriseDomainManager());
  EXPECT_EQ("", attributes_.GetSSOProfile());
  EXPECT_EQ("", attributes_.GetDeviceAssetID());
  EXPECT_EQ("", attributes_.GetDeviceSerialNumber());
  EXPECT_EQ("", attributes_.GetMachineName());
  EXPECT_EQ("", attributes_.GetDeviceAnnotatedLocation());
  EXPECT_EQ(std::nullopt, attributes_.GetDeviceHostname());
  EXPECT_EQ("", attributes_.GetDirectoryApiID());
  EXPECT_EQ("", attributes_.GetObfuscatedCustomerID());
  EXPECT_EQ("", attributes_.GetCustomerLogoURL());
  EXPECT_EQ(MarketSegment::UNKNOWN, attributes_.GetEnterpriseMarketSegment());

  // Set fake device policy and install attributes.
  stub_install_attributes()->SetCloudManaged(kFakeDomain, kFakeDeviceID);
  device_policy()->policy_data().set_display_domain(kFakeDisplayDomain);
  device_policy()->policy_data().set_sso_profile(kFakeSSOProfile);
  device_policy()->policy_data().set_annotated_asset_id(kFakeAssetId);
  device_policy()->policy_data().set_machine_name(kFakeMachineName);
  device_policy()->policy_data().set_annotated_location(kFakeAnnotatedLocation);
  device_policy()->policy_data().set_directory_api_id(kFakeDirectoryApiID);
  device_policy()->policy_data().set_obfuscated_customer_id(
      kFakeObfuscatedCustomerID);
  device_policy()->policy_data().mutable_customer_logo()->set_logo_url(
      kFakeLogoURL);
  device_policy()->policy_data().set_market_segment(
      enterprise_management::PolicyData_MarketSegment_ENROLLED_ENTERPRISE);
  scoped_testing_cros_settings_.device_settings()->Set(
      ash::kDeviceHostnameTemplate, base::Value(kFakeHostname));
  policy_helper()->RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();

  fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                kFakeSerialNumber);

  // Verify returned attributes correspond to what was set.
  EXPECT_EQ(kFakeDomain, attributes_.GetEnterpriseEnrollmentDomain());
  EXPECT_EQ(kFakeDisplayDomain, attributes_.GetEnterpriseDomainManager());
  EXPECT_EQ(kFakeSSOProfile, attributes_.GetSSOProfile());
  EXPECT_EQ(kFakeAssetId, attributes_.GetDeviceAssetID());
  EXPECT_EQ(kFakeSerialNumber, attributes_.GetDeviceSerialNumber());
  EXPECT_EQ(kFakeMachineName, attributes_.GetMachineName());
  EXPECT_EQ(kFakeAnnotatedLocation, attributes_.GetDeviceAnnotatedLocation());
  EXPECT_EQ(kFakeHostname, attributes_.GetDeviceHostname());
  EXPECT_EQ(kFakeDirectoryApiID, attributes_.GetDirectoryApiID());
  EXPECT_EQ(kFakeObfuscatedCustomerID, attributes_.GetObfuscatedCustomerID());
  EXPECT_EQ(kFakeLogoURL, attributes_.GetCustomerLogoURL());
  EXPECT_EQ(MarketSegment::ENTERPRISE,
            attributes_.GetEnterpriseMarketSegment());
}

}  // namespace policy
