// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"

#include <stdint.h>

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/util/tpm_util.h"
#include "chromeos/settings/install_attributes.h"
#include "crypto/rsa_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace policy {

namespace {

void WriteInstallAttributesFile(const std::string& install_attrs_blob) {
  base::FilePath install_attrs_file;
  ASSERT_TRUE(base::PathService::Get(chromeos::FILE_INSTALL_ATTRIBUTES,
                                     &install_attrs_file));
  ASSERT_EQ(base::checked_cast<int>(install_attrs_blob.size()),
            base::WriteFile(install_attrs_file, install_attrs_blob.c_str(),
                            install_attrs_blob.size()));
}

}  // namespace

DevicePolicyCrosTestHelper::DevicePolicyCrosTestHelper() {}

DevicePolicyCrosTestHelper::~DevicePolicyCrosTestHelper() {}

// static
void DevicePolicyCrosTestHelper::MarkAsEnterpriseOwnedBy(
    const std::string& user_name) {
  OverridePaths();
  WriteInstallAttributesFile(
      chromeos::InstallAttributes::
          GetEnterpriseOwnedInstallAttributesBlobForTesting(user_name));
}

void DevicePolicyCrosTestHelper::MarkAsEnterpriseOwned() {
  MarkAsEnterpriseOwnedBy(device_policy_.policy_data().username());
}

void DevicePolicyCrosTestHelper::InstallOwnerKey() {
  OverridePaths();

  base::FilePath owner_key_file;
  ASSERT_TRUE(
      base::PathService::Get(chromeos::FILE_OWNER_KEY, &owner_key_file));
  std::string owner_key_bits = device_policy()->GetPublicSigningKeyAsString();
  ASSERT_FALSE(owner_key_bits.empty());
  ASSERT_EQ(base::checked_cast<int>(owner_key_bits.length()),
            base::WriteFile(owner_key_file, owner_key_bits.data(),
                            owner_key_bits.length()));
}

// static
void DevicePolicyCrosTestHelper::OverridePaths() {
  // This is usually done by ChromeBrowserMainChromeOS, but some tests
  // use the overridden paths before ChromeBrowserMain starts. Make sure that
  // the paths are overridden before using them.
  base::FilePath user_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  chromeos::RegisterStubPathOverrides(user_data_dir);
}

DevicePolicyCrosBrowserTest::DevicePolicyCrosBrowserTest()
    : fake_session_manager_client_(new chromeos::FakeSessionManagerClient) {}

DevicePolicyCrosBrowserTest::~DevicePolicyCrosBrowserTest() = default;

void DevicePolicyCrosBrowserTest::SetUp() {
  // Set some fake state keys to make surethey are not empty.
  std::vector<std::string> state_keys;
  state_keys.push_back("1");
  fake_session_manager_client_->set_server_backed_state_keys(state_keys);
  InProcessBrowserTest::SetUp();
}

void DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture() {
  InstallOwnerKey();
  MarkOwnership();
  dbus_setter_ = chromeos::DBusThreadManager::GetSetterForTesting();
  dbus_setter_->SetSessionManagerClient(
      std::unique_ptr<chromeos::SessionManagerClient>(
          fake_session_manager_client_));
}

void DevicePolicyCrosBrowserTest::MarkOwnership() {
  MarkAsEnterpriseOwned();
}

void DevicePolicyCrosBrowserTest::MarkAsEnterpriseOwned() {
  test_helper_.MarkAsEnterpriseOwned();
}

void DevicePolicyCrosBrowserTest::InstallOwnerKey() {
  test_helper_.InstallOwnerKey();
}

void DevicePolicyCrosBrowserTest::RefreshDevicePolicy() {
  // Reset the key to its original state.
  device_policy()->SetDefaultSigningKey();
  device_policy()->Build();
  session_manager_client()->set_device_policy(device_policy()->GetBlob());
  session_manager_client()->OnPropertyChangeComplete(true);
}

}  // namespace policy
