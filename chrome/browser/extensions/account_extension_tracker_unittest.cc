// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/account_extension_tracker.h"

#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"

namespace extensions {

namespace {

constexpr char kGoodCrx[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";

}  // namespace

class AccountExtensionTrackerUnitTest : public ExtensionServiceTestWithInstall {
 protected:
  ExtensionSystem* extension_system() {
    return ExtensionSystem::Get(profile());
  }

  AccountExtensionTracker::AccountExtensionType GetAccountExtensionType(
      const ExtensionId& id) {
    return AccountExtensionTracker::Get(profile())
        ->GetAccountExtensionTypeForTesting(id);
  }
};

// Test that an extension's AccountExtensionType is set to the right value based
// on whether it was installed when there is a signed in user with sync enabled.
TEST_F(AccountExtensionTrackerUnitTest, AccountExtensionTypeSignedIn) {
  InitializeEmptyExtensionService();

  service()->Init();
  ASSERT_TRUE(extension_system()->is_ready());

  base::FilePath good_crx_path = data_dir().AppendASCII("good.crx");
  InstallCRX(good_crx_path, INSTALL_NEW);
  EXPECT_EQ(AccountExtensionTracker::AccountExtensionType::kLocal,
            GetAccountExtensionType(kGoodCrx));

  UninstallExtension(kGoodCrx);

  // Use a test identity environment to mimic signing a user in with sync
  // enabled.
  auto identity_test_env_profile_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
  identity_test_env_profile_adaptor->identity_test_env()
      ->MakePrimaryAccountAvailable("testy@mctestface.com",
                                    signin::ConsentLevel::kSync);

  // Reinstall `kGoodCrx` while there is a signed in user. Since `kGoodCrx` is
  // syncable, it should be associated with the user's account data, and since
  // it was installed when there is a signed in user, its AccountExtensionType
  // is `kAccountInstalledSignedIn`.
  InstallCRX(good_crx_path, INSTALL_NEW);
  EXPECT_EQ(
      AccountExtensionTracker::AccountExtensionType::kAccountInstalledSignedIn,
      GetAccountExtensionType(kGoodCrx));

  // Install an external extension. This extension shouldn't be attached to
  // account data due to being unsyncable and thus has the AccountExtensionType
  // `kLocal`.
  ChromeTestExtensionLoader extension_loader(profile());
  extension_loader.set_pack_extension(true);
  extension_loader.add_creation_flag(Extension::WAS_INSTALLED_BY_DEFAULT);
  scoped_refptr<const Extension> external_extension =
      extension_loader.LoadExtension(
          data_dir().AppendASCII("simple_with_file"));
  ASSERT_TRUE(external_extension);
  EXPECT_EQ(AccountExtensionTracker::AccountExtensionType::kLocal,
            GetAccountExtensionType(external_extension->id()));
}

}  // namespace extensions
