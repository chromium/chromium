// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_restore_service.h"

#include "apps/app_restore_service_factory.h"
#include "apps/saved_files_service.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/file_system/file_system_api.h"
#include "extensions/browser/api/file_system/saved_file_entry.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"

using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionSystem;
using extensions::FileSystemChooseEntryFunction;
using extensions::SavedFileEntry;

// TODO(benwells): Move PlatformAppBrowserTest to apps namespace in apps
// component.
using extensions::PlatformAppBrowserTest;

namespace apps {

// Tests that a running app is recorded in the preferences as such.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, RunningAppsAreRecorded) {
  extensions::ExtensionHostTestHelper host_helper(profile());
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("platform_apps/restart_test"));
  ASSERT_TRUE(extension);
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(browser()->profile());

  // App is running.
  ASSERT_TRUE(extension_prefs->IsExtensionRunning(extension->id()));

  // Wait for the extension to get suspended.
  host_helper.WaitForHostDestroyed();

  // App isn't running because it got suspended.
  ASSERT_FALSE(extension_prefs->IsExtensionRunning(extension->id()));

  // Pretend that the app is supposed to be running.
  extension_prefs->SetExtensionRunning(extension->id(), true);

  ExtensionTestMessageListener restart_listener("onRestarted");
  apps::AppRestoreServiceFactory::GetForBrowserContext(browser()->profile())
      ->HandleStartup(true);
  EXPECT_TRUE(restart_listener.WaitUntilSatisfied());
}

// Tests that apps are recorded in the preferences as active when and only when
// they have visible windows.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, ActiveAppsAreRecorded) {
  ExtensionTestMessageListener ready_listener("ready",
                                              ReplyBehavior::kWillReply);
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("platform_apps/active_test"));
  ASSERT_TRUE(extension);
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(browser()->profile());
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // Open a visible window and check the app is marked active.
  ready_listener.Reply("create");
  ready_listener.Reset();
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  ASSERT_TRUE(extension_prefs->IsActive(extension->id()));

  // Close the window, then open a minimized window and check the app is active.
  ready_listener.Reply("closeLastWindow");
  ready_listener.Reset();
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  ready_listener.Reply("createMinimized");
  ready_listener.Reset();
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  ASSERT_TRUE(extension_prefs->IsActive(extension->id()));

  // Close the window, then open a hidden window and check the app is not
  // marked active.
  ready_listener.Reply("closeLastWindow");
  ready_listener.Reset();
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  ready_listener.Reply("createHidden");
  ready_listener.Reset();
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  ASSERT_FALSE(extension_prefs->IsActive(extension->id()));

  // Open another window and check the app is marked active.
  ready_listener.Reply("create");
  ready_listener.Reset();
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  ASSERT_TRUE(extension_prefs->IsActive(extension->id()));

  // Close the visible window and check the app has been marked inactive.
  ready_listener.Reply("closeLastWindow");
  ready_listener.Reset();
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  ASSERT_FALSE(extension_prefs->IsActive(extension->id()));

  // Close the last window and exit.
  ready_listener.Reply("closeLastWindow");
  ready_listener.Reset();
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  ready_listener.Reply("exit");
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, FileAccessIsSavedToPrefs) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::FilePath temp_file;
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_directory.GetPath(), &temp_file));

  const FileSystemChooseEntryFunction::TestOptions test_options{
      .path_to_be_picked = &temp_file};
  auto reset_options =
      FileSystemChooseEntryFunction::SetOptionsForTesting(test_options);
  FileSystemChooseEntryFunction::RegisterTempExternalFileSystemForTest(
      "temp", temp_directory.GetPath());

  extensions::ExtensionHostTestHelper host_helper(profile());
  const Extension* extension = LoadAndLaunchPlatformApp(
      "file_access_saved_to_prefs_test", "fileWritten");
  ASSERT_TRUE(extension);

  SavedFilesService* saved_files_service = SavedFilesService::Get(profile());

  std::vector<SavedFileEntry> file_entries =
      saved_files_service->GetAllFileEntries(extension->id());
  // One for the read-only file entry and one for the writable file entry.
  ASSERT_EQ(2u, file_entries.size());

  host_helper.WaitForHostDestroyed();
  file_entries = saved_files_service->GetAllFileEntries(extension->id());
  // File entries should be cleared when the extension is suspended.
  ASSERT_TRUE(file_entries.empty());
}

// Flaky: crbug.com/269613
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
#define MAYBE_FileAccessIsRestored DISABLED_FileAccessIsRestored
#else
#define MAYBE_FileAccessIsRestored FileAccessIsRestored
#endif

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, MAYBE_FileAccessIsRestored) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::FilePath temp_file;
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_directory.GetPath(), &temp_file));

  const FileSystemChooseEntryFunction::TestOptions test_options{
      .path_to_be_picked = &temp_file};
  auto reset_options =
      FileSystemChooseEntryFunction::SetOptionsForTesting(test_options);
  FileSystemChooseEntryFunction::RegisterTempExternalFileSystemForTest(
      "temp", temp_directory.GetPath());

  extensions::ExtensionHostTestHelper host_helper(profile());
  ExtensionTestMessageListener access_ok_listener("restartedFileAccessOK");
  const Extension* extension =
      LoadAndLaunchPlatformApp("file_access_restored_test", "fileWritten");
  ASSERT_TRUE(extension);

  ExtensionPrefs* extension_prefs =
      ExtensionPrefs::Get(browser()->profile());
  SavedFilesService* saved_files_service = SavedFilesService::Get(profile());
  std::vector<SavedFileEntry> file_entries =
      saved_files_service->GetAllFileEntries(extension->id());
  host_helper.WaitForHostDestroyed();

  // Simulate a restart by populating the preferences as if the browser didn't
  // get time to clean itself up.
  extension_prefs->SetExtensionRunning(extension->id(), true);
  for (std::vector<SavedFileEntry>::const_iterator it = file_entries.begin();
       it != file_entries.end(); ++it) {
    saved_files_service->RegisterFileEntry(
        extension->id(), it->id, it->path, it->is_directory);
  }

  apps::AppRestoreServiceFactory::GetForBrowserContext(browser()->profile())
      ->HandleStartup(true);

  EXPECT_TRUE(access_ok_listener.WaitUntilSatisfied());
}

}  // namespace apps
