// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/test/fake_app_list_model_updater.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "extensions/common/constants.h"

FakeAppListModelUpdater::FakeAppListModelUpdater(
    Profile* profile,
    app_list::reorder::AppListReorderDelegate* order_delegate)
    : profile_(profile) {}

FakeAppListModelUpdater::~FakeAppListModelUpdater() = default;

void FakeAppListModelUpdater::AddItem(std::unique_ptr<ChromeAppListItem> item) {
  items_.push_back(std::move(item));
}

void FakeAppListModelUpdater::AddAppItemToFolder(
    std::unique_ptr<ChromeAppListItem> item,
    const std::string& folder_id,
    bool add_from_local) {
  ChromeAppListItem::TestApi test_api(item.get());
  test_api.SetFolderId(folder_id);
  items_.push_back(std::move(item));
}

void FakeAppListModelUpdater::RemoveItem(const std::string& id,
                                         bool is_uninstall) {
  size_t index;
  if (FindItemIndexForTest(id, &index)) {
    const std::string folder_id = items_[index]->folder_id();
    items_.erase(items_.begin() + index);

    if (folder_id.empty())
      return;

    // Remove the parent folder if the folder is empty.
    int folder_item_count = 0;
    for (const auto& item : items_) {
      if (item->folder_id() == folder_id)
        ++folder_item_count;
    }
    if (!folder_item_count)
      RemoveItem(folder_id, is_uninstall);
  }
}

void FakeAppListModelUpdater::SetItemIconAndColor(
    const std::string& id,
    const gfx::ImageSkia& icon,
    const ash::IconColor& icon_color,
    bool is_placeholder_icon) {
  ++update_image_count_;
  if (update_image_count_ == expected_update_image_count_ &&
      !icon_updated_callback_.is_null()) {
    std::move(icon_updated_callback_).Run();
  }
}

void FakeAppListModelUpdater::SetItemFolderId(const std::string& id,
                                              const std::string& folder_id) {
  ChromeAppListItem* item = FindItem(id);
  item->SetFolderId(folder_id);
  for (AppListModelUpdaterObserver& observer : observers_)
    observer.OnAppListItemUpdated(item);
}

void FakeAppListModelUpdater::SetItemPosition(
    const std::string& id,
    const syncer::StringOrdinal& new_position) {
  ChromeAppListItem* item = FindItem(id);
  if (!item)
    return;

  ChromeAppListItem::TestApi(item).SetPosition(new_position);
  for (AppListModelUpdaterObserver& observer : observers_)
    observer.OnAppListItemUpdated(item);
}

void FakeAppListModelUpdater::SetItemName(const std::string& id,
                                          const std::string& new_name) {
  ChromeAppListItem* item = FindItem(id);
  if (!item)
    return;

  ChromeAppListItem::TestApi(item).SetName(new_name);
}

void FakeAppListModelUpdater::SetSearchEngineIsGoogle(bool is_google) {
  search_engine_is_google_ = is_google;
}

ChromeAppListItem* FakeAppListModelUpdater::FindItem(const std::string& id) {
  size_t index;
  if (FindItemIndexForTest(id, &index))
    return items_[index].get();
  return nullptr;
}

std::vector<const ChromeAppListItem*> FakeAppListModelUpdater::GetItems()
    const {
  std::vector<const ChromeAppListItem*> item_pointer_vec;
  for (auto& item : items_)
    item_pointer_vec.push_back(item.get());
  return item_pointer_vec;
}

size_t FakeAppListModelUpdater::ItemCount() {
  return items_.size();
}

std::vector<ChromeAppListItem*> FakeAppListModelUpdater::GetTopLevelItems()
    const {
  std::vector<ChromeAppListItem*> top_level_items;
  for (auto& item : items_) {
    DCHECK(item->position().IsValid())
        << "Item with invalid position: id=" << item->id()
        << ", name=" << item->name() << ", is_folder=" << item->is_folder();
    if (item->folder_id().empty() && item->position().IsValid())
      top_level_items.emplace_back(item.get());
  }
  return top_level_items;
}

std::set<std::string> FakeAppListModelUpdater::GetTopLevelItemIds() const {
  std::set<std::string> item_ids;
  for (auto& item : items_) {
    if (item->folder_id().empty())
      item_ids.insert(item->id());
  }
  return item_ids;
}

ChromeAppListItem* FakeAppListModelUpdater::ItemAtForTest(size_t index) {
  return index < items_.size() ? items_[index].get() : nullptr;
}

bool FakeAppListModelUpdater::FindItemIndexForTest(const std::string& id,
                                                   size_t* index) {
  for (size_t i = 0; i < items_.size(); ++i) {
    if (items_[i]->id() == id) {
      *index = i;
      return true;
    }
  }
  return false;
}

ChromeAppListItem* FakeAppListModelUpdater::FindFolderItem(
    const std::string& folder_id) {
  ChromeAppListItem* item = FindItem(folder_id);
  return (item && item->is_folder()) ? item : nullptr;
}

syncer::StringOrdinal FakeAppListModelUpdater::GetPositionBeforeFirstItem()
    const {
  return GetPositionBeforeFirstItemInternal(GetTopLevelItems());
}

void FakeAppListModelUpdater::GetContextMenuModel(
    const std::string& id,
    ash::AppListItemContext item_context,
    GetMenuModelCallback callback) {
  std::move(callback).Run(nullptr);
}

void FakeAppListModelUpdater::ActivateChromeItem(const std::string& id,
                                                 int event_flags) {}

void FakeAppListModelUpdater::LoadAppIcon(const std::string& id) {
  ChromeAppListItem* item = FindItem(id);
  if (!item)
    return;
  item->LoadIcon();
}

size_t FakeAppListModelUpdater::BadgedItemCount() {
  return 0u;
}

bool FakeAppListModelUpdater::SearchEngineIsGoogle() {
  return search_engine_is_google_;
}

void FakeAppListModelUpdater::RecalculateWouldTriggerLauncherSearchIph() {}

void FakeAppListModelUpdater::PublishSearchResults(
    const std::vector<raw_ptr<ChromeSearchResult, VectorExperimental>>& results,
    const std::vector<ash::AppListSearchResultCategory>& categories) {
  search_results_ = results;
}

void FakeAppListModelUpdater::ClearSearchResults() {
  search_results_.clear();
}

void FakeAppListModelUpdater::UpdateAppItemFromSyncItem(
    app_list::AppListSyncableService::SyncItem* sync_item,
    bool update_name,
    bool update_folder) {
  // In chrome & ash:
  ChromeAppListItem* chrome_item = FindItem(sync_item->item_id);
  if (!chrome_item)
    return;

  VLOG(2) << this << " UpdateAppItemFromSyncItem: " << sync_item->ToString();
  if (sync_item->item_ordinal.IsValid() &&
      (!chrome_item->position().IsValid() ||
       !chrome_item->position().Equals(sync_item->item_ordinal))) {
    // This updates the position in both chrome and ash:
    SetItemPosition(chrome_item->id(), sync_item->item_ordinal);
  }
  // Only update the item name if it is a Folder or the name is empty.
  if (update_name && sync_item->item_name != chrome_item->name() &&
      (chrome_item->is_folder() || chrome_item->name().empty())) {
    // This updates the name in both chrome and ash:
    SetItemName(chrome_item->id(), sync_item->item_name);
  }
  if (update_folder && chrome_item->folder_id() != sync_item->parent_id) {
    VLOG(2) << " Moving Item To Folder: " << sync_item->parent_id;
    // This updates the folder in both chrome and ash:
    SetItemFolderId(chrome_item->id(), sync_item->parent_id);
  }
}

void FakeAppListModelUpdater::AddObserver(
    AppListModelUpdaterObserver* observer) {
  observers_.AddObserver(observer);
}

void FakeAppListModelUpdater::RemoveObserver(
    AppListModelUpdaterObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FakeAppListModelUpdater::WaitForIconUpdates(size_t expected_updates) {
  base::RunLoop run_loop;
  expected_update_image_count_ = expected_updates + update_image_count_;
  icon_updated_callback_ = run_loop.QuitClosure();
  run_loop.Run();
}
