// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_policy_service.h"

#include "base/run_loop.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/signin_policy_service_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/test_extension_system.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registrar_factory.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)

TEST(SigninPolicyServiceTest, ForceSigninAndExtensionsBlocking) {
  content::BrowserTaskEnvironment task_environment;
  signin_util::ScopedForceSigninSetterForTesting forced_signin_setter(true);
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  // Create services.
  TestingProfile* profile = profile_manager.CreateTestingProfile("Test");
  extensions::TestExtensionSystem* extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile));
  extensions::ExtensionRegistrar* extension_registrar =
      extensions::ExtensionRegistrarFactory::GetForBrowserContext(profile);
  SigninPolicyService signin_policy_service(
      profile->GetPath(), profile_manager.profile_attributes_storage(),
      extension_system, extension_registrar);

  // Checks the default value, extensions are not blocked by default.
  ASSERT_FALSE(extension_registrar->block_extensions());

  // Finalize initialization.
  base::RunLoop quit_when_ready;
  extension_system->ready().Post(FROM_HERE,
                                 quit_when_ready.QuitWhenIdleClosure());
  extension_system->Init();
  extension_system->SetReady();
  quit_when_ready.Run();

  // Checking the initial state. Profiles are locked by default after creation.
  ProfileAttributesEntry* entry =
      profile_manager.profile_attributes_storage()
          ->GetProfileAttributesWithPath(profile->GetPath());
  ASSERT_TRUE(entry);
  EXPECT_TRUE(entry->IsSigninRequired());
  EXPECT_TRUE(extension_registrar->block_extensions());

  // Unlocking the profile.
  entry->LockForceSigninProfile(false);
  EXPECT_FALSE(entry->IsSigninRequired());
  EXPECT_FALSE(extension_registrar->block_extensions());

  // Locking the profile.
  entry->LockForceSigninProfile(true);
  EXPECT_TRUE(entry->IsSigninRequired());
  EXPECT_TRUE(extension_registrar->block_extensions());
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace
