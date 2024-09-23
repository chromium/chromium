// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_policy_cros_test_helper.h"

#include <string>

#include "ash/constants/ash_paths.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_store_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {

namespace em = ::enterprise_management;

class CloudPolicyStoreWaiter : public CloudPolicyStore::Observer {
 public:
  CloudPolicyStoreWaiter() = default;
  CloudPolicyStoreWaiter(const CloudPolicyStoreWaiter&) = delete;
  CloudPolicyStoreWaiter& operator=(const CloudPolicyStoreWaiter&) = delete;
  ~CloudPolicyStoreWaiter() override = default;

  void OnStoreLoaded(CloudPolicyStore* store) override { loop_.Quit(); }

  void OnStoreError(CloudPolicyStore* store) override { FAIL(); }

  void Wait() { loop_.Run(); }

 private:
  base::RunLoop loop_;
};

}  // namespace

DevicePolicyCrosTestHelper::DevicePolicyCrosTestHelper() = default;

DevicePolicyCrosTestHelper::~DevicePolicyCrosTestHelper() = default;

void DevicePolicyCrosTestHelper::InstallOwnerKey() {
  OverridePaths();

  base::FilePath owner_key_file;
  ASSERT_TRUE(base::PathService::Get(chromeos::dbus_paths::FILE_OWNER_KEY,
                                     &owner_key_file));
  std::string owner_key_bits = device_policy()->GetPublicSigningKeyAsString();
  ASSERT_FALSE(owner_key_bits.empty());
  ASSERT_TRUE(base::WriteFile(owner_key_file, owner_key_bits));
}

// static
void DevicePolicyCrosTestHelper::OverridePaths() {
  // This is usually done by `ChromeBrowserMainPartsAsh`, but some tests
  // use the overridden paths before ChromeBrowserMain starts. Make sure that
  // the paths are overridden before using them.
  base::FilePath user_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  base::ScopedAllowBlockingForTesting allow_io;
  ash::RegisterStubPathOverrides(user_data_dir);
  chromeos::dbus_paths::RegisterStubPathOverrides(user_data_dir);
}

const std::string DevicePolicyCrosTestHelper::device_policy_blob() {
  // Reset the key to its original state.
  device_policy()->SetDefaultSigningKey();
  device_policy()->Build();
  return device_policy()->GetBlob();
}

void DevicePolicyCrosTestHelper::RefreshDevicePolicy() {
  ash::FakeSessionManagerClient::Get()->set_device_policy(device_policy_blob());
  ash::FakeSessionManagerClient::Get()->OnPropertyChangeComplete(true);
}

void DevicePolicyCrosTestHelper::RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
    const std::vector<std::string>& settings) {
  base::RunLoop run_loop;

  // For calls from SetPolicy().
  std::vector<base::CallbackListSubscription> subscriptions = {};
  for (auto& setting : settings) {
    subscriptions.push_back(ash::CrosSettings::Get()->AddSettingsObserver(
        setting, run_loop.QuitClosure()));
  }
  RefreshDevicePolicy();
  run_loop.Run();
  // Allow tasks posted by CrosSettings observers to complete:
  base::RunLoop().RunUntilIdle();
}

void DevicePolicyCrosTestHelper::
    RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated() {
  policy::DeviceCloudPolicyStoreAsh* policy_store =
      g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->GetDeviceCloudPolicyManager()
          ->device_store();
  if (!policy_store->has_policy()) {
    CloudPolicyStoreWaiter waiter;
    policy_store->AddObserver(&waiter);
    RefreshDevicePolicy();
    waiter.Wait();
    policy_store->RemoveObserver(&waiter);
  }
}

void DevicePolicyCrosTestHelper::UnsetPolicy(
    const std::vector<std::string>& settings) {
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.clear_display_rotation_default();
  proto.clear_device_display_resolution();
  RefreshPolicyAndWaitUntilDeviceSettingsUpdated(settings);
}

}  // namespace policy
