// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_SCOPED_CROS_SETTINGS_TEST_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_SCOPED_CROS_SETTINGS_TEST_HELPER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "chromeos/tpm/stub_install_attributes.h"

class Profile;

namespace base {
class Value;
}

namespace chromeos {

class FakeOwnerSettingsService;
class ScopedTestCrosSettings;
class ScopedTestDeviceSettingsService;

// Helps in a variety of ways with setting up CrosSettings for testing.
// This class is overly complex for most use-cases - if possible, prefer to
// use ScopedTestingCrosSettings for new tests.
class ScopedCrosSettingsTestHelper {
 public:
  // In some cases it is required to pass |create_settings_service| as false:
  // If the test already has a device settings service and/or CrosSettings set
  // up by another (instantiated or base) class, creating another one causes
  // crash.
  explicit ScopedCrosSettingsTestHelper(bool create_settings_service = true);
  ~ScopedCrosSettingsTestHelper();

  // This replaces the DeviceSettingsProvider with a simple stub that stores
  // settings in memory unsigned; see StubCrosSettingsProvider for more info.
  void ReplaceDeviceSettingsProviderWithStub();
  void RestoreRealDeviceSettingsProvider();
  bool IsDeviceSettingsProviderStubbed();

  // Method to create an owner settings service that uses
  // |stub_settings_provider_| as settings write path.
  std::unique_ptr<FakeOwnerSettingsService> CreateOwnerSettingsService(
      Profile* profile);

  // Returns the stubbed CrosSettingsProvider - the one that is swapped into
  // |CrosSettings| once |ReplaceDeviceSettingsProviderWithStub()| is called.
  // Note that if you want to test the real DeviceSettingsProvider in your test
  // (not a stub), you should set the settings using the OwnerSettingsService
  // which uses the current user's private key to sign the settings.
  StubCrosSettingsProvider* GetStubbedProvider();

  // These methods simply call the appropriate method on |GetStubbedProvider()|.
  // So if you use them, you need to make sure that a stubbed provider is used
  // in your test by calling |ReplaceDeviceSettingsProviderWithStub()|.
  void SetTrustedStatus(CrosSettingsProvider::TrustedStatus status);
  void SetCurrentUserIsOwner(bool owner);
  void Set(const std::string& path, const base::Value& in_value);

  // Convenience forms of Set() from CrosSettingsProvider. These methods will
  // replace any existing value at that |path|, even if it has a different type.
  void SetBoolean(const std::string& path, bool in_value);
  void SetInteger(const std::string& path, int in_value);
  void SetDouble(const std::string& path, double in_value);
  void SetString(const std::string& path, const std::string& in_value);

  // This may be called before or after |ReplaceDeviceSettingsProviderWithStub|
  // is called. It reads the value for |path| from the original, real,
  // DeviceSettingsProvider, and copies it to the stub DeviceSettingsProvider.
  void CopyStoredValue(const std::string& path);

  // Write the setting from |path| in the stub DeviceSettingsProvider to local
  // state so that it can be retrieved later on browser test startup by the
  // device settings service.
  void StoreCachedDeviceSetting(const std::string& path);

  // Get the scoped install attributes to change them as needed for the
  // current test.
  StubInstallAttributes* InstallAttributes();

 private:
  std::unique_ptr<ScopedStubInstallAttributes> test_install_attributes_;
  std::unique_ptr<ScopedTestDeviceSettingsService>
      test_device_settings_service_;
  std::unique_ptr<ScopedTestCrosSettings> test_cros_settings_;
  std::unique_ptr<CrosSettingsProvider> real_settings_provider_;
  std::unique_ptr<CrosSettingsProvider> stub_settings_provider_;
  StubCrosSettingsProvider* stub_settings_provider_ptr_;

  void Initialize(bool create_settings_service);

  DISALLOW_COPY_AND_ASSIGN(ScopedCrosSettingsTestHelper);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_SCOPED_CROS_SETTINGS_TEST_HELPER_H_
