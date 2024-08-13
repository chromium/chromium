// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SETTINGS_DEVICE_SETTINGS_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_SETTINGS_DEVICE_SETTINGS_TEST_HELPER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/ownership/mock_owner_key_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestingProfile;

namespace ash {

// Wraps the singleton device settings and initializes it to the point where it
// reports OwnershipStatus::kOwnershipNone for the ownership status.
class ScopedDeviceSettingsTestHelper {
 public:
  ScopedDeviceSettingsTestHelper();

  ScopedDeviceSettingsTestHelper(const ScopedDeviceSettingsTestHelper&) =
      delete;
  ScopedDeviceSettingsTestHelper& operator=(
      const ScopedDeviceSettingsTestHelper&) = delete;

  ~ScopedDeviceSettingsTestHelper();

 private:
  FakeSessionManagerClient session_manager_client_;
};

// A convenience test base class that initializes a DeviceSettingsService
// instance for testing and allows for straightforward updating of device
// settings. |device_settings_service_| starts out in uninitialized state, so
// startup code gets tested as well.
class DeviceSettingsTestBase : public testing::Test {
 public:
  DeviceSettingsTestBase(const DeviceSettingsTestBase&) = delete;
  DeviceSettingsTestBase& operator=(const DeviceSettingsTestBase&) = delete;

 protected:
  explicit DeviceSettingsTestBase(bool profile_creation_enabled = true);
  explicit DeviceSettingsTestBase(base::test::TaskEnvironment::TimeSource time);
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

  void SetSessionStopping();

  const bool profile_creation_enabled_ = true;

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<policy::DevicePolicyBuilder> device_policy_;

  FakeSessionManagerClient session_manager_client_;
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_;
  // Local DeviceSettingsService instance for tests. Avoid using in combination
  // with the global instance (DeviceSettingsService::Get()).
  std::unique_ptr<DeviceSettingsService> device_settings_service_;

  std::unique_ptr<TestingProfile> profile_;

 private:
  bool teardown_called_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SETTINGS_DEVICE_SETTINGS_TEST_HELPER_H_
