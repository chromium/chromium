// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/account_extension_tracker.h"

#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/signin_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"

namespace extensions {

namespace {

constexpr char kGoodCrx[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";

}  // namespace

class AccountExtensionTrackerUnitTest : public ExtensionServiceTestWithInstall {
 public:
  void SetUp() override {
    ExtensionServiceTestWithInstall::SetUp();
    InitializeEmptyExtensionService();

    service()->Init();
    ASSERT_TRUE(extension_system()->is_ready());

    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
  }

 protected:
  ExtensionSystem* extension_system() {
    return ExtensionSystem::Get(profile());
  }

  AccountExtensionTracker::AccountExtensionType GetAccountExtensionType(
      const ExtensionId& id) {
    return AccountExtensionTracker::Get(profile())->GetAccountExtensionType(id);
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

 private:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
};

// Test that an extension's AccountExtensionType is set to the right value based
// on whether it was installed when there is a signed in user with sync enabled.
TEST_F(AccountExtensionTrackerUnitTest, AccountExtensionTypeSignedIn) {
  base::FilePath good_crx_path = data_dir().AppendASCII("good.crx");
  InstallCRX(good_crx_path, INSTALL_NEW);
  EXPECT_EQ(AccountExtensionTracker::AccountExtensionType::kLocal,
            GetAccountExtensionType(kGoodCrx));

  UninstallExtension(kGoodCrx);

  // Mimic signing a user in with sync enabled.
  identity_test_env()->MakePrimaryAccountAvailable("testy@mctestface.com",
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

// Same as the above test, except this uses transport mode (signed in but not
// syncing) instead of sync, and an explicit user sign in.
TEST_F(AccountExtensionTrackerUnitTest, AccountExtensionTypeTransportMode) {
  // Enable extension syncing in transport mode.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      switches::kEnableExtensionsExplicitBrowserSignin);

  // The extension's AccountExtensionType is `kLocal` because the user has not
  // explicitly signed in yet.
  base::FilePath good_crx_path = data_dir().AppendASCII("good.crx");
  InstallCRX(good_crx_path, INSTALL_NEW);
  EXPECT_EQ(AccountExtensionTracker::AccountExtensionType::kLocal,
            GetAccountExtensionType(kGoodCrx));

  UninstallExtension(kGoodCrx);

  signin_test_util::SimulateExplicitSignIn(profile(), identity_test_env());

  // Reinstall the extension. Since the user has now signed in properly. the
  // extension should be associated with the user's account data and have an
  // AccountExtensionType of `kAccountInstalledSignedIn`.
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

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(AccountExtensionTrackerUnitTest,
       AccountExtensionTypeResetWhenSignedOut) {
  // Enable extension syncing in transport mode.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      switches::kEnableExtensionsExplicitBrowserSignin);

  signin_test_util::SimulateExplicitSignIn(profile(), identity_test_env());

  base::FilePath good_crx_path = data_dir().AppendASCII("good.crx");
  InstallCRX(good_crx_path, INSTALL_NEW);
  EXPECT_EQ(
      AccountExtensionTracker::AccountExtensionType::kAccountInstalledSignedIn,
      GetAccountExtensionType(kGoodCrx));

  // Sign the user out and verify that `kGoodCrx` is now treated as a local
  // extension again.
  identity_test_env()->ClearPrimaryAccount();
  EXPECT_TRUE(registry()->GetInstalledExtension(kGoodCrx));
  EXPECT_EQ(AccountExtensionTracker::AccountExtensionType::kLocal,
            GetAccountExtensionType(kGoodCrx));
}

TEST_F(AccountExtensionTrackerUnitTest, AccountExtensionsRemovedWhenSignedOut) {
  // Enable extension syncing in transport mode.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      switches::kEnableExtensionsExplicitBrowserSignin);

  signin_test_util::SimulateExplicitSignIn(profile(), identity_test_env());

  base::FilePath good_crx_path = data_dir().AppendASCII("good.crx");
  InstallCRX(good_crx_path, INSTALL_NEW);
  EXPECT_EQ(
      AccountExtensionTracker::AccountExtensionType::kAccountInstalledSignedIn,
      GetAccountExtensionType(kGoodCrx));

  // Install an extension and pretend it's a `kAccountInstalledSignedIn`
  // extension.
  ChromeTestExtensionLoader extension_loader(profile());
  extension_loader.set_pack_extension(true);
  scoped_refptr<const Extension> other_extension =
      extension_loader.LoadExtension(
          data_dir().AppendASCII("simple_with_file"));
  const ExtensionId other_extension_id = other_extension->id();
  AccountExtensionTracker::Get(profile())->SetAccountExtensionTypeForTesting(
      other_extension_id,
      AccountExtensionTracker::AccountExtensionType::kAccountInstalledLocally);

  // Set the uninstall flag.
  AccountExtensionTracker::Get(profile())
      ->set_uninstall_account_extensions_on_signout(true);

  // Sign the user out and verify that `kGoodCrx` is now uninstalled.
  identity_test_env()->ClearPrimaryAccount();
  EXPECT_FALSE(registry()->GetInstalledExtension(kGoodCrx));

  // But `other_extension` is still installed.
  EXPECT_TRUE(registry()->GetInstalledExtension(other_extension_id));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace extensions
