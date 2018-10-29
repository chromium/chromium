// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sync_data.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/feature_toggler.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_app_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_handlers/app_theme_color_info.h"
#include "components/sync/driver/sync_driver_switches.h"
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

extensions::ExtensionService* GetExtensionService(Profile* profile) {
  return extensions::ExtensionSystem::Get(profile)->extension_service();
}

}  // namespace

class TwoClientAppsSyncTest : public FeatureToggler, public SyncTest {
 public:
  TwoClientAppsSyncTest()
      : FeatureToggler(switches::kSyncPseudoUSSApps), SyncTest(TWO_CLIENT) {
    DisableVerifier();
  }

  ~TwoClientAppsSyncTest() override {}

  bool TestUsesSelfNotifications() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientAppsSyncTest);
};

IN_PROC_BROWSER_TEST_P(TwoClientAppsSyncTest, E2E_ENABLED(StartWithNoApps)) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AppsMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientAppsSyncTest, E2E_ENABLED(StartWithSameApps)) {
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
IN_PROC_BROWSER_TEST_P(TwoClientAppsSyncTest, StartWithDifferentApps) {
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
IN_PROC_BROWSER_TEST_P(TwoClientAppsSyncTest,
                       E2E_ENABLED(InstallDifferentApps)) {
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

IN_PROC_BROWSER_TEST_P(TwoClientAppsSyncTest, E2E_ENABLED(Add)) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AppsMatchChecker().Wait());

  InstallApp(GetProfile(0), 0);

  ASSERT_TRUE(AppsMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientAppsSyncTest, E2E_ENABLED(Uninstall)) {
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
IN_PROC_BROWSER_TEST_P(TwoClientAppsSyncTest,
                       E2E_ENABLED(UninstallThenInstall)) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AppsMatchChecker().Wait());

  InstallApp(GetProfile(0), 0);
  ASSERT_TRUE(AppsMatchChecker().Wait());

  UninstallApp(GetProfile(0), 0);
  ASSERT_TRUE(AppsMatchChecker().Wait());

  InstallApp(GetProfile(0), 1);
  ASSERT_TRUE(AppsMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientAppsSyncTest, E2E_ENABLED(Merge)) {
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

IN_PROC_BROWSER_TEST_P(TwoClientAppsSyncTest,
                       E2E_ENABLED(UpdateEnableDisableApp)) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AppsMatchChecker().Wait());

  InstallApp(GetProfile(0), 0);
  ASSERT_TRUE(AppsMatchChecker().Wait());

  DisableApp(GetProfile(0), 0);
  ASSERT_TRUE(AppsMatchChecker().Wait());

  EnableApp(GetProfile(1), 0);
  ASSERT_TRUE(AppsMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientAppsSyncTest,
                       E2E_ENABLED(UpdateIncognitoEnableDisable)) {
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
IN_PROC_BROWSER_TEST_P(TwoClientAppsSyncTest, E2E_ENABLED(UpdatePageOrdinal)) {
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
IN_PROC_BROWSER_TEST_P(TwoClientAppsSyncTest,
                       E2E_ENABLED(UpdateAppLaunchOrdinal)) {
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
IN_PROC_BROWSER_TEST_P(TwoClientAppsSyncTest, E2E_ENABLED(UpdateCWSOrdinals)) {
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
IN_PROC_BROWSER_TEST_P(TwoClientAppsSyncTest, E2E_ENABLED(UpdateLaunchType)) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AppsMatchChecker().Wait());

  // Change the launch type to window.
  extensions::SetLaunchType(GetProfile(1), extensions::kWebStoreAppId,
                            extensions::LAUNCH_TYPE_WINDOW);
  ASSERT_TRUE(AppsMatchChecker().Wait());
  ASSERT_EQ(
      extensions::GetLaunchTypePrefValue(
          extensions::ExtensionPrefs::Get(GetProfile(0)),
          extensions::kWebStoreAppId),
      extensions::LAUNCH_TYPE_WINDOW);

  // Change the launch type to regular tab.
  extensions::SetLaunchType(GetProfile(1), extensions::kWebStoreAppId,
                            extensions::LAUNCH_TYPE_REGULAR);
  ASSERT_TRUE(AppsMatchChecker().Wait());

  ASSERT_EQ(
      extensions::GetLaunchTypePrefValue(
          extensions::ExtensionPrefs::Get(GetProfile(0)),
          extensions::kWebStoreAppId),
      extensions::LAUNCH_TYPE_REGULAR);
}

IN_PROC_BROWSER_TEST_P(TwoClientAppsSyncTest, UnexpectedLaunchType) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameApps());

  extensions::SetLaunchType(GetProfile(1), extensions::kWebStoreAppId,
                            extensions::LAUNCH_TYPE_REGULAR);
  ASSERT_TRUE(AppsMatchChecker().Wait());

  const extensions::Extension* extension =
      GetExtensionRegistry(GetProfile(1))->GetExtensionById(
          extensions::kWebStoreAppId,
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
      *extension,
      original_data.enabled(),
      original_data.disable_reasons(),
      original_data.incognito_enabled(),
      original_data.remote_install(),
      original_data.installed_by_custodian(),
      original_data.app_launch_ordinal(),
      original_data.page_ordinal(),
      extensions::NUM_LAUNCH_TYPES);
  extension_sync_service->ApplySyncData(invalid_launch_type_data);

  // The launch type should remain the same.
  ASSERT_TRUE(AppsMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientAppsSyncTest, BookmarkAppBasic) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameApps());

  size_t num_extensions =
      GetExtensionRegistry(GetProfile(0))->enabled_extensions().size();

  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL("http://www.chromium.org/path");
  web_app_info.scope = GURL("http://www.chromium.org/");
  web_app_info.title = base::UTF8ToUTF16("Test name");
  web_app_info.description = base::UTF8ToUTF16("Test description");
  ++num_extensions;
  {
    content::WindowedNotificationObserver windowed_observer(
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        content::NotificationService::AllSources());
    extensions::CreateOrUpdateBookmarkApp(GetExtensionService(GetProfile(0)),
                                          &web_app_info,
                                          true /* is_locally_installed */);
    windowed_observer.Wait();
    EXPECT_EQ(num_extensions,
              GetExtensionRegistry(GetProfile(0))->enabled_extensions().size());
  }
  {
    // Wait for the synced app to install.
    content::WindowedNotificationObserver windowed_observer(
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        base::BindRepeating(&AllProfilesHaveSameApps));
    windowed_observer.Wait();
  }
}

IN_PROC_BROWSER_TEST_P(TwoClientAppsSyncTest, BookmarkAppMinimal) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameApps());

  size_t num_extensions =
      GetExtensionRegistry(GetProfile(0))->enabled_extensions().size();

  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL("http://www.chromium.org/");
  web_app_info.title = base::UTF8ToUTF16("Test name");
  ++num_extensions;
  {
    content::WindowedNotificationObserver windowed_observer(
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        content::NotificationService::AllSources());
    extensions::CreateOrUpdateBookmarkApp(GetExtensionService(GetProfile(0)),
                                          &web_app_info,
                                          true /* is_locally_installed */);
    windowed_observer.Wait();
    EXPECT_EQ(num_extensions,
              GetExtensionRegistry(GetProfile(0))->enabled_extensions().size());
  }
  {
    // Wait for the synced app to install.
    content::WindowedNotificationObserver windowed_observer(
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        base::BindRepeating(&AllProfilesHaveSameApps));
    windowed_observer.Wait();
  }
}

const extensions::Extension* GetAppByLaunchURL(const GURL& url,
                                               Profile* profile) {
  for (auto extension_it :
       GetExtensionRegistry(profile)->enabled_extensions()) {
    if (extensions::AppLaunchInfo::GetLaunchWebURL(extension_it.get()) == url)
      return extension_it.get();
  }

  return nullptr;
}

IN_PROC_BROWSER_TEST_P(TwoClientAppsSyncTest, BookmarkAppThemeColor) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameApps());

  size_t num_extensions =
      GetExtensionRegistry(GetProfile(0))->enabled_extensions().size();

  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL("http://www.chromium.org/");
  web_app_info.title = base::UTF8ToUTF16("Test name");
  web_app_info.theme_color = SK_ColorBLUE;
  ++num_extensions;
  {
    content::WindowedNotificationObserver windowed_observer(
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        content::NotificationService::AllSources());
    extensions::CreateOrUpdateBookmarkApp(GetExtensionService(GetProfile(0)),
                                          &web_app_info,
                                          true /* is_locally_installed */);
    windowed_observer.Wait();
    EXPECT_EQ(num_extensions,
              GetExtensionRegistry(GetProfile(0))->enabled_extensions().size());
  }
  {
    // Wait for the synced app to install.
    content::WindowedNotificationObserver windowed_observer(
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        base::BindRepeating(&AllProfilesHaveSameApps));
    windowed_observer.Wait();
  }
  auto* extension = GetAppByLaunchURL(web_app_info.app_url, GetProfile(1));
  base::Optional<SkColor> theme_color =
      extensions::AppThemeColorInfo::GetThemeColor(extension);
  EXPECT_EQ(SK_ColorBLUE, theme_color.value());
}

IN_PROC_BROWSER_TEST_P(TwoClientAppsSyncTest, IsLocallyInstalled) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameApps());

  size_t num_extensions =
      GetExtensionRegistry(GetProfile(0))->enabled_extensions().size();

  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL("http://www.chromium.org/");
  web_app_info.title = base::UTF8ToUTF16("Test name");
  web_app_info.theme_color = SK_ColorBLUE;
  ++num_extensions;
  {
    content::WindowedNotificationObserver windowed_observer(
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        content::NotificationService::AllSources());
    extensions::CreateOrUpdateBookmarkApp(GetExtensionService(GetProfile(0)),
                                          &web_app_info,
                                          true /* is_locally_installed */);
    windowed_observer.Wait();
    EXPECT_EQ(num_extensions,
              GetExtensionRegistry(GetProfile(0))->enabled_extensions().size());
  }
  {
    // Wait for the synced app to install.
    content::WindowedNotificationObserver windowed_observer(
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        base::BindRepeating(&AllProfilesHaveSameApps));
    windowed_observer.Wait();

    // The is_locally_installed pref is set in a post install task which
    // completes asynchronously after the CRX_INSTALLER_DONE notification is
    // sent. This test needs to wait for this to complete before checking the
    // is_locally_installed_pref, so it waits until all tasks are complete.
    //
    // Other tests do not need to do this, as all the fields they check are set
    // before the CRX_INSTALLER_DONE notification is sent.
    //
    // Note this cannot replace the CRX_INSTALLER_DONE notification observer as
    // it would not wait for the sync stuff to happen.
    content::RunAllTasksUntilIdle();
  }
  auto* extension = GetAppByLaunchURL(web_app_info.app_url, GetProfile(1));
#if defined(OS_CHROMEOS)
  EXPECT_TRUE(BookmarkAppIsLocallyInstalled(GetProfile(1), extension));
#else
  EXPECT_FALSE(BookmarkAppIsLocallyInstalled(GetProfile(1), extension));
#endif
}
// TODO(akalin): Add tests exercising:
//   - Offline installation/uninstallation behavior
//   - App-specific properties

INSTANTIATE_TEST_CASE_P(USS,
                        TwoClientAppsSyncTest,
                        ::testing::Values(false, true));
