// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_sync_data.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_app_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_handlers/app_theme_color_info.h"
#include "chrome/common/web_application_info.h"
#include "components/sync/model/string_ordinal.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"

using apps_helper::AllProfilesHaveSameApps;
using apps_helper::CopyNTPOrdinals;
using apps_helper::DisableApp;
using apps_helper::EnableApp;
using apps_helper::FixNTPOrdinalCollisions;
using apps_helper::GetAppLaunchOrdinalForApp;
using apps_helper::IncognitoDisableApp;
using apps_helper::IncognitoEnableApp;
using apps_helper::InstallApp;
using apps_helper::InstallPlatformApp;
using apps_helper::SetAppLaunchOrdinalForApp;
using apps_helper::SetPageOrdinalForApp;
using apps_helper::UninstallApp;

namespace {

extensions::ExtensionRegistry* GetExtensionRegistry(Profile* profile) {
  return extensions::ExtensionRegistry::Get(profile);
}

}  // namespace

class TwoClientExtensionAppsSyncTest : public SyncTest {
 public:
  TwoClientExtensionAppsSyncTest() : SyncTest(TWO_CLIENT) { DisableVerifier(); }

  ~TwoClientExtensionAppsSyncTest() override {}

  // Needed for AwaitQuiescence().
  bool TestUsesSelfNotifications() override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientExtensionAppsSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientExtensionAppsSyncTest,
                       E2E_ENABLED(StartWithNoApps)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AppsMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientExtensionAppsSyncTest,
                       E2E_ENABLED(StartWithSameApps)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupClients());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i) {
    InstallApp(GetProfile(0), i);
    InstallApp(GetProfile(1), i);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AppsMatchChecker().Wait());
}

// Install some apps on both clients, some on only one client, some on only the
// other, and sync.  Both clients should end up with all apps, and the app and
// page ordinals should be identical.
IN_PROC_BROWSER_TEST_F(TwoClientExtensionAppsSyncTest, StartWithDifferentApps) {
  ASSERT_TRUE(SetupClients());

  int i = 0;

  const int kNumCommonApps = 5;
  for (int j = 0; j < kNumCommonApps; ++i, ++j) {
    InstallApp(GetProfile(0), i);
    InstallApp(GetProfile(1), i);
  }

  const int kNumProfile0Apps = 10;
  for (int j = 0; j < kNumProfile0Apps; ++i, ++j) {
    InstallApp(GetProfile(0), i);
  }

  const int kNumProfile1Apps = 10;
  for (int j = 0; j < kNumProfile1Apps; ++i, ++j) {
    InstallApp(GetProfile(1), i);
  }

  const int kNumPlatformApps = 5;
  for (int j = 0; j < kNumPlatformApps; ++i, ++j) {
    InstallPlatformApp(GetProfile(1), i);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AppsMatchChecker().Wait());
}

// Install some apps on both clients, then sync.  Then install some apps on only
// one client, some on only the other, and then sync again.  Both clients should
// end up with all apps, and the app and page ordinals should be identical.
IN_PROC_BROWSER_TEST_F(TwoClientExtensionAppsSyncTest,
                       E2E_ENABLED(InstallDifferentApps)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupClients());

  int i = 0;

  const int kNumCommonApps = 5;
  for (int j = 0; j < kNumCommonApps; ++i, ++j) {
    InstallApp(GetProfile(0), i);
    InstallApp(GetProfile(1), i);
  }

  ASSERT_TRUE(SetupSync());

  const int kNumProfile0Apps = 10;
  for (int j = 0; j < kNumProfile0Apps; ++i, ++j) {
    InstallApp(GetProfile(0), i);
  }

  const int kNumProfile1Apps = 10;
  for (int j = 0; j < kNumProfile1Apps; ++i, ++j) {
    InstallApp(GetProfile(1), i);
  }

  ASSERT_TRUE(AppsMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientExtensionAppsSyncTest, E2E_ENABLED(Add)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AppsMatchChecker().Wait());

  InstallApp(GetProfile(0), 0);

  ASSERT_TRUE(AppsMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientExtensionAppsSyncTest, E2E_ENABLED(Uninstall)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AppsMatchChecker().Wait());

  InstallApp(GetProfile(0), 0);
  ASSERT_TRUE(AppsMatchChecker().Wait());

  UninstallApp(GetProfile(0), 0);
  ASSERT_TRUE(AppsMatchChecker().Wait());
}

// Install an app on one client, then sync. Then uninstall the app on the first
// client and sync again. Now install a new app on the first client and sync.
// Both client should only have the second app, with identical app and page
// ordinals.
IN_PROC_BROWSER_TEST_F(TwoClientExtensionAppsSyncTest,
                       E2E_ENABLED(UninstallThenInstall)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AppsMatchChecker().Wait());

  InstallApp(GetProfile(0), 0);
  ASSERT_TRUE(AppsMatchChecker().Wait());

  UninstallApp(GetProfile(0), 0);
  ASSERT_TRUE(AppsMatchChecker().Wait());

  InstallApp(GetProfile(0), 1);
  ASSERT_TRUE(AppsMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientExtensionAppsSyncTest, E2E_ENABLED(Merge)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AppsMatchChecker().Wait());

  InstallApp(GetProfile(0), 0);
  ASSERT_TRUE(AppsMatchChecker().Wait());

  UninstallApp(GetProfile(0), 0);

  InstallApp(GetProfile(0), 1);
  InstallApp(GetProfile(0), 2);

  InstallApp(GetProfile(1), 2);
  InstallApp(GetProfile(1), 3);

  ASSERT_TRUE(AppsMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientExtensionAppsSyncTest,
                       E2E_ENABLED(UpdateEnableDisableApp)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AppsMatchChecker().Wait());

  InstallApp(GetProfile(0), 0);
  ASSERT_TRUE(AppsMatchChecker().Wait());

  DisableApp(GetProfile(0), 0);
  ASSERT_TRUE(AppsMatchChecker().Wait());

  EnableApp(GetProfile(1), 0);
  ASSERT_TRUE(AppsMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientExtensionAppsSyncTest,
                       E2E_ENABLED(UpdateIncognitoEnableDisable)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AppsMatchChecker().Wait());

  InstallApp(GetProfile(0), 0);
  ASSERT_TRUE(AppsMatchChecker().Wait());

  IncognitoEnableApp(GetProfile(0), 0);
  ASSERT_TRUE(AppsMatchChecker().Wait());

  IncognitoDisableApp(GetProfile(1), 0);
  ASSERT_TRUE(AppsMatchChecker().Wait());
}

// Install the same app on both clients, then sync. Change the page ordinal on
// one client and sync. Both clients should have the updated page ordinal for
// the app.
IN_PROC_BROWSER_TEST_F(TwoClientExtensionAppsSyncTest,
                       E2E_ENABLED(UpdatePageOrdinal)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AppsMatchChecker().Wait());

  syncer::StringOrdinal initial_page =
      syncer::StringOrdinal::CreateInitialOrdinal();
  InstallApp(GetProfile(0), 0);
  ASSERT_TRUE(AppsMatchChecker().Wait());

  syncer::StringOrdinal second_page = initial_page.CreateAfter();
  SetPageOrdinalForApp(GetProfile(0), 0, second_page);
  ASSERT_TRUE(AppsMatchChecker().Wait());
}

// Install the same app on both clients, then sync. Change the app launch
// ordinal on one client and sync. Both clients should have the updated app
// launch ordinal for the app.
IN_PROC_BROWSER_TEST_F(TwoClientExtensionAppsSyncTest,
                       E2E_ENABLED(UpdateAppLaunchOrdinal)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AppsMatchChecker().Wait());

  InstallApp(GetProfile(0), 0);
  ASSERT_TRUE(AppsMatchChecker().Wait());

  syncer::StringOrdinal initial_position =
      GetAppLaunchOrdinalForApp(GetProfile(0), 0);

  syncer::StringOrdinal second_position = initial_position.CreateAfter();
  SetAppLaunchOrdinalForApp(GetProfile(0), 0, second_position);
  ASSERT_TRUE(AppsMatchChecker().Wait());
}

// Adjust the CWS location within a page on the first client and sync. Adjust
// which page the CWS appears on and sync. Both clients should have the same
// page and app launch ordinal values for the CWS.
IN_PROC_BROWSER_TEST_F(TwoClientExtensionAppsSyncTest,
                       E2E_ENABLED(UpdateCWSOrdinals)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AppsMatchChecker().Wait());

  // Change the app launch ordinal.
  syncer::StringOrdinal cws_app_launch_ordinal =
      extensions::ExtensionSystem::Get(GetProfile(0))
          ->app_sorting()
          ->GetAppLaunchOrdinal(extensions::kWebStoreAppId);
  extensions::ExtensionSystem::Get(GetProfile(0))
      ->app_sorting()
      ->SetAppLaunchOrdinal(extensions::kWebStoreAppId,
                            cws_app_launch_ordinal.CreateAfter());
  ASSERT_TRUE(AppsMatchChecker().Wait());

  // Change the page ordinal.
  syncer::StringOrdinal cws_page_ordinal =
      extensions::ExtensionSystem::Get(GetProfile(1))
          ->app_sorting()
          ->GetPageOrdinal(extensions::kWebStoreAppId);
  extensions::ExtensionSystem::Get(GetProfile(1))
      ->app_sorting()
      ->SetPageOrdinal(extensions::kWebStoreAppId,
                       cws_page_ordinal.CreateAfter());
  ASSERT_TRUE(AppsMatchChecker().Wait());
}

// Adjust the launch type on the first client and sync. Both clients should
// have the same launch type values for the CWS.
IN_PROC_BROWSER_TEST_F(TwoClientExtensionAppsSyncTest,
                       E2E_ENABLED(UpdateLaunchType)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync());
  // Wait until sync settles before we override the apps below.
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AppsMatchChecker().Wait());

  // Change the launch type to window.
  extensions::SetLaunchType(GetProfile(1), extensions::kWebStoreAppId,
                            extensions::LAUNCH_TYPE_WINDOW);
  ASSERT_TRUE(AppsMatchChecker().Wait());
  ASSERT_EQ(extensions::GetLaunchTypePrefValue(
                extensions::ExtensionPrefs::Get(GetProfile(0)),
                extensions::kWebStoreAppId),
            extensions::LAUNCH_TYPE_WINDOW);

  // Change the launch type to regular tab.
  extensions::SetLaunchType(GetProfile(1), extensions::kWebStoreAppId,
                            extensions::LAUNCH_TYPE_REGULAR);
  ASSERT_TRUE(AppsMatchChecker().Wait());

  ASSERT_EQ(extensions::GetLaunchTypePrefValue(
                extensions::ExtensionPrefs::Get(GetProfile(0)),
                extensions::kWebStoreAppId),
            extensions::LAUNCH_TYPE_REGULAR);
}

IN_PROC_BROWSER_TEST_F(TwoClientExtensionAppsSyncTest, UnexpectedLaunchType) {
  ASSERT_TRUE(SetupSync());
  // Wait until sync settles before we override the apps below.
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameApps());

  extensions::SetLaunchType(GetProfile(1), extensions::kWebStoreAppId,
                            extensions::LAUNCH_TYPE_REGULAR);
  ASSERT_TRUE(AppsMatchChecker().Wait());

  const extensions::Extension* extension =
      GetExtensionRegistry(GetProfile(1))
          ->GetExtensionById(extensions::kWebStoreAppId,
                             extensions::ExtensionRegistry::EVERYTHING);
  ASSERT_TRUE(extension);

  ExtensionSyncService* extension_sync_service =
      ExtensionSyncService::Get(GetProfile(1));

  extensions::ExtensionSyncData original_data(
      extension_sync_service->CreateSyncData(*extension));

  // Create an invalid launch type and ensure it doesn't get down-synced. This
  // simulates the case of a future launch type being added which old versions
  // don't yet understand.
  extensions::ExtensionSyncData invalid_launch_type_data(
      *extension, original_data.enabled(), original_data.disable_reasons(),
      original_data.incognito_enabled(), original_data.remote_install(),
      original_data.app_launch_ordinal(), original_data.page_ordinal(),
      extensions::NUM_LAUNCH_TYPES);
  extension_sync_service->ApplySyncData(invalid_launch_type_data);

  // The launch type should remain the same.
  ASSERT_TRUE(AppsMatchChecker().Wait());
}

// TODO(akalin): Add tests exercising:
//   - Offline installation/uninstallation behavior
//   - App-specific properties
