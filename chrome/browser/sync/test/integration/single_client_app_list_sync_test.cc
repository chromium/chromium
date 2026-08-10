// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_app_list_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/app_constants/constants.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/protocol/app_list_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/constants.h"
#include "testing/gmock/include/gmock/gmock.h"

using syncer::UserSelectableOsTypeSet;
using syncer::UserSelectableTypeSet;
using testing::UnorderedElementsAre;

namespace {

std::ostream& operator<<(std::ostream& os,
                         const base::flat_set<std::string>& set) {
  os << "{";
  for (const std::string& element : set) {
    os << element << ", ";
  }
  os << "}";
  return os;
}

base::flat_set<std::string> GetServerAppListItemIds(
    fake_server::FakeServer* fake_server) {
  std::vector<sync_pb::SyncEntity> entities =
      fake_server->GetSyncEntitiesByDataType(syncer::APP_LIST);
  return base::MakeFlatSet<std::string>(
      entities,
      /*comp=*/{}, /*proj=*/[](const sync_pb::SyncEntity& e) {
        return e.specifics().app_list().item_id();
      });
}

// Waits for the APP_LIST entities in the server to be those with ids
// `expected_app_ids`. Default apps (Chrome and Web Store) are implicitly added.
class FakeServerAppListChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  explicit FakeServerAppListChecker(std::vector<std::string> expected_app_ids) {
    expected_app_ids.push_back(app_constants::kChromeAppId);
    expected_app_ids.push_back(extensions::kWebStoreAppId);
    expected_app_ids_ = base::MakeFlatSet<std::string>(expected_app_ids);
  }

  FakeServerAppListChecker(const FakeServerAppListChecker&) = delete;
  FakeServerAppListChecker& operator=(const FakeServerAppListChecker&) = delete;

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for app list item ids in fake server to match: "
        << expected_app_ids_ << ".";
    base::flat_set<std::string> actual_app_ids =
        GetServerAppListItemIds(fake_server());
    *os << " Actual app list item ids: " << actual_app_ids << ".";
    return expected_app_ids_ == actual_app_ids;
  }

 private:
  base::flat_set<std::string> expected_app_ids_;
};

// Returns true if sync items from |service| all have non-empty names.
bool SyncItemsHaveNames(const app_list::AppListSyncableService* service) {
  for (const auto& [item_id, item] : service->sync_items()) {
    if (item->item_name.empty()) {
      return false;
    }
  }
  return true;
}

// Returns true if sync items from |service1| match to sync items in |service2|.
bool SyncItemsMatch(const app_list::AppListSyncableService* service1,
                    const app_list::AppListSyncableService* service2) {
  if (service1->sync_items().size() != service2->sync_items().size()) {
    return false;
  }

  for (const auto& [item_id, item1] : service1->sync_items()) {
    const app_list::AppListSyncableService::SyncItem* item2 =
        service2->GetSyncItem(item_id);
    if (!item2) {
      return false;
    }
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
  explicit AppListSyncUpdateWaiter(app_list::AppListSyncableService* service) {
    observer_.Observe(service);
  }

  AppListSyncUpdateWaiter(const AppListSyncUpdateWaiter&) = delete;
  AppListSyncUpdateWaiter& operator=(const AppListSyncUpdateWaiter&) = delete;

  ~AppListSyncUpdateWaiter() override = default;

  // StatusChangeChecker:
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "AwaitAppListSyncUpdated";
    return service_updated_;
  }

  // app_list::AppListSyncableService::Observer:
  void OnSyncModelUpdated() override {
    service_updated_ = true;
    CheckExitCondition();
  }

 private:
  base::ScopedObservation<app_list::AppListSyncableService,
                          app_list::AppListSyncableService::Observer>
      observer_{this};
  bool service_updated_ = false;
};

}  // namespace

class SingleClientAppListSyncTest : public SyncTest {
 public:
  SingleClientAppListSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientAppListSyncTest() override = default;

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

  // This test suite is ChromeOS specific, where there's only Sync-the-feature.
  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return SetupSyncMode::kSyncTheFeature;
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientAppListSyncTest, AppListEmpty) {
  ASSERT_TRUE(SetupSync());

  // Default apps (Chrome and Web Store) are synced even when no user apps are
  // installed.
  EXPECT_THAT(GetServerAppListItemIds(GetFakeServer()),
              UnorderedElementsAre(app_constants::kChromeAppId,
                                   extensions::kWebStoreAppId));
}

IN_PROC_BROWSER_TEST_F(SingleClientAppListSyncTest, AppListSomeApps) {
  ASSERT_TRUE(SetupSync());

  const size_t kNumApps = 5;
  std::vector<std::string> app_ids;
  for (int i = 0; i < static_cast<int>(kNumApps); ++i) {
    app_ids.push_back(apps_helper::InstallHostedApp(GetProfile(0), i));
  }

  // Allow async callbacks to run, such as App Service Mojo calls.
  base::RunLoop().RunUntilIdle();

  app_list::AppListSyncableService* service =
      app_list::AppListSyncableServiceFactory::GetForProfile(GetProfile(0));

  // Default apps: chrome + web store.
  ASSERT_EQ(kNumApps + 2u, service->sync_items().size());

  EXPECT_TRUE(FakeServerAppListChecker(app_ids).Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientAppListSyncTest, LocalStorage) {
  ASSERT_TRUE(SetupSync());

  Profile* profile = GetProfile(0);
  app_list::AppListSyncableService* service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);

  const size_t kNumApps = 7;
  syncer::StringOrdinal pin_position =
      syncer::StringOrdinal::CreateInitialOrdinal();
  std::vector<std::string> app_ids;
  for (int i = 0; i < static_cast<int>(kNumApps); ++i) {
    app_ids.push_back(apps_helper::InstallHostedApp(profile, i));
  }

  // Allow async callbacks to run, such as App Service Mojo calls.
  base::RunLoop().RunUntilIdle();

  for (const std::string& app_id : app_ids) {
    service->SetPinPosition(app_id, pin_position);
    pin_position = pin_position.CreateAfter();
  }
  EXPECT_TRUE(SyncItemsHaveNames(service));

  auto folder_item1 = std::make_unique<ChromeAppListItem>(
      profile, "folder1", service->GetModelUpdater());
  folder_item1->SetChromeIsFolder(true);
  ChromeAppListItem::TestApi(folder_item1.get()).SetPosition(pin_position);
  pin_position = pin_position.CreateAfter();
  ChromeAppListItem::TestApi(folder_item1.get()).SetName("Folder 1");
  service->AddItem(std::move(folder_item1));

  auto folder_item2 = std::make_unique<ChromeAppListItem>(
      profile, "folder2", service->GetModelUpdater());
  folder_item2->SetChromeIsFolder(true);
  ChromeAppListItem::TestApi(folder_item2.get()).SetPosition(pin_position);
  ChromeAppListItem::TestApi(folder_item2.get()).SetName("Folder 2");
  service->AddItem(std::move(folder_item2));

  // Ensure that one folder has more than one child. Otherwise, the folder could
  // be deleted.
  SyncAppListHelper::GetInstance()->MoveAppToFolder(profile, app_ids[2],
                                                    "folder1");
  SyncAppListHelper::GetInstance()->MoveAppToFolder(profile, app_ids[3],
                                                    "folder2");
  SyncAppListHelper::GetInstance()->MoveAppToFolder(profile, app_ids[5],
                                                    "folder1");
  SyncAppListHelper::GetInstance()->MoveAppToFolder(profile, app_ids[6],
                                                    "folder2");

  app_list::AppListSyncableService compare_service(profile);

  // Make sure that that on start, when sync has not been started yet, model
  // content is filled from local prefs and it matches latest state.
  EXPECT_TRUE(SyncItemsMatch(service, &compare_service));

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  // Disable app sync by disabling all user-selectable types.
  sync_service->GetUserSettings()->SetSelectedOsTypes(
      /*sync_all_os_types=*/false, syncer::UserSelectableOsTypeSet());

  // Change data when sync is off.
  for (const std::string& app_id : app_ids) {
    service->SetPinPosition(app_id, pin_position);
    pin_position = pin_position.CreateAfter();
  }
  SyncAppListHelper::GetInstance()->MoveAppFromFolder(profile, app_ids[0],
                                                      "folder1");
  SyncAppListHelper::GetInstance()->MoveAppFromFolder(profile, app_ids[0],
                                                      "folder2");

  EXPECT_FALSE(SyncItemsMatch(service, &compare_service));

  // Restore app sync and sync data should override local changes.
  sync_service->GetUserSettings()->SetSelectedOsTypes(
      /*sync_all_os_types=*/true, syncer::UserSelectableOsTypeSet());
  EXPECT_TRUE(AppListSyncUpdateWaiter(service).Wait());
  EXPECT_TRUE(SyncItemsMatch(service, &compare_service));
}

class SingleClientAppListOsSyncTest : public SyncTest {
 public:
  SingleClientAppListOsSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientAppListOsSyncTest() override = default;

  // This test suite is ChromeOS specific, where there's only Sync-the-feature.
  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return SetupSyncMode::kSyncTheFeature;
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientAppListOsSyncTest,
                       AppListSyncedByOsSettings) {
  ASSERT_TRUE(SetupSync());
  syncer::SyncService* service = GetSyncService(0);
  syncer::SyncUserSettings* settings = service->GetUserSettings();

  // Initially app list is enabled.
  ASSERT_TRUE(settings->IsSyncEverythingEnabled());
  ASSERT_TRUE(settings->IsSyncAllOsTypesEnabled());
  ASSERT_TRUE(service->GetActiveDataTypes().Has(syncer::APP_LIST));

  // Disable all browser types.
  settings->SetSelectedTypes(false, UserSelectableTypeSet());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  // APP_LIST is still synced because it is an OS setting.
  EXPECT_TRUE(service->GetActiveDataTypes().Has(syncer::APP_LIST));

  // Disable OS types.
  settings->SetSelectedOsTypes(false, UserSelectableOsTypeSet());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  // APP_LIST is not synced.
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::APP_LIST));
}
