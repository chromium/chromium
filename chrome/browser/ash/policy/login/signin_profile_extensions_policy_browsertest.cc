// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/constants/ash_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "chrome/browser/ash/policy/login/signin_profile_extensions_policy_test_base.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_observer.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/update_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
class StoragePartition;
}

namespace policy {

namespace {

// Parameters for the several extensions and apps that are used by the tests in
// this file (note that the paths are given relative to the src/chrome/test/data
// directory):
// * The manual testing app which is allowlisted for running in the sign-in
//   profile:
const char kAllowlistedAppId[] = "bjaiihebfngildkcjkjckolinodhliff";
const char kAllowlistedAppCrxPath[] =
    "extensions/signin_screen_manual_test_app/app_signed_by_webstore.crx";
// * A trivial test app which is NOT allowlisted for running in the sign-in
//   profile:
const char kNotAllowlistedAppId[] = "mockapnacjbcdncmpkjngjalkhphojek";
const char kNotAllowlistedAppPath[] = "extensions/trivial_platform_app/app/";
const char kNotAllowlistedAppPemPath[] =
    "extensions/trivial_platform_app/app.pem";
// * A trivial test extension which is allowlisted for running in the sign-in
//   profile:
const char kAllowlistedExtensionId[] = "ngjobkbdodapjbbncmagbccommkggmnj";
const char kAllowlistedExtensionCrxPath[] =
    "extensions/signin_screen_manual_test_extension/"
    "extension_signed_by_webstore.crx";
// * A trivial test extension which is NOT allowlisted for running in the
//   sign-in profile:
const char kNotAllowlistedExtensionId[] = "mockepjebcnmhmhcahfddgfcdgkdifnc";
const char kNotAllowlistedExtensionPath[] =
    "extensions/trivial_extension/extension/";
const char kNotAllowlistedExtensionPemPath[] =
    "extensions/trivial_extension/extension.pem";
// * An extension which is NOT allowlisted for running in the sign-in profile
//   and that suppresses its immediate auto updates:
const char kNoImmediateUpdateExtensionId[] = "noidlplbgmdmbccnafgibfgokggdpncj";
const char kNoImmediateUpdateExtensionPathTemplate[] =
    "extensions/no_immediate_update_extension/src-%s";
const char kNoImmediateUpdateExtensionPemPath[] =
    "extensions/no_immediate_update_extension/key.pem";
const char kNoImmediateUpdateExtensionLatestVersion[] = "2.0";
const char kNoImmediateUpdateExtensionOlderVersion[] = "1.0";

// Returns the path to the no_immediate_update_extension with the given version.
base::FilePath GetNoImmediateUpdateExtensionPath(const std::string& version) {
  return base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
      .AppendASCII(base::StringPrintf(kNoImmediateUpdateExtensionPathTemplate,
                                      version.c_str()));
}

// Observer that allows waiting for an installation failure of a specific
// extension/app.
class ExtensionInstallErrorObserver : public extensions::InstallObserver {
 public:
  ExtensionInstallErrorObserver(Profile* profile,
                                const std::string& extension_id)
      : extension_id_(extension_id) {
    auto* tracker = extensions::InstallTracker::Get(profile);
    CHECK(tracker);
    observation_.Observe(tracker);
  }

  ExtensionInstallErrorObserver(const ExtensionInstallErrorObserver&) = delete;
  ExtensionInstallErrorObserver& operator=(
      const ExtensionInstallErrorObserver&) = delete;

  void Wait() { run_loop_.Run(); }

  // extensions::InstallObserver:
  void OnFinishCrxInstall(content::BrowserContext* context,
                          const extensions::CrxInstaller& installer,
                          const std::string& extension_id,
                          bool success) override {
    if (extension_id == extension_id_) {
      run_loop_.Quit();
    }
  }

 private:
  base::RunLoop run_loop_;
  const extensions::ExtensionId extension_id_;
  base::ScopedObservation<extensions::InstallTracker, InstallObserver>
      observation_{this};
};

// Observer that allows waiting until the specified version of the given
// extension/app gets available for an update.
class ExtensionUpdateAvailabilityObserver final
    : public extensions::UpdateObserver {
 public:
  ExtensionUpdateAvailabilityObserver(Profile* profile,
                                      const std::string& extension_id,
                                      const base::Version& awaited_version)
      : profile_(profile),
        extension_id_(extension_id),
        awaited_version_(awaited_version) {
    extensions::ExtensionSystem::Get(profile_)
        ->extension_service()
        ->AddUpdateObserver(this);
  }

  ExtensionUpdateAvailabilityObserver(
      const ExtensionUpdateAvailabilityObserver&) = delete;
  ExtensionUpdateAvailabilityObserver& operator=(
      const ExtensionUpdateAvailabilityObserver&) = delete;

  ~ExtensionUpdateAvailabilityObserver() override {
    extensions::ExtensionSystem::Get(profile_)
        ->extension_service()
        ->RemoveUpdateObserver(this);
  }

  // Should be called no more than once.
  void Wait() {
    // Note that the expected event could have already been observed before this
    // point, in which case the run loop will exit immediately.
    run_loop_.Run();
  }

  void OnAppUpdateAvailable(const extensions::Extension* extension) override {
    if (extension->id() == extension_id_ &&
        extension->version() == awaited_version_) {
      run_loop_.Quit();
    }
  }

  void OnChromeUpdateAvailable() override {}

 private:
  const raw_ptr<Profile> profile_;
  const extensions::ExtensionId extension_id_;
  const base::Version awaited_version_;
  base::RunLoop run_loop_;
};

// Class for testing sign-in profile apps/extensions.
class SigninProfileExtensionsPolicyTest
    : public SigninProfileExtensionsPolicyTestBase {
 public:
  SigninProfileExtensionsPolicyTest(const SigninProfileExtensionsPolicyTest&) =
      delete;
  SigninProfileExtensionsPolicyTest& operator=(
      const SigninProfileExtensionsPolicyTest&) = delete;

 protected:
  SigninProfileExtensionsPolicyTest()
      : SigninProfileExtensionsPolicyTestBase(version_info::Channel::STABLE) {}

  void SetUpOnMainThread() override {
    SigninProfileExtensionsPolicyTestBase::SetUpOnMainThread();

    extension_force_install_mixin_.InitWithDevicePolicyCrosTestHelper(
        GetInitialProfile(), policy_helper());
  }

  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};
};

}  // namespace

// Tests that a allowlisted app gets installed.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsPolicyTest,
                       AllowlistedAppInstallation) {
  EXPECT_TRUE(extension_force_install_mixin_.ForceInstallFromCrx(
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kAllowlistedAppCrxPath),
      ExtensionForceInstallMixin::WaitMode::kLoad));
  const extensions::Extension* extension =
      extension_force_install_mixin_.GetEnabledExtension(kAllowlistedAppId);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(extension->is_platform_app());
}

// Tests that a non-allowlisted app is forbidden from installation.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsPolicyTest,
                       NotAllowlistedAppInstallation) {
  Profile* profile = GetInitialProfile();

  ExtensionInstallErrorObserver install_error_observer(profile,
                                                       kNotAllowlistedAppId);
  EXPECT_TRUE(extension_force_install_mixin_.ForceInstallFromSourceDir(
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kNotAllowlistedAppPath),
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kNotAllowlistedAppPemPath),
      ExtensionForceInstallMixin::WaitMode::kNone));
  install_error_observer.Wait();
  EXPECT_FALSE(extension_force_install_mixin_.GetInstalledExtension(
      kNotAllowlistedAppId));
}

// Tests that a allowlisted extension is installed. Force-installed extensions
// on the sign-in screen should also automatically have the
// |login_screen_extension| type.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsPolicyTest,
                       AllowlistedExtensionInstallation) {
  EXPECT_TRUE(extension_force_install_mixin_.ForceInstallFromCrx(
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kAllowlistedExtensionCrxPath),
      ExtensionForceInstallMixin::WaitMode::kLoad));

  const extensions::Extension* extension =
      extension_force_install_mixin_.GetEnabledExtension(
          kAllowlistedExtensionId);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(extension->is_login_screen_extension());
}

// Tests that a non-allowlisted extension is forbidden from installation.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsPolicyTest,
                       NotAllowlistedExtensionInstallation) {
  Profile* profile = GetInitialProfile();

  ExtensionInstallErrorObserver install_error_observer(
      profile, kNotAllowlistedExtensionId);
  EXPECT_TRUE(extension_force_install_mixin_.ForceInstallFromSourceDir(
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kNotAllowlistedExtensionPath),
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kNotAllowlistedExtensionPemPath),
      ExtensionForceInstallMixin::WaitMode::kNone));
  install_error_observer.Wait();
  EXPECT_FALSE(extension_force_install_mixin_.GetInstalledExtension(
      kNotAllowlistedExtensionId));
}

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
  EXPECT_TRUE(extension_force_install_mixin_.ForceInstallFromCrx(
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kAllowlistedAppCrxPath),
      ExtensionForceInstallMixin::WaitMode::kBackgroundPageFirstLoad));
}

// Tests installation of multiple sign-in profile apps/extensions.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsPolicyTest,
                       MultipleAppsOrExtensions) {
  EXPECT_TRUE(extension_force_install_mixin_.ForceInstallFromCrx(
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kAllowlistedAppCrxPath),
      ExtensionForceInstallMixin::WaitMode::kLoad));
  EXPECT_TRUE(extension_force_install_mixin_.ForceInstallFromCrx(
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kAllowlistedExtensionCrxPath),
      ExtensionForceInstallMixin::WaitMode::kLoad));

  EXPECT_TRUE(
      extension_force_install_mixin_.GetEnabledExtension(kAllowlistedAppId));
  EXPECT_TRUE(extension_force_install_mixin_.GetEnabledExtension(
      kAllowlistedExtensionId));
}

// Tests that a sign-in profile app or a sign-in profile extension has isolated
// storage, i.e. that it does not reuse the Profile's default StoragePartition.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsPolicyTest,
                       IsolatedStoragePartition) {
  Profile* profile = GetInitialProfile();

  EXPECT_TRUE(extension_force_install_mixin_.ForceInstallFromCrx(
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kAllowlistedAppCrxPath),
      ExtensionForceInstallMixin::WaitMode::kBackgroundPageFirstLoad));
  EXPECT_TRUE(extension_force_install_mixin_.ForceInstallFromCrx(
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII(kAllowlistedExtensionCrxPath),
      ExtensionForceInstallMixin::WaitMode::kLoad));

  content::StoragePartition* storage_partition_for_app =
      extensions::util::GetStoragePartitionForExtensionId(
          kAllowlistedAppId, profile, /*can_create=*/false);
  content::StoragePartition* storage_partition_for_extension =
      extensions::util::GetStoragePartitionForExtensionId(
          kAllowlistedExtensionId, profile, /*can_create=*/false);
  content::StoragePartition* default_storage_partition =
      profile->GetDefaultStoragePartition();

  ASSERT_TRUE(storage_partition_for_app);
  ASSERT_TRUE(storage_partition_for_extension);
  ASSERT_TRUE(default_storage_partition);

  EXPECT_NE(default_storage_partition, storage_partition_for_app);
  EXPECT_NE(default_storage_partition, storage_partition_for_extension);
  EXPECT_NE(storage_partition_for_app, storage_partition_for_extension);
}

// Class for testing the sign-in profile extensions with the simulated absence
// of network connectivity.
class SigninProfileExtensionsPolicyOfflineLaunchTest
    : public SigninProfileExtensionsPolicyTest {
 protected:
  SigninProfileExtensionsPolicyOfflineLaunchTest() {
    // In the non-PRE test, this simulates inability to make network requests
    // for fetching the extension update manifest and CRX files. In the PRE test
    // the server is not hung, allowing the initial installation of the
    // extension.
    if (!content::IsPreTest()) {
      extension_force_install_mixin_.SetServerErrorMode(
          ExtensionForceInstallMixin::ServerErrorMode::kHung);
    }
  }

  void SetUpOnMainThread() override {
    SigninProfileExtensionsPolicyTest::SetUpOnMainThread();

    test_extension_registry_observer_ =
        std::make_unique<extensions::TestExtensionRegistryObserver>(
            extensions::ExtensionRegistry::Get(GetInitialProfile()),
            kAllowlistedAppId);

    EXPECT_TRUE(extension_force_install_mixin_.ForceInstallFromCrx(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
            .AppendASCII(kAllowlistedAppCrxPath),
        ExtensionForceInstallMixin::WaitMode::kNone));
  }

  void TearDownOnMainThread() override {
    test_extension_registry_observer_.reset();

    SigninProfileExtensionsPolicyTest::TearDownOnMainThread();
  }

  void WaitForTestExtensionLoaded() {
    test_extension_registry_observer_->WaitForExtensionLoaded();
  }

 private:
  std::unique_ptr<extensions::TestExtensionRegistryObserver>
      test_extension_registry_observer_;
};

// This is the preparation step for the actual test. Here the allowlisted app
// gets installed into the sign-in profile.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsPolicyOfflineLaunchTest,
                       PRE_Test) {
  WaitForTestExtensionLoaded();
}

// Tests that the allowlisted app gets launched using the cached version even
// when there's no network connection (i.e., neither the extension update
// manifest nor the CRX file can be fetched during this browser execution).
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsPolicyOfflineLaunchTest, Test) {
  WaitForTestExtensionLoaded();
}

// Class for testing the sign-in profile extensions with a corrupt cache file.
class SigninProfileExtensionsPolicyCorruptCacheTest
    : public SigninProfileExtensionsPolicyTest {
 protected:
  void SetUpOnMainThread() override {
    SigninProfileExtensionsPolicyTest::SetUpOnMainThread();

    test_extension_registry_observer_ =
        std::make_unique<extensions::TestExtensionRegistryObserver>(
            extensions::ExtensionRegistry::Get(GetInitialProfile()),
            kAllowlistedAppId);

    EXPECT_TRUE(extension_force_install_mixin_.ForceInstallFromCrx(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
            .AppendASCII(kAllowlistedAppCrxPath),
        ExtensionForceInstallMixin::WaitMode::kNone, &installed_extension_id_,
        &installed_extension_version_));
  }

  void TearDownOnMainThread() override {
    test_extension_registry_observer_.reset();

    SigninProfileExtensionsPolicyTest::TearDownOnMainThread();
  }

  void WaitForTestExtensionLoaded() {
    test_extension_registry_observer_->WaitForExtensionLoaded();
  }

  base::FilePath GetCachedCrxFilePath() const {
    const base::FilePath cache_file_path =
        base::PathService::CheckedGet(ash::DIR_SIGNIN_PROFILE_EXTENSIONS);
    const std::string file_name =
        base::StringPrintf("%s-%s.crx", installed_extension_id_.c_str(),
                           installed_extension_version_.GetString().c_str());
    return cache_file_path.AppendASCII(file_name);
  }

 private:
  std::unique_ptr<extensions::TestExtensionRegistryObserver>
      test_extension_registry_observer_;

  extensions::ExtensionId installed_extension_id_;
  base::Version installed_extension_version_;
};

// This is the preparation step for the actual test. Here the allowlisted app
// gets installed into the sign-in profile and then the cached .crx file get
// corrupted.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsPolicyCorruptCacheTest,
                       PRE_ExtensionIsInstalledAfterCorruption) {
  WaitForTestExtensionLoaded();

  // Manually corrupt file. The directory for cached extensions
  // (|DIR_SIGNIN_PROFILE_EXTENSIONS|) is overridden with a new temp directory
  // for every test run (see RegisterStubPathOverrides()) so this does not
  // affect any other tests.
  base::ScopedAllowBlockingForTesting scoped_allowed_blocking_for_testing;
  const base::FilePath cached_crx_file_path = GetCachedCrxFilePath();
  ASSERT_TRUE(PathExists(cached_crx_file_path));
  ASSERT_TRUE(base::WriteFile(cached_crx_file_path, "random-data"));
}

// Tests that the allowlisted app still gets installed correctly, even if the
// existing file in cache is corrupted.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsPolicyCorruptCacheTest,
                       ExtensionIsInstalledAfterCorruption) {
  WaitForTestExtensionLoaded();
}

// Class for testing the auto update of the sign-in profile extensions.
class SigninProfileExtensionsAutoUpdatePolicyTest
    : public SigninProfileExtensionsPolicyTest {
 public:
  SigninProfileExtensionsAutoUpdatePolicyTest() {
    // Initially block the server that hosts the extension. This is to let the
    // test bodies simulate the offline scenario or to let them make the updated
    // manifest seen by the extension system quickly (background: once it
    // fetches a manifest for the first time, the next check will happen after a
    // very long delay, which would make the test time out).
    extension_force_install_mixin_.SetServerErrorMode(
        ExtensionForceInstallMixin::ServerErrorMode::kInternalError);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SigninProfileExtensionsPolicyTest::SetUpCommandLine(command_line);
    // Allow the test extension to be run on the login screen despite not being
    // conformant to the "signin_screen" behavior feature.
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID,
        kNoImmediateUpdateExtensionId);
  }

  void SetUpOnMainThread() override {
    SigninProfileExtensionsPolicyTest::SetUpOnMainThread();

    test_extension_registry_observer_ =
        std::make_unique<extensions::TestExtensionRegistryObserver>(
            extensions::ExtensionRegistry::Get(GetInitialProfile()),
            kNoImmediateUpdateExtensionId);
    test_extension_latest_version_update_available_observer_ =
        std::make_unique<ExtensionUpdateAvailabilityObserver>(
            GetInitialProfile(), kNoImmediateUpdateExtensionId,
            base::Version(kNoImmediateUpdateExtensionLatestVersion));

    const std::string version = content::IsPreTest()
                                    ? kNoImmediateUpdateExtensionOlderVersion
                                    : kNoImmediateUpdateExtensionLatestVersion;
    EXPECT_TRUE(extension_force_install_mixin_.ForceInstallFromSourceDir(
        GetNoImmediateUpdateExtensionPath(version),
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
            .AppendASCII(kNoImmediateUpdateExtensionPemPath),
        ExtensionForceInstallMixin::WaitMode::kNone));
  }

  void TearDownOnMainThread() override {
    test_extension_latest_version_update_available_observer_.reset();
    test_extension_registry_observer_.reset();
    SigninProfileExtensionsPolicyTest::TearDownOnMainThread();
  }

  void WaitForTestExtensionLoaded() {
    test_extension_registry_observer_->WaitForExtensionLoaded();
  }

  void WaitForTestExtensionLatestVersionUpdateAvailable() {
    test_extension_latest_version_update_available_observer_->Wait();
  }

  base::Version GetTestExtensionVersion() const {
    const extensions::Extension* const extension =
        extension_force_install_mixin_.GetEnabledExtension(
            kNoImmediateUpdateExtensionId);
    if (!extension)
      return base::Version();
    return extension->version();
  }

 private:
  std::unique_ptr<extensions::TestExtensionRegistryObserver>
      test_extension_registry_observer_;
  std::unique_ptr<ExtensionUpdateAvailabilityObserver>
      test_extension_latest_version_update_available_observer_;
};

// This is the first preparation step for the actual test. Here the old version
// of the extension is served, and it gets installed into the sign-in profile.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsAutoUpdatePolicyTest,
                       PRE_PRE_Test) {
  // Unblock the server hosting the extension immediately.
  extension_force_install_mixin_.SetServerErrorMode(
      ExtensionForceInstallMixin::ServerErrorMode::kNone);

  WaitForTestExtensionLoaded();
  EXPECT_EQ(GetTestExtensionVersion(),
            base::Version(kNoImmediateUpdateExtensionOlderVersion));
}

// This is the second preparation step for the actual test. Here the new version
// of the extension is served, and it gets fetched and cached.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsAutoUpdatePolicyTest, PRE_Test) {
  // Let the extensions system load the previously fetched version before
  // starting to serve the newer version, to avoid hitting flaky DCHECKs in the
  // extensions system internals (see https://crbug.com/810799).
  WaitForTestExtensionLoaded();
  EXPECT_EQ(GetTestExtensionVersion(),
            base::Version(kNoImmediateUpdateExtensionOlderVersion));

  // Start serving the newer version. The extensions system should eventually
  // fetch this version due to the retry mechanism when the fetch request to the
  // update servers was failing. We verify that the new version eventually gets
  // fetched and becomes available for an update.
  EXPECT_TRUE(extension_force_install_mixin_.UpdateFromSourceDir(
      GetNoImmediateUpdateExtensionPath(
          kNoImmediateUpdateExtensionLatestVersion),
      kNoImmediateUpdateExtensionId,
      ExtensionForceInstallMixin::UpdateWaitMode::kNone));
  extension_force_install_mixin_.SetServerErrorMode(
      ExtensionForceInstallMixin::ServerErrorMode::kNone);
  WaitForTestExtensionLatestVersionUpdateAvailable();

  // The running extension should stay at the older version, since it ignores
  // update notifications and never idles, and also the browser is expected to
  // not force immediate updates.
  // Note: There's no reliable way to test that the preliminary autoupdate
  // doesn't happen, but doing RunUntilIdle() at this point should make the test
  // at least flaky in case a bug is introduced somewhere.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetTestExtensionVersion(),
            base::Version(kNoImmediateUpdateExtensionOlderVersion));
}

// This is the actual test. Here we verify that the new version of the
// extension, as fetched in the PRE_Test, gets launched even in the "offline"
// mode (since the server hosting the extension stays in the error mode
// throughout this part).
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsAutoUpdatePolicyTest, Test) {
  WaitForTestExtensionLoaded();
  EXPECT_EQ(GetTestExtensionVersion(),
            base::Version(kNoImmediateUpdateExtensionLatestVersion));
}

}  // namespace policy
