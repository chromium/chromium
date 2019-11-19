// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sync_data.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sync/model/fake_sync_change_processor.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync/protocol/extension_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/url_request/test_url_request_interceptor.h"

using content::BrowserThread;
using extensions::Extension;
using extensions::ExtensionRegistry;
using extensions::ExtensionPrefs;
using extensions::ExtensionSyncData;

class ExtensionDisabledGlobalErrorTest
    : public extensions::ExtensionBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kAppsGalleryUpdateURL,
                                    "http://localhost/autoupdate/updates.xml");
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
    EXPECT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    const base::FilePath test_dir =
        test_data_dir_.AppendASCII("permissions_increase");
    const base::FilePath pem_path = test_dir.AppendASCII("permissions.pem");
    path_v1_ = PackExtensionWithOptions(
        test_dir.AppendASCII("v1"),
        scoped_temp_dir_.GetPath().AppendASCII("permissions1.crx"), pem_path,
        base::FilePath());
    path_v2_ = PackExtensionWithOptions(
        test_dir.AppendASCII("v2"),
        scoped_temp_dir_.GetPath().AppendASCII("permissions2.crx"), pem_path,
        base::FilePath());
    path_v3_ = PackExtensionWithOptions(
        test_dir.AppendASCII("v3"),
        scoped_temp_dir_.GetPath().AppendASCII("permissions3.crx"), pem_path,
        base::FilePath());
  }

  // Returns the ExtensionDisabledGlobalError, if present.
  // Caution: currently only supports one error at a time.
  GlobalError* GetExtensionDisabledGlobalError() {
    return GlobalErrorServiceFactory::GetForProfile(profile())->
        GetGlobalErrorByMenuItemCommandID(IDC_EXTENSION_INSTALL_ERROR_FIRST);
  }

  // Install the initial version, which should happen just fine.
  const Extension* InstallIncreasingPermissionExtensionV1() {
    size_t size_before = extension_registry()->enabled_extensions().size();
    const Extension* extension = InstallExtension(path_v1_, 1);
    if (!extension)
      return NULL;
    if (extension_registry()->enabled_extensions().size() != size_before + 1)
      return NULL;
    return extension;
  }

  // Upgrade to a version that wants more permissions. We should disable the
  // extension and prompt the user to reenable.
  const Extension* UpdateIncreasingPermissionExtension(
      const Extension* extension,
      const base::FilePath& crx_path,
      int expected_change) {
    size_t size_before = extension_registry()->enabled_extensions().size();
    if (UpdateExtension(extension->id(), crx_path, expected_change))
      return NULL;
    content::RunAllTasksUntilIdle();
    EXPECT_EQ(size_before + expected_change,
              extension_registry()->enabled_extensions().size());
    if (extension_registry()->disabled_extensions().size() != 1u)
      return NULL;

    return extension_registry()->disabled_extensions().begin()->get();
  }

  // Helper function to install an extension and upgrade it to a version
  // requiring additional permissions. Returns the new disabled Extension.
  const Extension* InstallAndUpdateIncreasingPermissionsExtension() {
    const Extension* extension = InstallIncreasingPermissionExtensionV1();
    extension = UpdateIncreasingPermissionExtension(extension, path_v2_, -1);
    return extension;
  }

  base::ScopedTempDir scoped_temp_dir_;
  base::FilePath path_v1_;
  base::FilePath path_v2_;
  base::FilePath path_v3_;
};

// Tests the process of updating an extension to one that requires higher
// permissions, and accepting the permissions.
IN_PROC_BROWSER_TEST_F(ExtensionDisabledGlobalErrorTest, AcceptPermissions) {
  const Extension* extension = InstallAndUpdateIncreasingPermissionsExtension();
  ASSERT_TRUE(extension);
  ASSERT_TRUE(GetExtensionDisabledGlobalError());
  const size_t size_before = extension_registry()->enabled_extensions().size();

  ExtensionTestMessageListener listener("v2.onInstalled", false);
  listener.set_failure_message("FAILED");
  extension_service()->GrantPermissionsAndEnableExtension(extension);
  EXPECT_EQ(size_before + 1, extension_registry()->enabled_extensions().size());
  EXPECT_EQ(0u, extension_registry()->disabled_extensions().size());
  ASSERT_FALSE(GetExtensionDisabledGlobalError());
  // Expect onInstalled event to fire.
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

// Tests uninstalling an extension that was disabled due to higher permissions.
IN_PROC_BROWSER_TEST_F(ExtensionDisabledGlobalErrorTest, Uninstall) {
  const Extension* extension = InstallAndUpdateIncreasingPermissionsExtension();
  ASSERT_TRUE(extension);
  ASSERT_TRUE(GetExtensionDisabledGlobalError());
  const size_t size_before = extension_registry()->enabled_extensions().size();

  UninstallExtension(extension->id());
  EXPECT_EQ(size_before, extension_registry()->enabled_extensions().size());
  EXPECT_EQ(0u, extension_registry()->disabled_extensions().size());
  ASSERT_FALSE(GetExtensionDisabledGlobalError());
}

// Tests uninstalling a disabled extension with an uninstall dialog.
IN_PROC_BROWSER_TEST_F(ExtensionDisabledGlobalErrorTest, UninstallFromDialog) {
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  const Extension* extension = InstallAndUpdateIncreasingPermissionsExtension();
  ASSERT_TRUE(extension);
  std::string extension_id = extension->id();
  GlobalErrorWithStandardBubble* error =
      static_cast<GlobalErrorWithStandardBubble*>(
          GetExtensionDisabledGlobalError());
  ASSERT_TRUE(error);

  // The "cancel" button is the uninstall button on the browser.
  extensions::TestExtensionRegistryObserver test_observer(extension_registry(),
                                                          extension_id);
  error->BubbleViewCancelButtonPressed(browser());
  test_observer.WaitForExtensionUninstalled();

  EXPECT_FALSE(extension_registry()->GetExtensionById(
      extension_id, ExtensionRegistry::EVERYTHING));
  EXPECT_FALSE(GetExtensionDisabledGlobalError());
}

IN_PROC_BROWSER_TEST_F(ExtensionDisabledGlobalErrorTest,
                       UninstallWhilePromptBeingShown) {
  const Extension* extension = InstallAndUpdateIncreasingPermissionsExtension();
  ASSERT_TRUE(extension);
  ASSERT_TRUE(GetExtensionDisabledGlobalError());

  // Navigate a tab to the disabled extension, it will show a permission
  // increase dialog.
  GURL url = extension->GetResourceURL("");
  int starting_tab_count = browser()->tab_strip_model()->count();
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  int tab_count = browser()->tab_strip_model()->count();
  EXPECT_EQ(starting_tab_count + 1, tab_count);

  // Uninstall the extension while the dialog is being shown.
  // Although the dialog is modal, a user can still uninstall the extension by
  // other means, e.g. if the user had two browser windows open they can use the
  // second browser window that does not contain the modal dialog, navigate to
  // chrome://extensions and uninstall the extension.
  UninstallExtension(extension->id());
}

// Tests that no error appears if the user disabled the extension.
IN_PROC_BROWSER_TEST_F(ExtensionDisabledGlobalErrorTest, UserDisabled) {
  const Extension* extension = InstallIncreasingPermissionExtensionV1();
  DisableExtension(extension->id());
  extension = UpdateIncreasingPermissionExtension(extension, path_v2_, 0);
  ASSERT_FALSE(GetExtensionDisabledGlobalError());
}

// Test that an error appears if the extension gets disabled because a
// version with higher permissions was installed by sync.
IN_PROC_BROWSER_TEST_F(ExtensionDisabledGlobalErrorTest,
                       HigherPermissionsFromSync) {
  // Get sync data for extension v2 (disabled).
  const Extension* extension = InstallAndUpdateIncreasingPermissionsExtension();
  std::string extension_id = extension->id();
  ExtensionSyncService* sync_service = ExtensionSyncService::Get(profile());
  extensions::ExtensionSyncData sync_data =
      sync_service->CreateSyncData(*extension);
  UninstallExtension(extension_id);
  extension = NULL;

  // Install extension v1.
  InstallIncreasingPermissionExtensionV1();

  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        std::string path = params->url_request.url.path();
        if (path == "/autoupdate/updates.xml") {
          content::URLLoaderInterceptor::WriteResponse(
              test_data_dir_.AppendASCII("permissions_increase")
                  .AppendASCII("updates.xml"),
              params->client.get());
          return true;
        } else if (path == "/autoupdate/v2.crx") {
          content::URLLoaderInterceptor::WriteResponse(
              scoped_temp_dir_.GetPath().AppendASCII("permissions2.crx"),
              params->client.get());
          return true;
        }
        return false;
      }));

  sync_service->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      base::WrapUnique(new syncer::FakeSyncChangeProcessor()),
      base::WrapUnique(new syncer::SyncErrorFactoryMock()));
  extensions::TestExtensionRegistryObserver install_observer(
      extension_registry());
  sync_service->ProcessSyncChanges(
      FROM_HERE,
      syncer::SyncChangeList(
          1, sync_data.GetSyncChange(syncer::SyncChange::ACTION_ADD)));

  install_observer.WaitForExtensionWillBeInstalled();
  content::RunAllTasksUntilIdle();

  extension = extension_registry()->disabled_extensions().GetByID(extension_id);
  ASSERT_TRUE(extension);
  EXPECT_EQ("2", extension->VersionString());
  EXPECT_EQ(1u, extension_registry()->disabled_extensions().size());
  EXPECT_EQ(extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE,
            ExtensionPrefs::Get(extension_service()->profile())
                ->GetDisableReasons(extension_id));
  EXPECT_TRUE(GetExtensionDisabledGlobalError());
}

// Test that an error appears if an extension gets installed server side.
IN_PROC_BROWSER_TEST_F(ExtensionDisabledGlobalErrorTest, RemoteInstall) {
  static const char extension_id[] = "pgdpcfcocojkjfbgpiianjngphoopgmo";

  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        std::string path = params->url_request.url.path();
        if (path == "/autoupdate/updates.xml") {
          content::URLLoaderInterceptor::WriteResponse(
              test_data_dir_.AppendASCII("permissions_increase")
                  .AppendASCII("updates.xml"),
              params->client.get());
          return true;
        } else if (path == "/autoupdate/v2.crx") {
          content::URLLoaderInterceptor::WriteResponse(
              scoped_temp_dir_.GetPath().AppendASCII("permissions2.crx"),
              params->client.get());
          return true;
        }
        return false;
      }));

  sync_pb::EntitySpecifics specifics;
  specifics.mutable_extension()->set_id(extension_id);
  specifics.mutable_extension()->set_enabled(false);
  specifics.mutable_extension()->set_remote_install(true);
  specifics.mutable_extension()->set_disable_reasons(
      extensions::disable_reason::DISABLE_REMOTE_INSTALL);
  specifics.mutable_extension()->set_update_url(
      "http://localhost/autoupdate/updates.xml");
  specifics.mutable_extension()->set_version("2");
  syncer::SyncData sync_data =
      syncer::SyncData::CreateRemoteData(1234567, specifics);

  ExtensionSyncService* sync_service = ExtensionSyncService::Get(profile());
  sync_service->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      base::WrapUnique(new syncer::FakeSyncChangeProcessor()),
      base::WrapUnique(new syncer::SyncErrorFactoryMock()));
  extensions::TestExtensionRegistryObserver install_observer(
      extension_registry());
  sync_service->ProcessSyncChanges(
      FROM_HERE,
      syncer::SyncChangeList(
          1, syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_ADD,
                                sync_data)));

  install_observer.WaitForExtensionWillBeInstalled();
  content::RunAllTasksUntilIdle();

  const Extension* extension =
      extension_registry()->disabled_extensions().GetByID(extension_id);
  ASSERT_TRUE(extension);
  EXPECT_EQ("2", extension->VersionString());
  EXPECT_EQ(1u, extension_registry()->disabled_extensions().size());
  EXPECT_EQ(extensions::disable_reason::DISABLE_REMOTE_INSTALL,
            ExtensionPrefs::Get(extension_service()->profile())
                ->GetDisableReasons(extension_id));
  EXPECT_TRUE(GetExtensionDisabledGlobalError());
}
