// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/device_cloud_policy_status_provider_chromeos.h"

#include <memory>
#include <string>

#include "ash/constants/ash_paths.h"
#include "base/memory/ref_counted.h"
#include "base/test/run_until.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/ash/settings/scoped_test_device_settings_service.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/policy/status_provider/status_provider_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/resources/webui/mojom/policy.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class DeviceCloudPolicyStatusProviderChromeOSTest
    : public ash::DeviceSettingsTestBase {
 public:
  DeviceCloudPolicyStatusProviderChromeOSTest()
      : ash::DeviceSettingsTestBase(/*profile_creation_enabled=*/true) {}
  DeviceCloudPolicyStatusProviderChromeOSTest(
      const DeviceCloudPolicyStatusProviderChromeOSTest&) = delete;
  DeviceCloudPolicyStatusProviderChromeOSTest& operator=(
      const DeviceCloudPolicyStatusProviderChromeOSTest&) = delete;
  ~DeviceCloudPolicyStatusProviderChromeOSTest() override = default;

  void SetUp() override {
    ash::DBusThreadManager::Initialize();
    install_attributes_ = std::make_unique<ash::ScopedStubInstallAttributes>();
    ash::DeviceSettingsTestBase::SetUp();
    scoped_test_device_settings_service_ =
        std::make_unique<ash::ScopedTestDeviceSettingsService>();
    ash::DeviceSettingsService::Get()->StartProcessing(
        TestingBrowserProcess::GetGlobal()->local_state(),
        &session_manager_client_, owner_key_util_);

    connector_ = std::make_unique<BrowserPolicyConnectorAsh>();
  }

  void TearDown() override {
    connector_.reset();
    install_attributes_.reset();
    ash::DBusThreadManager::Shutdown();
    scoped_test_device_settings_service_.reset();
    ash::DeviceSettingsTestBase::TearDown();
  }

  void InstallManagedDevicePolicy(const std::string& managed_by) {
    if (!install_attributes_->Get()->IsCloudManaged()) {
      install_attributes_->Get()->SetCloudManaged("example.com",
                                                  "fake-device-id");
    }
    ash::DeviceSettingsTestBase::device_policy_->policy_data().set_username(
        "admin@example.com");
    if (!managed_by.empty()) {
      ash::DeviceSettingsTestBase::device_policy_->policy_data().set_managed_by(
          managed_by);
    }
    ash::DeviceSettingsTestBase::device_policy_->policy_data().set_device_id(
        "fake-device-id");
    ash::DeviceSettingsTestBase::device_policy_->policy_data()
        .set_annotated_location("HQ Floor 2");
    ash::DeviceSettingsTestBase::device_policy_->policy_data()
        .set_annotated_asset_id("ASSET-12345");
    ash::DeviceSettingsTestBase::device_policy_->policy_data()
        .set_directory_api_id("DIR-999");
    ash::DeviceSettingsTestBase::device_policy_->Build();
    session_manager_client_.set_device_policy(
        ash::DeviceSettingsTestBase::device_policy_->GetBlob());
    ash::DeviceSettingsTestBase::ReloadDeviceSettings();
    ash::DeviceSettingsService::Get()->OwnerKeySet(true);
  }

 protected:
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  std::unique_ptr<ash::ScopedTestDeviceSettingsService>
      scoped_test_device_settings_service_;
  std::unique_ptr<ash::ScopedStubInstallAttributes> install_attributes_;
  std::unique_ptr<BrowserPolicyConnectorAsh> connector_;
};

TEST_F(DeviceCloudPolicyStatusProviderChromeOSTest, GetStatusMojo_Empty) {
  DeviceCloudPolicyStatusProviderChromeOS provider(connector_.get(),
                                                   profile_.get());
  policy::mojom::StatusPtr status = provider.GetStatusMojo();

  EXPECT_EQ(status->policy_description_key, kDevicePolicyStatusDescription);
}

TEST_F(DeviceCloudPolicyStatusProviderChromeOSTest, GetStatusMojo_Managed) {
  InstallManagedDevicePolicy("manager.example.com");
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return connector_->GetDeviceCloudPolicyManager()
        ->core()
        ->store()
        ->is_managed();
  }));

  DeviceCloudPolicyStatusProviderChromeOS provider(connector_.get(),
                                                   profile_.get());
  policy::mojom::StatusPtr status = provider.GetStatusMojo();

  EXPECT_EQ(status->policy_description_key, kDevicePolicyStatusDescription);
  EXPECT_EQ(status->enterprise_domain_manager.value_or(""),
            "manager.example.com");
  EXPECT_EQ(status->username.value_or(""), "admin@example.com");
  EXPECT_EQ(status->client_id, "fake-device-id");
  EXPECT_EQ(status->location.value_or(""), "HQ Floor 2");
  EXPECT_EQ(status->asset_id.value_or(""), "ASSET-12345");
  EXPECT_EQ(status->directory_api_id.value_or(""), "DIR-999");
}

TEST_F(DeviceCloudPolicyStatusProviderChromeOSTest,
       GetStatusMojo_FallbackDomain) {
  install_attributes_->Get()->SetCloudManaged("enrollment.example.com",
                                              "fake-device-id");
  InstallManagedDevicePolicy(/*managed_by=*/"");
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return connector_->GetDeviceCloudPolicyManager()
        ->core()
        ->store()
        ->is_managed();
  }));

  DeviceCloudPolicyStatusProviderChromeOS provider(connector_.get(),
                                                   profile_.get());
  policy::mojom::StatusPtr status = provider.GetStatusMojo();

  EXPECT_EQ(status->policy_description_key, kDevicePolicyStatusDescription);
  EXPECT_EQ(status->enterprise_domain_manager.value_or(""),
            "enrollment.example.com");
}

}  // namespace policy
