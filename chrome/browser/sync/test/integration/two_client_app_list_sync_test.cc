// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/one_shot_event.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/extensions_helper.h"
#include "chrome/browser/sync/test/integration/sync_app_list_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_user_settings.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"

using apps_helper::DisableApp;
using apps_helper::EnableApp;
using apps_helper::IncognitoDisableApp;
using apps_helper::IncognitoEnableApp;
using apps_helper::InstallAppsPendingForSync;
using apps_helper::InstallHostedApp;
using apps_helper::IsAppEnabled;
using apps_helper::IsIncognitoEnabled;
using apps_helper::UninstallApp;
using syncer::SyncUserSettings;
using syncer::UserSelectableOsType;
using syncer::UserSelectableOsTypeSet;
using syncer::UserSelectableType;
using syncer::UserSelectableTypeSet;

namespace {

const size_t kNumDefaultApps = 2;

bool AllProfilesHaveSameAppList(size_t* size_out = nullptr) {
  return SyncAppListHelper::GetInstance()->AllProfilesHaveSameAppList(size_out);
}

const app_list::AppListSyncableService::SyncItem* GetSyncItem(
    Profile* profile,
    const std::string& app_id) {
  app_list::AppListSyncableService* service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile);
  return service->GetSyncItem(app_id);
}

}  // namespace

class TwoClientAppListSyncTest : public SyncTest {
 public:
  TwoClientAppListSyncTest() : SyncTest(TWO_CLIENT) {}
  ~TwoClientAppListSyncTest() override = default;

  // SyncTest
  bool SetupClients() override {
    if (!SyncTest::SetupClients()) {
      return false;
    }

    // Init SyncAppListHelper to ensure that the extension system is initialized
    // for each Profile.
    SyncAppListHelper::GetInstance();
    return true;
  }

  void AwaitQuiescenceAndInstallAppsPendingForSync() {
    ASSERT_TRUE(AwaitQuiescence());
    InstallAppsPendingForSync(GetProfile(0));
    InstallAppsPendingForSync(GetProfile(1));
  }

  void WaitForExtensionServicesToLoad() {
    for (int i = 0; i < num_clients(); ++i) {
      WaitForExtensionsServiceToLoadForProfile(GetProfile(i));
    }
  }

 private:
  void WaitForExtensionsServiceToLoadForProfile(Profile* profile) {
    extensions::ExtensionSystem* extension_system =
        extensions::ExtensionSystem::Get(profile);
    base::RunLoop run_loop;
    extension_system->ready().Post(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }
};

class RemoveDefaultAppSyncTest : public testing::WithParamInterface<bool>,
                                 public TwoClientAppListSyncTest {
 public:
  RemoveDefaultAppSyncTest() = default;

  RemoveDefaultAppSyncTest(const RemoveDefaultAppSyncTest&) = delete;
  RemoveDefaultAppSyncTest& operator=(const RemoveDefaultAppSyncTest&) = delete;

  ~RemoveDefaultAppSyncTest() override = default;

  bool MarkAppAsDefaultApp() { return GetParam(); }
};

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, StartWithNoApps) {
  ASSERT_TRUE(SetupSync());
  WaitForExtensionServicesToLoad();

  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, StartWithSameApps) {
  ASSERT_TRUE(SetupClients());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i) {
    InstallHostedApp(GetProfile(0), i);
    InstallHostedApp(GetProfile(1), i);
  }

  ASSERT_TRUE(SetupSync());
  WaitForExtensionServicesToLoad();

  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

// Install some apps on both clients, some on only one client, some on only the
// other, and sync.  Both clients should end up with all apps, and the app and
// page ordinals should be identical.
IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, StartWithDifferentApps) {
  ASSERT_TRUE(SetupClients());

  int i = 0;

  const int kNumCommonApps = 5;
  for (int j = 0; j < kNumCommonApps; ++i, ++j) {
    InstallHostedApp(GetProfile(0), i);
    InstallHostedApp(GetProfile(1), i);
  }

  const int kNumProfile0Apps = 10;
  for (int j = 0; j < kNumProfile0Apps; ++i, ++j) {
    std::string id = InstallHostedApp(GetProfile(0), i);
  }

  const int kNumProfile1Apps = 10;
  for (int j = 0; j < kNumProfile1Apps; ++i, ++j) {
    std::string id = InstallHostedApp(GetProfile(1), i);
  }

  ASSERT_TRUE(SetupSync());
  WaitForExtensionServicesToLoad();

  AwaitQuiescenceAndInstallAppsPendingForSync();

  // Verify the app lists, but ignore absolute position values, checking only
  // relative positions (see note in app_list_syncable_service.h).
  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

// Install some apps on both clients, then sync.  Then install some apps on only
// one client, some on only the other, and then sync again.  Both clients should
// end up with all apps, and the app and page ordinals should be identical.
IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, InstallDifferentApps) {
  ASSERT_TRUE(SetupClients());

  int i = 0;

  const int kNumCommonApps = 5;
  for (int j = 0; j < kNumCommonApps; ++i, ++j) {
    InstallHostedApp(GetProfile(0), i);
    InstallHostedApp(GetProfile(1), i);
  }

  ASSERT_TRUE(SetupSync());
  WaitForExtensionServicesToLoad();

  ASSERT_TRUE(AwaitQuiescence());

  const int kNumProfile0Apps = 10;
  for (int j = 0; j < kNumProfile0Apps; ++i, ++j) {
    std::string id = InstallHostedApp(GetProfile(0), i);
  }

  const int kNumProfile1Apps = 10;
  for (int j = 0; j < kNumProfile1Apps; ++i, ++j) {
    std::string id = InstallHostedApp(GetProfile(1), i);
  }

  AwaitQuiescenceAndInstallAppsPendingForSync();

  // Verify the app lists, but ignore absolute position values, checking only
  // relative positions (see note in app_list_syncable_service.h).
  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, Install) {
  ASSERT_TRUE(SetupSync());
  WaitForExtensionServicesToLoad();
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  InstallHostedApp(GetProfile(0), 0);
  AwaitQuiescenceAndInstallAppsPendingForSync();

  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, Uninstall) {
  ASSERT_TRUE(SetupSync());
  WaitForExtensionServicesToLoad();
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  InstallHostedApp(GetProfile(0), 0);
  AwaitQuiescenceAndInstallAppsPendingForSync();

  ASSERT_TRUE(AllProfilesHaveSameAppList());

  UninstallApp(GetProfile(0), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

// Install an app on one client, then sync. Then uninstall the app on the first
// client and sync again. Now install a new app on the first client and sync.
// Both client should only have the second app, with identical app and page
// ordinals.
IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, UninstallThenInstall) {
  ASSERT_TRUE(SetupSync());
  WaitForExtensionServicesToLoad();
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  InstallHostedApp(GetProfile(0), 0);
  AwaitQuiescenceAndInstallAppsPendingForSync();

  ASSERT_TRUE(AllProfilesHaveSameAppList());

  UninstallApp(GetProfile(0), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  InstallHostedApp(GetProfile(0), 1);
  AwaitQuiescenceAndInstallAppsPendingForSync();

  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, Merge) {
  ASSERT_TRUE(SetupSync());
  WaitForExtensionServicesToLoad();
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  InstallHostedApp(GetProfile(0), 0);
  InstallHostedApp(GetProfile(1), 0);
  ASSERT_TRUE(AwaitQuiescence());

  UninstallApp(GetProfile(0), 0);
  InstallHostedApp(GetProfile(0), 1);

  InstallHostedApp(GetProfile(0), 2);
  InstallHostedApp(GetProfile(1), 2);

  InstallHostedApp(GetProfile(1), 3);
  AwaitQuiescenceAndInstallAppsPendingForSync();

  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, UpdateEnableDisableApp) {
  ASSERT_TRUE(SetupSync());
  WaitForExtensionServicesToLoad();
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  InstallHostedApp(GetProfile(0), 0);
  InstallHostedApp(GetProfile(1), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  ASSERT_TRUE(IsAppEnabled(GetProfile(0), 0));
  ASSERT_TRUE(IsAppEnabled(GetProfile(1), 0));

  DisableApp(GetProfile(0), 0);

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());
  ASSERT_FALSE(IsAppEnabled(GetProfile(0), 0));
  ASSERT_FALSE(IsAppEnabled(GetProfile(1), 0));

  EnableApp(GetProfile(1), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());
  ASSERT_TRUE(IsAppEnabled(GetProfile(0), 0));
  ASSERT_TRUE(IsAppEnabled(GetProfile(1), 0));
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, UpdateIncognitoEnableDisable) {
  ASSERT_TRUE(SetupSync());
  WaitForExtensionServicesToLoad();
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  InstallHostedApp(GetProfile(0), 0);
  InstallHostedApp(GetProfile(1), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  ASSERT_FALSE(IsIncognitoEnabled(GetProfile(0), 0));
  ASSERT_FALSE(IsIncognitoEnabled(GetProfile(1), 0));

  IncognitoEnableApp(GetProfile(0), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  ASSERT_TRUE(IsIncognitoEnabled(GetProfile(0), 0));
  ASSERT_TRUE(IsIncognitoEnabled(GetProfile(1), 0));

  IncognitoDisableApp(GetProfile(1), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());
  ASSERT_FALSE(IsIncognitoEnabled(GetProfile(0), 0));
  ASSERT_FALSE(IsIncognitoEnabled(GetProfile(1), 0));
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, DisableApps) {
  ASSERT_TRUE(SetupSync());
  WaitForExtensionServicesToLoad();
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  SyncUserSettings* settings = GetClient(1)->service()->GetUserSettings();
  {
    // Disable APP_LIST by disabling apps sync.
    UserSelectableOsTypeSet types = settings->GetSelectedOsTypes();
    types.Remove(UserSelectableOsType::kOsApps);
    settings->SetSelectedOsTypes(/*sync_all_os_types=*/false, types);
    InstallHostedApp(GetProfile(0), 0);
    ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
    ASSERT_FALSE(AllProfilesHaveSameAppList());
  }

  {
    // Enable APP_LIST by enabling apps sync.
    UserSelectableOsTypeSet types = settings->GetSelectedOsTypes();
    types.Put(UserSelectableOsType::kOsApps);
    settings->SetSelectedOsTypes(/*sync_all_os_types=*/false, types);
    AwaitQuiescenceAndInstallAppsPendingForSync();
  }

  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

// Disable sync for the second client and then install an app on the first
// client, then enable sync on the second client. Both clients should have the
// same app with identical app and page ordinals.
IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, DisableSync) {
  ASSERT_TRUE(SetupSync());
  WaitForExtensionServicesToLoad();
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  ASSERT_TRUE(GetClient(1)->DisableSyncForAllDatatypes());
  InstallHostedApp(GetProfile(0), 0);
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_FALSE(AllProfilesHaveSameAppList());

  ASSERT_TRUE(GetClient(1)->EnableSyncForRegisteredDatatypes());
  AwaitQuiescenceAndInstallAppsPendingForSync();

  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

// Install some apps on both clients, then sync. Move an app on one client
// and sync. Both clients should have the updated position for the app.
IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, Move) {
  ASSERT_TRUE(SetupSync());
  WaitForExtensionServicesToLoad();
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i) {
    InstallHostedApp(GetProfile(1), i);
  }

  AwaitQuiescenceAndInstallAppsPendingForSync();

  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

// Install a Default App on both clients, then sync. Remove the app on one
// client and sync. Ensure that the app is removed on the other client and
// that a TYPE_REMOVE_DEFAULT_APP entry exists.
//
// This test largely checks mechanism (the How), not policy (the What or Why).
// "How" is user-invisible implementation detail, such as whether or not a
// SyncItem exists and what its code is (e.g. TYPE_REMOVE_DEFAULT_APP). "What"
// is user-visible effects, such as whether or not an app is (still) installed.
//
// To be clear, "policy" in this comment means something different than the
// "policy" in "enterprise administrative policy can install apps". Similarly,
// "default apps" means hardware-specific apps from OEMs, such as an "HP
// [Hewlett Packard]" app installed by default on a HP laptop. "Default apps"
// doesn't refer to built-in apps like the Camera or Files apps.
//
// This test is run twice, with MarkAppAsDefaultApp() returning either false or
// true. Either way, the What (what's user visible, i.e. whether or not apps
// are installed or uninstalled after synchronizing) is unaffected by whether
// or not we mark an app as default-installed in Profile1 before we sync the
// two profiles. It's unclear whether this non-difference is deliberate.
//
// This isn't ideal, but probably still better than nothing. Obviously, it
// would be better to confirm some user-visible effect happens for
// default-installed apps and does not happen otherwise, but it's not certain
// what such an effect would be, or at least how to easily test it. There is
// some discussion at https://crrev.com/c/1732092 and
// https://crrev.com/c/1720229/1/chrome/browser/sync/test/integration/two_client_app_list_sync_test.cc#402
// although the desired What is possibly lost to the mists of time.
IN_PROC_BROWSER_TEST_P(RemoveDefaultAppSyncTest, Remove) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(SetupSync());
  WaitForExtensionServicesToLoad();

  // Install a non-default app in two synchronized Profiles. We should end up
  // with a certain number of apps, lets say N.
  InstallHostedApp(GetProfile(0), 0);
  InstallHostedApp(GetProfile(1), 0);
  size_t number_of_apps = 0;
  ASSERT_TRUE(AllProfilesHaveSameAppList(&number_of_apps));
  const size_t initial_number_of_apps = number_of_apps;

  // Install an app in Profile 0 only. At a later point, we'll mark it as
  // default-installed, but for now, it's just a regular app.
  //
  // After sync'ing, we should have N+1 apps in both Profiles.
  const int default_app_index = 1;
  std::string default_app_id =
      InstallHostedApp(GetProfile(0), default_app_index);
  AwaitQuiescenceAndInstallAppsPendingForSync();
  ASSERT_TRUE(AllProfilesHaveSameAppList(&number_of_apps));
  EXPECT_EQ(number_of_apps, initial_number_of_apps + 1);

  // Mark that app as a default app, in Profile 1 only.
  using ALSS = app_list::AppListSyncableService;
  EXPECT_FALSE(ALSS::AppIsDefaultForTest(GetProfile(1), default_app_id));
  if (MarkAppAsDefaultApp()) {
    ALSS::SetAppIsDefaultForTest(GetProfile(1), default_app_id);
    EXPECT_TRUE(ALSS::AppIsDefaultForTest(GetProfile(1), default_app_id));
  }

  // Remove that app in Profile 0. After sync'ing, it should also be removed
  // from Profile 1: we should go back to having N apps in both Profiles.
  UninstallApp(GetProfile(0), default_app_index);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList(&number_of_apps));
  EXPECT_EQ(number_of_apps, initial_number_of_apps);

  // Ensure that a TYPE_REMOVE_DEFAULT_APP SyncItem exists in both Profiles, if
  // it was marked as a default app in Profile 1. This tests the How, not the
  // What, but it's better than nothing.
  for (int i = 0; i < 2; i++) {
    const ALSS::SyncItem* sync_item =
        GetSyncItem(GetProfile(i), default_app_id);
    if (MarkAppAsDefaultApp()) {
      ASSERT_TRUE(sync_item);
      EXPECT_EQ(sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP,
                sync_item->item_type);
    } else {
      ASSERT_FALSE(sync_item);
    }
  }

  // Re-Install the same app in Profile 0.
  std::string app_id2 = InstallHostedApp(GetProfile(0), default_app_index);
  EXPECT_EQ(default_app_id, app_id2);

  // Ensure that the TYPE_REMOVE_DEFAULT_APP SyncItem (if present) was replaced
  // with an TYPE_APP entry, for at least Profile 0.
  {
    const ALSS::SyncItem* sync_item = GetSyncItem(GetProfile(0), app_id2);
    ASSERT_TRUE(sync_item);
    EXPECT_EQ(sync_pb::AppListSpecifics::TYPE_APP, sync_item->item_type);
  }

  // After sync'ing, we should have N+1 apps in both Profiles.
  AwaitQuiescenceAndInstallAppsPendingForSync();
  ASSERT_TRUE(AllProfilesHaveSameAppList(&number_of_apps));
  EXPECT_EQ(number_of_apps, initial_number_of_apps + 1);

  // Ensure that the TYPE_REMOVE_DEFAULT_APP SyncItem was replaced with an
  // TYPE_APP entry, for both Profiles.
  for (int i = 0; i < 2; i++) {
    const ALSS::SyncItem* sync_item = GetSyncItem(GetProfile(i), app_id2);
    ASSERT_TRUE(sync_item);
    EXPECT_EQ(sync_pb::AppListSpecifics::TYPE_APP, sync_item->item_type);
  }
}

INSTANTIATE_TEST_SUITE_P(All, RemoveDefaultAppSyncTest, ::testing::Bool());

// Install some apps on both clients, then sync. Move an app on one client
// to a folder and sync. The app lists, including folders, should match.
IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, MoveToFolder) {
  ASSERT_TRUE(SetupSync());
  WaitForExtensionServicesToLoad();
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  const int kNumApps = 5;
  std::vector<std::string> app_ids;
  for (int i = 0; i < kNumApps; ++i) {
    app_ids.push_back(InstallHostedApp(GetProfile(0), i));
    InstallHostedApp(GetProfile(1), i);
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  size_t index = 2u;
  std::string folder_id = "Folder 0";
  SyncAppListHelper::GetInstance()->MoveAppToFolder(GetProfile(0),
                                                    app_ids[index], folder_id);

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, FolderAddRemove) {
  ASSERT_TRUE(SetupSync());
  WaitForExtensionServicesToLoad();
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  const int kNumApps = 10;
  std::vector<std::string> app_ids;
  for (int i = 0; i < kNumApps; ++i) {
    app_ids.push_back(InstallHostedApp(GetProfile(0), i));
    InstallHostedApp(GetProfile(1), i);
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  // Move a few apps to a folder.
  const size_t kNumAppsToMove = 3;
  std::string folder_id = "Folder 0";
  // The folder will be created at the end of the list; always move the
  // non default items in the list.
  // Note: We don't care about the order of items in Chrome, so when we
  //       changes a file's folder, its index in the list remains unchanged.
  //       The |kNumAppsToMove| items to move are
  //       app_ids[item_index..(item_index+kNumAppsToMove-1)].
  size_t item_index = kNumDefaultApps;
  for (size_t i = 0; i < kNumAppsToMove; ++i) {
    SyncAppListHelper::GetInstance()->MoveAppToFolder(
        GetProfile(0), app_ids[item_index + i], folder_id);
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  // Remove one app from the folder.
  SyncAppListHelper::GetInstance()->MoveAppFromFolder(GetProfile(0), app_ids[0],
                                                      folder_id);

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  // Remove remaining apps from the folder (deletes folder).
  for (size_t i = 1; i < kNumAppsToMove; ++i) {
    SyncAppListHelper::GetInstance()->MoveAppFromFolder(GetProfile(0),
                                                        app_ids[0], folder_id);
  }

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  // Move apps back to a (new) folder.
  for (size_t i = 0; i < kNumAppsToMove; ++i) {
    SyncAppListHelper::GetInstance()->MoveAppToFolder(
        GetProfile(0), app_ids[item_index], folder_id);
  }

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());
}
