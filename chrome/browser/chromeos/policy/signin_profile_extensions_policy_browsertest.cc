// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/policy/signin_profile_extensions_policy_test_base.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature_channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class StoragePartition;
}

namespace policy {

namespace {

// Parameters for the several extensions and apps that are used by the tests in
// this file (note that the paths are given relative to the src/chrome/test/data
// directory):
// * The manual testing app which is whitelisted for running in the sign-in
//   profile:
const char kWhitelistedAppId[] = "bjaiihebfngildkcjkjckolinodhliff";
const char kWhitelistedAppUpdateManifestPath[] =
    "/extensions/signin_screen_manual_test_app/update_manifest.xml";
// * A trivial test app which is NOT whitelisted for running in the sign-in
//   profile:
const char kNotWhitelistedAppId[] = "mockapnacjbcdncmpkjngjalkhphojek";
const char kNotWhitelistedUpdateManifestPath[] =
    "/extensions/trivial_platform_app/update_manifest.xml";
// * A trivial test extension which is whitelisted for running in the sign-in
//   profile:
const char kWhitelistedExtensionId[] = "ngjobkbdodapjbbncmagbccommkggmnj";
const char kWhitelistedExtensionUpdateManifestPath[] =
    "/extensions/signin_screen_manual_test_extension/update_manifest.xml";
// * A trivial test extension which is NOT whitelisted for running in the
//   sign-in profile:
const char kNotWhitelistedExtensionId[] = "mockepjebcnmhmhcahfddgfcdgkdifnc";
const char kNotWhitelistedExtensionUpdateManifestPath[] =
    "/extensions/trivial_extension/update_manifest.xml";

// Observer that allows waiting for an installation failure of a specific
// extension/app.
// TODO(emaxx): Extract this into a more generic helper class for using in other
// tests.
class ExtensionInstallErrorObserver final {
 public:
  ExtensionInstallErrorObserver(const Profile* profile,
                                const std::string& extension_id)
      : profile_(profile),
        extension_id_(extension_id),
        notification_observer_(
            extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR,
            base::Bind(&ExtensionInstallErrorObserver::IsNotificationRelevant,
                       base::Unretained(this))) {}

  void Wait() { notification_observer_.Wait(); }

 private:
  // Callback which is used for |WindowedNotificationObserver| for checking
  // whether the condition being awaited is met.
  bool IsNotificationRelevant(
      const content::NotificationSource& source,
      const content::NotificationDetails& details) const {
    extensions::CrxInstaller* const crx_installer =
        content::Source<extensions::CrxInstaller>(source).ptr();
    return crx_installer->profile() == profile_ &&
           crx_installer->extension()->id() == extension_id_;
  }

  const Profile* const profile_;
  const std::string extension_id_;
  content::WindowedNotificationObserver notification_observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallErrorObserver);
};

// Observer that allows waiting until the background page of the specified
// extension/app loads.
// TODO(emaxx): Extract this into a more generic helper class for using in other
// tests.
class ExtensionBackgroundPageReadyObserver final {
 public:
  explicit ExtensionBackgroundPageReadyObserver(const std::string& extension_id)
      : extension_id_(extension_id),
        notification_observer_(
            extensions::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
            base::Bind(
                &ExtensionBackgroundPageReadyObserver::IsNotificationRelevant,
                base::Unretained(this))) {}

  void Wait() { notification_observer_.Wait(); }

 private:
  // Callback which is used for |WindowedNotificationObserver| for checking
  // whether the condition being awaited is met.
  bool IsNotificationRelevant(
      const content::NotificationSource& source,
      const content::NotificationDetails& details) const {
    return content::Source<const extensions::Extension>(source)->id() ==
           extension_id_;
  }

  const std::string extension_id_;
  content::WindowedNotificationObserver notification_observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionBackgroundPageReadyObserver);
};

// Class for testing sign-in profile apps/extensions that are installed via the
// device policy under different browser channels.
class SigninProfileExtensionsPolicyPerChannelTest
    : public SigninProfileExtensionsPolicyTestBase,
      public testing::WithParamInterface<version_info::Channel> {
 protected:
  SigninProfileExtensionsPolicyPerChannelTest();

 private:
  DISALLOW_COPY_AND_ASSIGN(SigninProfileExtensionsPolicyPerChannelTest);
};

SigninProfileExtensionsPolicyPerChannelTest::
    SigninProfileExtensionsPolicyPerChannelTest()
    : SigninProfileExtensionsPolicyTestBase(GetParam()) {}

content::StoragePartition* GetStoragePartitionForSigninExtension(
    Profile* profile,
    const std::string& extension_id) {
  const GURL site =
      extensions::util::GetSiteForExtensionId(extension_id, profile);
  return content::BrowserContext::GetStoragePartitionForSite(
      profile, site, /*can_create=*/false);
}

}  // namespace

// Tests that a whitelisted app gets installed on any browser channel.
IN_PROC_BROWSER_TEST_P(SigninProfileExtensionsPolicyPerChannelTest,
                       WhitelistedAppInstallation) {
  Profile* profile = GetInitialProfile();

  extensions::TestExtensionRegistryObserver registry_observer(
      extensions::ExtensionRegistry::Get(profile), kWhitelistedAppId);

  AddExtensionForForceInstallation(kWhitelistedAppId,
                                   kWhitelistedAppUpdateManifestPath);

  registry_observer.WaitForExtensionLoaded();
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
          kWhitelistedAppId);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(extension->is_platform_app());
}

// Tests that a non-whitelisted app is installed only when on Dev, Canary or
// "unknown" (trunk) channels, but not on Beta or Stable channels.
IN_PROC_BROWSER_TEST_P(SigninProfileExtensionsPolicyPerChannelTest,
                       NotWhitelistedAppInstallation) {
  Profile* profile = GetInitialProfile();

  extensions::TestExtensionRegistryObserver registry_observer(
      extensions::ExtensionRegistry::Get(profile), kNotWhitelistedAppId);
  ExtensionInstallErrorObserver install_error_observer(profile,
                                                       kNotWhitelistedAppId);

  AddExtensionForForceInstallation(kNotWhitelistedAppId,
                                   kNotWhitelistedUpdateManifestPath);

  switch (channel_) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV: {
      registry_observer.WaitForExtensionLoaded();
      const extensions::Extension* extension =
          extensions::ExtensionRegistry::Get(profile)
              ->enabled_extensions()
              .GetByID(kNotWhitelistedAppId);
      ASSERT_TRUE(extension);
      EXPECT_TRUE(extension->is_platform_app());
      break;
    }
    case version_info::Channel::BETA:
    case version_info::Channel::STABLE: {
      install_error_observer.Wait();
      EXPECT_FALSE(
          extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
              kNotWhitelistedAppId));
      break;
    }
  }
}

// Tests that a whitelisted extension is installed on any browser channel.
// Force-installed extensions on the sign-in screen should also automatically
// have the |login_screen_extension| type.
IN_PROC_BROWSER_TEST_P(SigninProfileExtensionsPolicyPerChannelTest,
                       WhitelistedExtensionInstallation) {
  Profile* profile = GetInitialProfile();

  extensions::TestExtensionRegistryObserver registry_observer(
      extensions::ExtensionRegistry::Get(profile), kWhitelistedExtensionId);

  AddExtensionForForceInstallation(kWhitelistedExtensionId,
                                   kWhitelistedExtensionUpdateManifestPath);

  registry_observer.WaitForExtensionLoaded();
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
          kWhitelistedExtensionId);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(extension->is_login_screen_extension());
}

// Tests that a non-whitelisted extension (as opposed to an app) is forbidden
// from installation regardless of the browser channel.
IN_PROC_BROWSER_TEST_P(SigninProfileExtensionsPolicyPerChannelTest,
                       NotWhitelistedExtensionInstallation) {
  Profile* profile = GetInitialProfile();

  ExtensionInstallErrorObserver install_error_observer(
      profile, kNotWhitelistedExtensionId);
  AddExtensionForForceInstallation(kNotWhitelistedExtensionId,
                                   kNotWhitelistedExtensionUpdateManifestPath);
  install_error_observer.Wait();
  EXPECT_FALSE(
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          kNotWhitelistedExtensionId));
}

INSTANTIATE_TEST_SUITE_P(,
                         SigninProfileExtensionsPolicyPerChannelTest,
                         testing::Values(version_info::Channel::UNKNOWN,
                                         version_info::Channel::CANARY,
                                         version_info::Channel::DEV,
                                         version_info::Channel::BETA,
                                         version_info::Channel::STABLE));

namespace {

// Class for testing sign-in profile apps/extensions under the "unknown" browser
// channel, which allows to bypass the troublesome whitelist checks.
class SigninProfileExtensionsPolicyTest
    : public SigninProfileExtensionsPolicyTestBase {
 protected:
  SigninProfileExtensionsPolicyTest()
      : SigninProfileExtensionsPolicyTestBase(version_info::Channel::UNKNOWN) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SigninProfileExtensionsPolicyTest);
};

}  // namespace

// Tests that the extension system enables non-standard extensions in the
// sign-in profile.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsPolicyTest, ExtensionsEnabled) {
  EXPECT_TRUE(extensions::ExtensionSystem::Get(GetInitialProfile())
                  ->extension_service()
                  ->extensions_enabled());
}

// Tests that a background page is created for the installed sign-in profile
// app.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsPolicyTest, BackgroundPage) {
  EXPECT_FALSE(
      chromeos::ProfileHelper::SigninProfileHasLoginScreenExtensions());
  ExtensionBackgroundPageReadyObserver page_observer(kNotWhitelistedAppId);
  AddExtensionForForceInstallation(kNotWhitelistedAppId,
                                   kNotWhitelistedUpdateManifestPath);
  page_observer.Wait();
  EXPECT_TRUE(chromeos::ProfileHelper::SigninProfileHasLoginScreenExtensions());
}

// Tests installation of multiple sign-in profile apps.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsPolicyTest, MultipleApps) {
  Profile* profile = GetInitialProfile();

  extensions::TestExtensionRegistryObserver registry_observer1(
      extensions::ExtensionRegistry::Get(profile), kWhitelistedAppId);
  extensions::TestExtensionRegistryObserver registry_observer2(
      extensions::ExtensionRegistry::Get(profile), kNotWhitelistedAppId);

  AddExtensionForForceInstallation(kWhitelistedAppId,
                                   kWhitelistedAppUpdateManifestPath);
  AddExtensionForForceInstallation(kNotWhitelistedAppId,
                                   kNotWhitelistedUpdateManifestPath);

  registry_observer1.WaitForExtensionLoaded();
  registry_observer2.WaitForExtensionLoaded();
}

// Tests that a sign-in profile app or a sign-in profile extension has isolated
// storage, i.e. that it does not reuse the Profile's default StoragePartition.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsPolicyTest,
                       IsolatedStoragePartition) {
  Profile* profile = GetInitialProfile();

  ExtensionBackgroundPageReadyObserver page_observer_for_app(kWhitelistedAppId);
  ExtensionBackgroundPageReadyObserver page_observer_for_extension(
      kWhitelistedExtensionId);

  AddExtensionForForceInstallation(kWhitelistedAppId,
                                   kWhitelistedAppUpdateManifestPath);
  AddExtensionForForceInstallation(kWhitelistedExtensionId,
                                   kWhitelistedExtensionUpdateManifestPath);

  page_observer_for_app.Wait();
  page_observer_for_extension.Wait();

  content::StoragePartition* storage_partition_for_app =
      GetStoragePartitionForSigninExtension(profile, kWhitelistedAppId);
  content::StoragePartition* storage_partition_for_extension =
      GetStoragePartitionForSigninExtension(profile, kWhitelistedExtensionId);
  content::StoragePartition* default_storage_partition =
      content::BrowserContext::GetDefaultStoragePartition(profile);

  ASSERT_TRUE(storage_partition_for_app);
  ASSERT_TRUE(storage_partition_for_extension);
  ASSERT_TRUE(default_storage_partition);

  EXPECT_NE(default_storage_partition, storage_partition_for_app);
  EXPECT_NE(default_storage_partition, storage_partition_for_extension);
  EXPECT_NE(storage_partition_for_app, storage_partition_for_extension);
}

}  // namespace policy
