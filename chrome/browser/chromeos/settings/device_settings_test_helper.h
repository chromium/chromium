// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_SETTINGS_TEST_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_SETTINGS_TEST_HELPER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestingProfile;

namespace chromeos {

class DBusThreadManagerSetter;
class FakePowerManagerClient;

// Wraps the singleton device settings and initializes it to the point where it
// reports OWNERSHIP_NONE for the ownership status.
class ScopedDeviceSettingsTestHelper {
 public:
  ScopedDeviceSettingsTestHelper();
  ~ScopedDeviceSettingsTestHelper();

 private:
  FakeSessionManagerClient session_manager_client_;
  DISALLOW_COPY_AND_ASSIGN(ScopedDeviceSettingsTestHelper);
};

// A convenience test base class that initializes a DeviceSettingsService
// instance for testing and allows for straightforward updating of device
// settings. |device_settings_service_| starts out in uninitialized state, so
// startup code gets tested as well.
class DeviceSettingsTestBase : public testing::Test {
 protected:
  DeviceSettingsTestBase();
  ~DeviceSettingsTestBase() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // Subclasses that modify the DevicePolicy should call this afterwards.
  void ReloadDevicePolicy();

  // Flushes any pending device settings operations.
  void FlushDeviceSettings();

  // Triggers an owner key and device settings reload on
  // |device_settings_service_| and flushes the resulting load operation.
  void ReloadDeviceSettings();

  void InitOwner(const AccountId& account_id, bool tpm_is_ready);

  FakePowerManagerClient* power_manager_client();

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<policy::DevicePolicyBuilder> device_policy_;

  FakeSessionManagerClient session_manager_client_;
  // Note that FakeUserManager is used by ProfileHelper, which some of the
  // tested classes depend on implicitly.
  FakeChromeUserManager* user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_;
  // Local DeviceSettingsService instance for tests. Avoid using in combination
  // with the global instance (DeviceSettingsService::Get()).
  std::unique_ptr<DeviceSettingsService> device_settings_service_;

  std::unique_ptr<DBusThreadManagerSetter> dbus_setter_;

  std::unique_ptr<TestingProfile> profile_;

 private:
  bool teardown_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(DeviceSettingsTestBase);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_SETTINGS_TEST_HELPER_H_
