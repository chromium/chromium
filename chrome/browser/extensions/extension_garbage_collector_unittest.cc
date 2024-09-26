// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_garbage_collector.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "ppapi/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#endif

namespace extensions {

class ExtensionGarbageCollectorUnitTest : public ExtensionServiceTestBase {
 protected:
  void InitPluginService() {
#if BUILDFLAG(ENABLE_PLUGINS)
    content::PluginService::GetInstance()->Init();
#endif
  }

  // A delayed task to call GarbageCollectExtensions is posted by
  // ExtensionGarbageCollector's constructor. But, as the test won't wait for
  // the delayed task to be called, we have to call it manually instead.
  void GarbageCollectExtensions() {
    ExtensionGarbageCollector::Get(profile_.get())
        ->GarbageCollectExtensionsForTest();
    // Wait for GarbageCollectExtensions task to complete.
    content::RunAllTasksUntilIdle();
  }
};

// TODO(crbug.com/40875193): The test extension good_juKvIh seems to error on
// install with "Manifest file is missing or unreadable" despite the manifest
// being valid. This test case is still valid because we're only checking if the
// files get deleted. The files get copied to the install directory by the test
// infra despite the installation failure. So we should probably fix this in the
// future so that this test extension can be used in other tests.

// Test that partially deleted unpacked extensions (e.g. from .zips) are cleaned
// up during startup.
TEST_F(ExtensionGarbageCollectorUnitTest,
       CleanupUnpackedOnStartup_DeleteWhenNoLongerInstalled) {
  const ExtensionId kExtensionId = "lckcjklfapeiadkadngidmocpbkemckm";

  InitPluginService();
  InitializeGoodInstalledExtensionService();
  base::FilePath zipped_extension_dir =
      unpacked_install_dir().AppendASCII("good_juKvIh");
  ASSERT_TRUE(base::PathExists(zipped_extension_dir));

  // Simulate that the extensions was partially deleted (no longer considered
  // installed) by clearing its pref.
  {
    ScopedDictPrefUpdate update(profile_->GetPrefs(), pref_names::kExtensions);
    update->Remove(kExtensionId);
  }

  service_->Init();
  GarbageCollectExtensions();

  base::FileEnumerator dirs(unpacked_install_dir(),
                            false,  // not recursive
                            base::FileEnumerator::DIRECTORIES);

  // We should have have zero extensions now.
  EXPECT_TRUE(dirs.Next().empty());

  // And unpacked extension dir should now be toast.
  EXPECT_FALSE(base::PathExists(zipped_extension_dir));
}

TEST_F(ExtensionGarbageCollectorUnitTest,
       CleanupUnpackedOnStartup_DoNotDeleteWhenStillInstalled) {
  const ExtensionId kExtensionId = "lckcjklfapeiadkadngidmocpbkemckm";

  InitPluginService();
  InitializeGoodInstalledExtensionService();
  base::FilePath zipped_extension_dir =
      unpacked_install_dir().AppendASCII("good_juKvIh");
  ASSERT_TRUE(base::PathExists(zipped_extension_dir));

  // Update the path of the installed extension to be accurate for the test.
  {
    ScopedDictPrefUpdate update(profile_->GetPrefs(), pref_names::kExtensions);
    base::Value::Dict& update_dict = update.Get();
    // An unpacked extension installed in the profile dir in production usually
    // has it's full install path written to the "path" key, but since we don't
    // know what the path is during the test (due to variation of test directory
    // location) we need to manually set it during the test. The garbage
    // collection checks this path to determine whether to delete the
    // installation directory.
    base::Value::Dict* extension_entry = update_dict.FindDict(kExtensionId);
    ASSERT_TRUE(extension_entry);
    extension_entry->Set("path",
                         base::Value(zipped_extension_dir.MaybeAsASCII()));
  }

  service_->Init();
  GarbageCollectExtensions();

  // Unpacked extension dir should not be deleted.
  EXPECT_TRUE(base::PathExists(zipped_extension_dir));
}

// Test that garbage collection doesn't delete anything while a crx is being
// installed.
TEST_F(ExtensionGarbageCollectorUnitTest, NoCleanupDuringInstall) {
  const ExtensionId kExtensionId = "behllobkkfkfnphdnhnkndlbkcpglgmj";

  InitPluginService();
  InitializeGoodInstalledExtensionService();

  // Simulate that one of them got partially deleted by clearing its pref.
  {
    ScopedDictPrefUpdate update(profile_->GetPrefs(), pref_names::kExtensions);
    update->Remove(kExtensionId);
  }

  service_->Init();

  // Simulate a CRX installation.
  auto installer = CrxInstaller::CreateSilent(service_);
  InstallTracker::Get(profile_.get())
      ->OnBeginCrxInstall(*installer, kExtensionId);

  GarbageCollectExtensions();

  // extension1 dir should still exist.
  base::FilePath extension_dir =
      extensions_install_dir().AppendASCII(kExtensionId);
  ASSERT_TRUE(base::PathExists(extension_dir));

  // Finish CRX installation and re-run garbage collection.
  InstallTracker::Get(profile_.get())
      ->OnFinishCrxInstall(*installer, kExtensionId, false);
  GarbageCollectExtensions();

  // extension1 dir should be gone
  ASSERT_FALSE(base::PathExists(extension_dir));
}

// Test that GarbageCollectExtensions deletes the right versions of an
// extension.
TEST_F(ExtensionGarbageCollectorUnitTest, GarbageCollectWithPendingUpdates) {
  InitPluginService();

  ExtensionServiceInitParams params;
  ASSERT_TRUE(params.ConfigureByTestDataDirectory(
      data_dir().AppendASCII("pending_updates")));
  InitializeExtensionService(std::move(params));

  // This is the directory that is going to be deleted, so make sure it actually
  // is there before the garbage collection.
  ASSERT_TRUE(base::PathExists(extensions_install_dir().AppendASCII(
      "hpiknbiabeeppbpihjehijgoemciehgk/3")));

  GarbageCollectExtensions();

  // Verify that the pending update for the first extension didn't get
  // deleted.
  EXPECT_TRUE(base::PathExists(extensions_install_dir().AppendASCII(
      "bjafgdebaacbbbecmhlhpofkepfkgcpa/1.0")));
  EXPECT_TRUE(base::PathExists(extensions_install_dir().AppendASCII(
      "bjafgdebaacbbbecmhlhpofkepfkgcpa/2.0")));
  EXPECT_TRUE(base::PathExists(extensions_install_dir().AppendASCII(
      "hpiknbiabeeppbpihjehijgoemciehgk/2")));
  EXPECT_FALSE(base::PathExists(extensions_install_dir().AppendASCII(
      "hpiknbiabeeppbpihjehijgoemciehgk/3")));
}

// Test that pending updates are properly handled on startup.
TEST_F(ExtensionGarbageCollectorUnitTest, UpdateOnStartup) {
  InitPluginService();

  ExtensionServiceInitParams params;
  ASSERT_TRUE(params.ConfigureByTestDataDirectory(
      data_dir().AppendASCII("pending_updates")));
  InitializeExtensionService(std::move(params));

  // This is the directory that is going to be deleted, so make sure it actually
  // is there before the garbage collection.
  ASSERT_TRUE(base::PathExists(extensions_install_dir().AppendASCII(
      "hpiknbiabeeppbpihjehijgoemciehgk/3")));

  service_->Init();
  GarbageCollectExtensions();

  // Verify that the pending update for the first extension got installed.
  EXPECT_FALSE(base::PathExists(extensions_install_dir().AppendASCII(
      "bjafgdebaacbbbecmhlhpofkepfkgcpa/1.0")));
  EXPECT_TRUE(base::PathExists(extensions_install_dir().AppendASCII(
      "bjafgdebaacbbbecmhlhpofkepfkgcpa/2.0")));
  EXPECT_TRUE(base::PathExists(extensions_install_dir().AppendASCII(
      "hpiknbiabeeppbpihjehijgoemciehgk/2")));
  EXPECT_FALSE(base::PathExists(extensions_install_dir().AppendASCII(
      "hpiknbiabeeppbpihjehijgoemciehgk/3")));

  // Make sure update information got deleted.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_.get());
  EXPECT_FALSE(
      prefs->GetDelayedInstallInfo("bjafgdebaacbbbecmhlhpofkepfkgcpa"));
}

}  // namespace extensions
