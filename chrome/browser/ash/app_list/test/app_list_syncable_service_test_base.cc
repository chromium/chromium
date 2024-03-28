// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/test/app_list_syncable_service_test_base.h"

#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "chrome/browser/ash/app_list/chrome_app_list_model_updater.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"

namespace test {

AppListSyncableServiceTestBase::AppListSyncableServiceTestBase() = default;

AppListSyncableServiceTestBase::~AppListSyncableServiceTestBase() = default;

void AppListSyncableServiceTestBase::SetUp() {
  AppListTestBase::SetUp();

  // Make sure we have a Profile Manager.
  DCHECK(temp_dir_.CreateUniqueTempDir());
  TestingBrowserProcess::GetGlobal()->SetProfileManager(
      std::make_unique<ProfileManagerWithoutInit>(temp_dir_.GetPath()));

  app_list_syncable_service_ =
      std::make_unique<app_list::AppListSyncableService>(profile_.get());
  content::RunAllTasksUntilIdle();
}

void AppListSyncableServiceTestBase::RestartSyncableService() {
  app_list_syncable_service_ =
      std::make_unique<app_list::AppListSyncableService>(profile_.get());
  content::RunAllTasksUntilIdle();
}

void AppListSyncableServiceTestBase::RemoveAllExistingItems() {
  std::vector<std::string> existing_item_ids;
  for (const auto& pair : app_list_syncable_service()->sync_items()) {
    existing_item_ids.emplace_back(pair.first);
  }

  for (std::string& id : existing_item_ids) {
    app_list_syncable_service()->RemoveItem(id, /*is_uninstall=*/true);
  }

  content::RunAllTasksUntilIdle();
}

void AppListSyncableServiceTestBase::InstallExtension(
    extensions::Extension* extension) {
  const syncer::StringOrdinal& page_ordinal =
      syncer::StringOrdinal::CreateInitialOrdinal();
  service()->OnExtensionInstalled(extension, page_ordinal,
                                  extensions::kInstallFlagNone);
  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();
}

void AppListSyncableServiceTestBase::RemoveExtension(const std::string& id) {
  service()->UninstallExtension(id, extensions::UNINSTALL_REASON_FOR_TESTING,
                                nullptr);

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();
}

std::vector<std::string>
AppListSyncableServiceTestBase::GetOrderedItemIdsFromModelUpdater() {
  std::vector<ChromeAppListItem*> items;
  AppListModelUpdater* model_updater = GetModelUpdater();
  for (size_t i = 0; i < model_updater->ItemCount(); ++i)
    items.push_back(model_updater->ItemAtForTest(i));
  std::sort(items.begin(), items.end(),
            [](ChromeAppListItem* const& item1,
               ChromeAppListItem* const& item2) -> bool {
              return item1->position().LessThan(item2->position());
            });
  std::vector<std::string> ids;
  for (auto*& item : items)
    ids.push_back(item->id());

  return ids;
}

std::vector<std::string>
AppListSyncableServiceTestBase::GetOrderedNamesFromModelUpdater() {
  const std::vector<std::string> ids = GetOrderedItemIdsFromModelUpdater();
  std::vector<std::string> names;
  for (const auto& id : ids) {
    names.push_back(GetModelUpdater()->FindItem(id)->name());
  }
  return names;
}

std::vector<std::string>
AppListSyncableServiceTestBase::GetOrderedItemIdsFromSyncableService() {
  std::vector<std::string> ids;
  app_list::AppListSyncableService* service = app_list_syncable_service();
  const auto& sync_items = service->sync_items();
  for (const auto& id_item_mapping : sync_items) {
    ids.push_back(id_item_mapping.first);
  }

  std::sort(ids.begin(), ids.end(),
            [service](const std::string& id1, const std::string& id2) {
              return service->GetSyncItem(id1)->item_ordinal.LessThan(
                  service->GetSyncItem(id2)->item_ordinal);
            });

  return ids;
}

std::vector<std::string>
AppListSyncableServiceTestBase::GetOrderedNamesFromSyncableService() {
  const std::vector<std::string> ids = GetOrderedItemIdsFromSyncableService();
  std::vector<std::string> names;
  for (const auto& id : ids) {
    names.push_back(app_list_syncable_service()->GetSyncItem(id)->item_name);
  }
  return names;
}

std::vector<std::vector<std::string>> AppListSyncableServiceTestBase::
    GetNamesOfSortedItemsPerPageFromSyncableService() {
  std::vector<std::vector<std::string>> pages;
  pages.emplace_back();

  const std::vector<std::string> ids = GetOrderedItemIdsFromSyncableService();
  for (const auto& id : ids) {
    const app_list::AppListSyncableService::SyncItem* item =
        app_list_syncable_service()->GetSyncItem(id);
    if (!item->parent_id.empty())
      continue;
    if (item->item_type == sync_pb::AppListSpecifics::TYPE_PAGE_BREAK) {
      pages.emplace_back();
      continue;
    }
    pages.back().push_back(
        app_list_syncable_service()->GetSyncItem(id)->item_name);
  }
  return pages;
}

syncer::StringOrdinal AppListSyncableServiceTestBase::GetPositionFromSyncData(
    const std::string& id) const {
  return app_list_syncable_service_->GetSyncItem(id)->item_ordinal;
}

ChromeAppListModelUpdater* AppListSyncableServiceTestBase::GetModelUpdater() {
  return static_cast<ChromeAppListModelUpdater*>(
      app_list_syncable_service_->GetModelUpdater());
}

const app_list::AppListSyncableService::SyncItem*
AppListSyncableServiceTestBase::GetSyncItem(const std::string& id) const {
  return app_list_syncable_service_->GetSyncItem(id);
}

}  // namespace test
