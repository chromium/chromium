// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_SCOPED_TESTING_CROS_SETTINGS_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_SCOPED_TESTING_CROS_SETTINGS_H_

#include <memory>

#include "base/macros.h"

namespace chromeos {

class CrosSettings;
class StubCrosSettingsProvider;
class SystemSettingsProvider;

// Helper class which calls CrosSettings::SetForTesting when it is constructed,
// and calls CrosSettings::ShutdownForTesting when it goes out of scope,
// with a CrosSettings instance that it creates and owns.
// This CrosSettings instance has two settings providers:
// a StubCrosSettingsProvider for device settings,
// and a regular SystemSettingsProvider for the system settings.
//
// OwnerSettingsServiceChromeOSFactory::SetStubCrosSettingsProviderForTesting is
// caled too, with the StubCrosSettingsProvider, so that any
// OwnerSettingsService created will write to the StubCrosSettingsProvider.
//
// Prefer to use this class instead of ScopedCrosSettingsTestHelper - that class
// has even more responsibilities and is overly complex for most use-cases.
class ScopedTestingCrosSettings {
 public:
  // Creates a CrosSettings test instance that has two settings providers -
  // a StubCrosSettingsProvider, and a SystemsSettingsProvider - and sets it
  // for testing.
  ScopedTestingCrosSettings();

  ~ScopedTestingCrosSettings();

  StubCrosSettingsProvider* device_settings() { return device_settings_ptr_; }

  SystemSettingsProvider* system_settings() { return system_settings_ptr_; }

 private:
  std::unique_ptr<CrosSettings> test_instance_;

  // These are raw pointers since these objects are owned by |test_instance_|.
  StubCrosSettingsProvider* device_settings_ptr_;
  SystemSettingsProvider* system_settings_ptr_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTestingCrosSettings);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_SCOPED_TESTING_CROS_SETTINGS_H_
