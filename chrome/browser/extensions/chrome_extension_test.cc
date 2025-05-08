// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_test.h"

#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {

ChromeExtensionTest::ChromeExtensionTest() = default;

ChromeExtensionTest::~ChromeExtensionTest() = default;

void ChromeExtensionTest::SetUp() {
  // Ensure the profile uses a TestExtensionSystem.
  profile_ = TestingProfile::Builder().Build();

  TestExtensionSystem* test_extension_system =
      static_cast<TestExtensionSystem*>(
          ExtensionSystem::Get(browser_context()));
  test_extension_system->Init();

  extension_registrar_ = ExtensionRegistrar::Get(browser_context());
  extension_registry_ = ExtensionRegistry::Get(browser_context());
}

void ChromeExtensionTest::TearDown() {
  // Tear down the profile. First, clear out any KeyedService references
  // (they'll be destroyed as part of profile destruction).
  extension_registrar_ = nullptr;
  extension_registry_ = nullptr;

  profile_.reset();
}

Profile* ChromeExtensionTest::profile() {
  return profile_.get();
}

content::BrowserContext* ChromeExtensionTest::browser_context() {
  return profile_.get();
}

}  // namespace extensions
