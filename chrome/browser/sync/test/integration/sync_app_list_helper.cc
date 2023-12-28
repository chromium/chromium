// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_app_list_helper.h"

#include <map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "chrome/browser/ash/app_list/chrome_app_list_model_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/extensions/sync_helper.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_system.h"

using app_list::AppListSyncableService;
using app_list::AppListSyncableServiceFactory;

SyncAppListHelper* SyncAppListHelper::GetInstance() {
  SyncAppListHelper* instance = base::Singleton<SyncAppListHelper>::get();
  instance->SetupIfNecessary(sync_datatype_helper::test());
  return instance;
}

SyncAppListHelper::SyncAppListHelper() = default;

SyncAppListHelper::~SyncAppListHelper() = default;

void SyncAppListHelper::SetupIfNecessary(SyncTest* test) {
  if (setup_completed_) {
    DCHECK_EQ(test, test_);
    return;
  }
  test_ = test;
  for (Profile* profile : test_->GetAllProfiles()) {
    extensions::ExtensionSystem::Get(profile)->InitForRegularProfile(
        true /* extensions_enabled */);
  }

  setup_completed_ = true;
}

bool SyncAppListHelper::AppListMatch(Profile* profile1, Profile* profile2) {
  AppListSyncableService* service1 =
      AppListSyncableServiceFactory::GetForProfile(profile1);
  AppListSyncableService* service2 =
      AppListSyncableServiceFactory::GetForProfile(profile2);
  // Note: sync item entries may not exist in verifier, but item lists should
  // match.
  if (service1->GetModelUpdater()->ItemCount() !=
      service2->GetModelUpdater()->ItemCount()) {
    LOG(ERROR) << "Model item count: "
               << service1->GetModelUpdater()->ItemCount()
               << " != " << service2->GetModelUpdater()->ItemCount();
    return false;
  }
  bool res = true;
  for (size_t i = 0; i < service1->GetModelUpdater()->ItemCount(); ++i) {
    ChromeAppListItem* item1 = service1->GetModelUpdater()->ItemAtForTest(i);
    size_t index2;
    if (!service2->GetModelUpdater()->FindItemIndexForTest(item1->id(),
                                                           &index2)) {
      LOG(ERROR) << " Item(" << i << ") in profile1: " << item1->ToDebugString()
                 << " Not in profile2.";
      res = false;
      continue;
    }

    ChromeAppListItem* item2 =
        service2->GetModelUpdater()->ItemAtForTest(index2);
    if (item1->CompareForTest(item2)) {
      continue;
    }

    LOG(ERROR) << "Item(" << i << ") in profile1: " << item1->ToDebugString()
               << " != "
               << "Item(" << i << ") in profile2: " << item2->ToDebugString();
    res = false;
  }
  return res;
}

bool SyncAppListHelper::AllProfilesHaveSameAppList(size_t* size_out) {
  const std::vector<raw_ptr<Profile, VectorExperimental>>& profiles =
      test_->GetAllProfiles();
  for (Profile* profile : profiles) {
    if (profile != profiles.front() &&
        !AppListMatch(profiles.front(), profile)) {
      DVLOG(1) << "Profile1: "
               << AppListSyncableServiceFactory::GetForProfile(profile);
      PrintAppList(profile);
      DVLOG(1) << "Profile2: "
               << AppListSyncableServiceFactory::GetForProfile(
                      profiles.front());
      PrintAppList(profiles.front());
      return false;
    }
  }
  if (size_out) {
    *size_out = AppListSyncableServiceFactory::GetForProfile(profiles.front())
                    ->GetModelUpdater()
                    ->ItemCount();
  }
  return true;
}

void SyncAppListHelper::MoveAppToFolder(Profile* profile,
                                        const std::string& id,
                                        const std::string& folder_id) {
  AppListSyncableService* service =
      AppListSyncableServiceFactory::GetForProfile(profile);
  service->GetModelUpdater()->SetItemFolderId(id, folder_id);
}

void SyncAppListHelper::MoveAppFromFolder(Profile* profile,
                                          const std::string& id,
                                          const std::string& folder_id) {
  AppListSyncableService* service =
      AppListSyncableServiceFactory::GetForProfile(profile);
  ChromeAppListItem* folder =
      service->GetModelUpdater()->FindFolderItem(folder_id);
  if (!folder) {
    LOG(ERROR) << "Folder not found: " << folder_id;
    return;
  }
  service->GetModelUpdater()->SetItemFolderId(id, "");
}

void SyncAppListHelper::PrintAppList(Profile* profile) {
  AppListSyncableService* service =
      AppListSyncableServiceFactory::GetForProfile(profile);
  // Build a map from each folder item's id to a list of its child items.
  std::map<const std::string, std::vector<ChromeAppListItem*>> children;
  for (size_t i = 0; i < service->GetModelUpdater()->ItemCount(); ++i) {
    ChromeAppListItem* item = service->GetModelUpdater()->ItemAtForTest(i);
    if (!item->folder_id().empty()) {
      children[item->folder_id()].push_back(item);
    }
  }
  for (size_t i = 0; i < service->GetModelUpdater()->ItemCount(); ++i) {
    ChromeAppListItem* item = service->GetModelUpdater()->ItemAtForTest(i);
    // Skip if it's not a top level item.
    if (!item->folder_id().empty()) {
      continue;
    }
    std::string label = base::StringPrintf("Item(%d): ", static_cast<int>(i));
    PrintItem(profile, item, label);
    // Print children if it has any.
    if (children.count(item->id())) {
      DCHECK(item->is_folder());
      std::vector<ChromeAppListItem*>& child_items =
          children[item->folder_id()];
      for (size_t j = 0; j < child_items.size(); ++j) {
        ChromeAppListItem* child_item = child_items[j];
        std::string child_label =
            base::StringPrintf(" Folder Item(%d): ", static_cast<int>(j));
        PrintItem(profile, child_item, child_label);
      }
    }
  }
}

void SyncAppListHelper::PrintItem(Profile* profile,
                                  ChromeAppListItem* item,
                                  const std::string& label) {
  extensions::AppSorting* s =
      extensions::ExtensionSystem::Get(profile)->app_sorting();
  std::string id = item->id();
  DVLOG(1) << label << item->ToDebugString()
           << " Page: " << s->GetPageOrdinal(id).ToDebugString().substr(0, 8)
           << " Item: "
           << s->GetAppLaunchOrdinal(id).ToDebugString().substr(0, 8);
}
