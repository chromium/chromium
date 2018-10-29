// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_app_list_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ui/app_list/page_break_constants.h"
#include "components/browser_sync/profile_sync_service.h"
#include "extensions/browser/extension_system.h"

namespace {

bool AllProfilesHaveSameAppList() {
  return SyncAppListHelper::GetInstance()->AllProfilesHaveSameAppList();
}

// Returns true if sync items from |service1| match to sync items in |service2|.
bool SyncItemsMatch(const app_list::AppListSyncableService* service1,
                    const app_list::AppListSyncableService* service2) {
  if (service1->sync_items().size() != service2->sync_items().size())
    return false;

  for (const auto& it : service1->sync_items()) {
    const app_list::AppListSyncableService::SyncItem* item1 = it.second.get();
    const app_list::AppListSyncableService::SyncItem* item2 =
        service2->GetSyncItem(it.first);
    if (!item2)
      return false;
    if (item1->item_id != item2->item_id ||
        item1->item_type != item2->item_type ||
        item1->item_name != item2->item_name ||
        item1->parent_id != item2->parent_id ||
        !item1->item_ordinal.EqualsOrBothInvalid(item2->item_ordinal) ||
        !item1->item_pin_ordinal.EqualsOrBothInvalid(item2->item_pin_ordinal)) {
      return false;
    }
  }
  return true;
}

class AppListSyncUpdateWaiter
    : public StatusChangeChecker,
      public app_list::AppListSyncableService::Observer {
 public:
  explicit AppListSyncUpdateWaiter(app_list::AppListSyncableService* service)
      : service_(service) {
    service_->AddObserverAndStart(this);
  }

  ~AppListSyncUpdateWaiter() override {
    service_->RemoveObserver(this);
  }

  // StatusChangeChecker:
  std::string GetDebugMessage() const override {
    return "AwaitAppListSyncUpdated";
  }

  bool IsExitConditionSatisfied() override {
    return service_updated_;
  }

  // app_list::AppListSyncableService::Observer:
  void OnSyncModelUpdated() override {
    service_updated_ = true;
    StopWaiting();
  }

 private:
  app_list::AppListSyncableService* const service_;
  bool service_updated_ = false;

  DISALLOW_COPY_AND_ASSIGN(AppListSyncUpdateWaiter);
};

}  // namespace

class SingleClientAppListSyncTest : public SyncTest {
 public:
  SingleClientAppListSyncTest() : SyncTest(SINGLE_CLIENT) {}

  ~SingleClientAppListSyncTest() override {}

  // SyncTest
  bool SetupClients() override {
    if (!SyncTest::SetupClients())
      return false;

    // Init SyncAppListHelper to ensure that the extension system is initialized
    // for each Profile.
    SyncAppListHelper::GetInstance();
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientAppListSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientAppListSyncTest, AppListEmpty) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

IN_PROC_BROWSER_TEST_F(SingleClientAppListSyncTest, AppListSomeApps) {
  ASSERT_TRUE(SetupSync());

  const size_t kNumApps = 5;
  for (int i = 0; i < static_cast<int>(kNumApps); ++i) {
    apps_helper::InstallApp(GetProfile(0), i);
    apps_helper::InstallApp(verifier(), i);
  }

  app_list::AppListSyncableService* service =
      app_list::AppListSyncableServiceFactory::GetForProfile(verifier());

  // Default apps: chrome + web store + internal apps + number of default page
  // breaks.
  const size_t kNumDefaultApps =
      2u +
      app_list::GetNumberOfInternalAppsShowInLauncherForTest(
          /*apps_name=*/nullptr, GetProfile(0)) +
      app_list::kDefaultPageBreakAppIdsLength;
  ASSERT_EQ(kNumApps + kNumDefaultApps, service->GetNumSyncItemsForTest());

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

IN_PROC_BROWSER_TEST_F(SingleClientAppListSyncTest, LocalStorage) {
  ASSERT_TRUE(SetupSync());

  Profile* profile = GetProfile(0);
  app_list::AppListSyncableService* service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile);
  browser_sync::ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);

  const size_t kNumApps = 5;
  syncer::StringOrdinal pin_position =
      syncer::StringOrdinal::CreateInitialOrdinal();
  std::vector<std::string> app_ids;
  for (int i = 0; i < static_cast<int>(kNumApps); ++i) {
    app_ids.push_back(apps_helper::InstallApp(profile, i));
    service->SetPinPosition(app_ids.back(), pin_position);
    pin_position = pin_position.CreateAfter();
  }
  SyncAppListHelper::GetInstance()->MoveAppToFolder(profile, app_ids[2],
                                                    "folder1");
  SyncAppListHelper::GetInstance()->MoveAppToFolder(profile, app_ids[3],
                                                    "folder2");

  app_list::AppListSyncableService compare_service(
      profile, extensions::ExtensionSystem::Get(profile));

  // Make sure that that on start, when sync has not been started yet, model
  // content is filled from local prefs and it matches latest state.
  EXPECT_TRUE(SyncItemsMatch(service, &compare_service));

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  // Disable app sync.
  sync_service->OnUserChoseDatatypes(false, syncer::ModelTypeSet());

  // Change data when sync is off.
  for (const auto& app_id : app_ids) {
    service->SetPinPosition(app_id, pin_position);
    pin_position = pin_position.CreateAfter();
  }
  SyncAppListHelper::GetInstance()->MoveAppFromFolder(profile, app_ids[0],
                                                      "folder1");
  SyncAppListHelper::GetInstance()->MoveAppFromFolder(profile, app_ids[0],
                                                      "folder2");

  EXPECT_FALSE(SyncItemsMatch(service, &compare_service));

  // Restore sync and sync data should override local changes.
  sync_service->OnUserChoseDatatypes(true, syncer::ModelTypeSet());
  EXPECT_TRUE(AppListSyncUpdateWaiter(service).Wait());
  EXPECT_TRUE(SyncItemsMatch(service, &compare_service));
}
