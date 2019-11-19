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
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/constants/chromeos_paths.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "chromeos/dbus/cryptohome/tpm_util.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/tpm/install_attributes.h"
#include "crypto/rsa_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace policy {

DevicePolicyCrosTestHelper::DevicePolicyCrosTestHelper() {}

DevicePolicyCrosTestHelper::~DevicePolicyCrosTestHelper() {}

void DevicePolicyCrosTestHelper::InstallOwnerKey() {
  OverridePaths();

  base::FilePath owner_key_file;
  ASSERT_TRUE(base::PathService::Get(chromeos::dbus_paths::FILE_OWNER_KEY,
                                     &owner_key_file));
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
  base::ScopedAllowBlockingForTesting allow_io;
  chromeos::RegisterStubPathOverrides(user_data_dir);
}

DevicePolicyCrosBrowserTest::DevicePolicyCrosBrowserTest()
    : fake_session_manager_client_(new chromeos::FakeSessionManagerClient) {}

DevicePolicyCrosBrowserTest::~DevicePolicyCrosBrowserTest() = default;

void DevicePolicyCrosBrowserTest::RefreshDevicePolicy() {
  // Reset the key to its original state.
  device_policy()->SetDefaultSigningKey();
  device_policy()->Build();
  session_manager_client()->set_device_policy(device_policy()->GetBlob());
  session_manager_client()->OnPropertyChangeComplete(true);
}

}  // namespace policy
