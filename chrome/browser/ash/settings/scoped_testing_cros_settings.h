// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SETTINGS_SCOPED_TESTING_CROS_SETTINGS_H_
#define CHROME_BROWSER_ASH_SETTINGS_SCOPED_TESTING_CROS_SETTINGS_H_

#include <memory>

#include "base/memory/raw_ptr.h"

namespace ash {

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
// OwnerSettingsServiceAshFactory::SetStubCrosSettingsProviderForTesting is
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

  ScopedTestingCrosSettings(const ScopedTestingCrosSettings&) = delete;
  ScopedTestingCrosSettings& operator=(const ScopedTestingCrosSettings&) =
      delete;

  ~ScopedTestingCrosSettings();

  StubCrosSettingsProvider* device_settings() { return device_settings_ptr_; }

  SystemSettingsProvider* system_settings() { return system_settings_ptr_; }

 private:
  std::unique_ptr<CrosSettings> test_instance_;

  // These are raw pointers since these objects are owned by |test_instance_|.
  raw_ptr<StubCrosSettingsProvider> device_settings_ptr_;
  raw_ptr<SystemSettingsProvider> system_settings_ptr_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SETTINGS_SCOPED_TESTING_CROS_SETTINGS_H_
