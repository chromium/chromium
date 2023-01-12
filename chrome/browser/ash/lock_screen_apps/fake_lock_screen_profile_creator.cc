// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lock_screen_apps/fake_lock_screen_profile_creator.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"

namespace lock_screen_apps {

FakeLockScreenProfileCreator::FakeLockScreenProfileCreator(
    TestingProfileManager* profile_manager)
    : profile_manager_(profile_manager) {}

FakeLockScreenProfileCreator::~FakeLockScreenProfileCreator() {}

void FakeLockScreenProfileCreator::CreateProfile() {
  OnLockScreenProfileCreateStarted();

  Profile* profile = profile_manager_->CreateTestingProfile(
      ash::kLockScreenAppBrowserContextBaseName);

  extensions::TestExtensionSystem* extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile));
  extension_system->CreateExtensionService(
      base::CommandLine::ForCurrentProcess(),
      profile->GetPath().Append("Extensions") /* install_directory */,
      false /* autoupdate_enabled */);

  OnLockScreenProfileCreated(profile);
}

void FakeLockScreenProfileCreator::SetProfileCreationFailed() {
  OnLockScreenProfileCreateStarted();
  OnLockScreenProfileCreated(nullptr);
}

void FakeLockScreenProfileCreator::InitializeImpl() {}

}  // namespace lock_screen_apps
