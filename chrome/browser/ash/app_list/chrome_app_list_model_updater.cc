// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/chrome_app_list_model_updater.h"

#include <unordered_map>
#include <utility>

#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_item_list.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_controller.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/app_list_sync_model_sanitizer.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item_manager.h"
#include "chrome/browser/ash/app_list/reorder/app_list_reorder_core.h"
#include "chrome/browser/ash/app_list/reorder/app_list_reorder_delegate.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/app_icon_color_cache/app_icon_color_cache.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "extensions/common/constants.h"
#include "ui/base/models/menu_model.h"
#include "ui/display/screen.h"

namespace {

std::unique_ptr<ash::AppListItem> CreateAppListItem(
    std::unique_ptr<ash::AppListItemMetadata> metadata,
    ash::AppListModelDelegate* delegate) {
  std::unique_ptr<ash::AppListItem> app_list_item =
      metadata->is_folder
          ? std::make_unique<ash::AppListFolderItem>(metadata->id, delegate)
          : std::make_unique<ash::AppListItem>(metadata->id);
  app_list_item->SetMetadata(std::move(metadata));
  return app_list_item;
}

}  // namespace

// TemporarySortManager --------------------------------------------------------

// A helper class for managing temporary sort data. Used only when the app list
// is under temporary sort.
class ChromeAppListModelUpdater::TemporarySortManager {
 public:
  TemporarySortManager(
      ash::AppListSortOrder temporary_order,
      const std::map<std::string, std::unique_ptr<ChromeAppListItem>>&
          permanent_items)
      : temporary_order_(temporary_order) {
    // Fill permanent position storage.
    for (const auto& id_item_pair : permanent_items)
      AddPermanentPosition(id_item_pair.first, id_item_pair.second->position());
  }
  TemporarySortManager(const TemporarySortManager&) = delete;
  TemporarySortManager& operator=(const TemporarySortManager&) = delete;
  ~TemporarySortManager() = default;

  bool HasId(const std::string& id) const {
    return permanent_position_storage_.find(id) !=
           permanent_position_storage_.cend();
  }

  const syncer::StringOrdinal& GetPermanentPositionForId(
      const std::string& id) const {
    auto iter = permanent_position_storage_.find(id);
    DCHECK(iter != permanent_position_storage_.cend());
    return iter->second;
  }

  void AddPermanentPosition(const std::string& id,
                            const syncer::StringOrdinal& position) {
    DCHECK(!HasId(id));
    permanent_position_storage_.emplace(id, position);
  }

  void SetPermanentPosition(const std::string& id,
                            const syncer::StringOrdinal& position) {
    auto iter = permanent_position_storage_.find(id);
    DCHECK(iter != permanent_position_storage_.end());
    iter->second = position;
  }

  void DeletePermanentPosition(const std::string& id) {
    auto iter = permanent_position_storage_.find(id);
    DCHECK(iter != permanent_position_storage_.end());
    permanent_position_storage_.erase(iter);
  }

  void Deactivate() {
    DCHECK(is_active_);
    is_active_ = false;
  }

  ash::AppListSortOrder temporary_order() const { return temporary_order_; }
  void set_temporary_order(ash::AppListSortOrder new_order) {
    temporary_order_ = new_order;
  }

  bool is_active() const { return is_active_; }

 private:
  // Indicates the app list order that is not committed yet. When under
  // temporary sort, the app list items on the local device (i.e. the device on
  // which `temporary_order` is triggered) are sorted with `temporary_order_`.
  // But the new positions are not synced with other devices.
  ash::AppListSortOrder temporary_order_ = ash::AppListSortOrder::kCustom;

  // For each key-value pair, the key is an item id while the value is the
  // permanent position (i.e. the position that is shared with other synced
  // devices) of the item indexed by the key.
  std::map<std::string, syncer::StringOrdinal> permanent_position_storage_;

  // `TemporarySortManager` is active when:
  // (1) App list is under temporary sort, and
  // (2) App list is not in the progress of ending temporary sort.
  // Note that model updates will not be propagated to observers until the
  // temporary sort manager is deactivated
  bool is_active_ = true;
};

// ChromeAppListModelUpdater ---------------------------------------------------

ChromeAppListModelUpdater::ChromeAppListModelUpdater(
    Profile* profile,
    app_list::reorder::AppListReorderDelegate* order_delegate,
    app_list::AppListSyncModelSanitizer* sync_model_sanitizer)
    : profile_(profile),
      order_delegate_(order_delegate),
      sync_model_sanitizer_(sync_model_sanitizer),
      item_manager_(std::make_unique<ChromeAppListItemManager>()),
      model_(this) {
  DCHECK(order_delegate_);
  model_.AddObserver(this);
}

ChromeAppListModelUpdater::~ChromeAppListModelUpdater() {
  model_.RemoveObserver(this);
}

void ChromeAppListModelUpdater::SetActive(bool active) {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::SetActive");
  is_active_ = active;

  if (active) {
    ash::AppListController::Get()->SetActiveModel(
        model_id(), &model_, &search_model_, &quick_app_access_model_);
  } else if (is_under_temporary_sort()) {
    // Commit the temporary order when the model updater is deactivated.
    EndTemporarySortAndTakeAction(EndAction::kCommit);
  }
}

void ChromeAppListModelUpdater::AddItem(
    std::unique_ptr<ChromeAppListItem> app_item) {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::AddItem");
  std::unique_ptr<ash::AppListItemMetadata> item_data =
      app_item->CloneMetadata();

  // Add to Chrome first leave all updates to observer methods.
  item_manager_->AddChromeItem(std::move(app_item));
  const std::string folder_id = item_data->folder_id;
  item_data->folder_id.clear();
  if (folder_id.empty()) {
    model_.AddItem(CreateAppListItem(std::move(item_data), this));
  } else {
    model_.AddItemToFolder(CreateAppListItem(std::move(item_data), this),
                           folder_id);
  }
}

void ChromeAppListModelUpdater::AddAppItemToFolder(
    std::unique_ptr<ChromeAppListItem> app_item,
    const std::string& folder_id,
    bool add_from_local) {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::AddAppItemToFolder");
  DCHECK(!app_item->is_folder());

  if (is_under_temporary_sort()) {
    // Store `app_item`'s position before calculating a new position under the
    // temporary sorting order.
    DCHECK(temporary_sort_manager_->is_active());
    temporary_sort_manager_->AddPermanentPosition(app_item->id(),
                                                  app_item->position());

    // Calculate `app_item`'s position under the temporary order.
    syncer::StringOrdinal position_under_temporary_order;
    bool is_successful = app_list::reorder::CalculateItemPositionInOrder(
        temporary_sort_manager_->temporary_order(), app_item->metadata(),
        GetItems(),
        /*global_items=*/nullptr, &position_under_temporary_order);

    // When the app list is under temporary sorting, local items should be
    // ordered. Therefore `is_successful` should be true.
    DCHECK(is_successful);

    if (!is_successful) {
      DCHECK(!position_under_temporary_order.IsValid());
      position_under_temporary_order =
          order_delegate_->CalculateGlobalFrontPosition();
    }

    DCHECK(position_under_temporary_order.IsValid());
    app_item->SetChromePosition(position_under_temporary_order);
  }

  std::unique_ptr<ash::AppListItemMetadata> item_data =
      app_item->CloneMetadata();
  // Add to Chrome first leave all updates to observer methods.
  // TODO(https://crbug.com/1261796): now we are using the APIs provided by
  // `ChromeAppListItem` to set item attributes. It is buggy-prone because
  // when an item's attribute is updated in this way, `item_manager_` is not
  // aware of the update. We should ensure that after an item is added to
  // `item_manager_` the item is always updated through `item_manager_`.
  app_item->SetChromeFolderId(folder_id);
  ChromeAppListItem* item_added =
      item_manager_->AddChromeItem(std::move(app_item));
  item_data->folder_id.clear();
  model_.AddItemToFolder(CreateAppListItem(std::move(item_data), this),
                         folder_id);
  // Set the item's default icon if it has one.
  if (!item_added->icon().isNull()) {
    const bool is_placeholder_icon = item_added->is_placeholder_icon();
    ash::AppListItem* item = model_.FindItem(item_added->id());
    item->SetDefaultIconAndColor(item_added->icon(), item_added->icon_color(),
                                 is_placeholder_icon);
  }

  if (add_from_local) {
    // If the app list is under temporary sort and the new app is installed from
    // the local device (i.e. the device on which temporary sorting is
    // initiated), commit the temporary sorting order.
    if (is_under_temporary_sort()) {
      EndTemporarySortAndTakeAction(EndAction::kCommit);
    } else if (folder_id.empty()) {
      // If an app is a new install, sanitize the page breaks if productivity
      // launcher is enabled.
      // No need to sanitize page breaks after committing temporary sort, as
      // page breaks get sanitized when the sorted order is set.
      // Adding an item to a folder is not expected for new items, but also does
      // not impact the top level grid pagination structure.
      sync_model_sanitizer_->SanitizePageBreaks(GetTopLevelItemIds(),
                                                /*reset_page_breaks=*/false);
    }
  }
}

void ChromeAppListModelUpdater::RemoveItem(const std::string& id,
                                           bool is_uninstall) {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::RemoveItem");
  // The item matched by `id` may be unavailable on the local device.
  if (!model_.FindItem(id)) {
    return;
  }

  ash::AppIconColorCache::GetInstance(profile_).RemoveColorDataForApp(id);

  // Copy the ID to the stack since it may to be destroyed in
  // RemoveChromeItem(). See crbug.com/1190347.
  std::string id_copy = id;

  item_manager_->RemoveChromeItem(id_copy);
  model_.DeleteItem(id_copy);

  if (is_uninstall) {
    // When item deletion is triggered by local app uninstallation instead of
    // sync, commits the temporary order if any.
    if (is_under_temporary_sort()) {
      EndTemporarySortAndTakeAction(EndAction::kCommit);
    } else {
      // NOTE: Committing temporary sort will also reset page breaks, so they
      // don't have to be sanitized again in that case.
      sync_model_sanitizer_->SanitizePageBreaks(GetTopLevelItemIds(),
                                                /*reset_page_breaks=*/false);
    }
  }
}

void ChromeAppListModelUpdater::SetStatus(ash::AppListModelStatus status) {
  model_.SetStatus(status);
}

void ChromeAppListModelUpdater::SetSearchEngineIsGoogle(bool is_google) {
  search_engine_is_google_ = is_google;
  search_model_.SetSearchEngineIsGoogle(is_google);
}

void ChromeAppListModelUpdater::RecalculateWouldTriggerLauncherSearchIph() {
  TRACE_EVENT0(
      "ui",
      "ChromeAppListModelUpdater::RecalculateWouldTriggerLauncherSearchIph");
  raw_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile_);
  if (!tracker) {
    // Set false as a fail-safe behavior.
    search_model_.SetWouldTriggerLauncherSearchIph(false);
    return;
  }

  // `AddOnInitializedCallback` will call the callback immediately if it's
  // already initialized.
  tracker->AddOnInitializedCallback(base::BindOnce(
      &ChromeAppListModelUpdater::OnFeatureEngagementTrackerInitialized,
      weak_ptr_factory_.GetWeakPtr()));
}

void ChromeAppListModelUpdater::OnFeatureEngagementTrackerInitialized(
    bool success) {
  TRACE_EVENT0(
      "ui", "ChromeAppListModelUpdater::OnFeatureEngagementTrackerInitialized");
  if (!success) {
    // Set false as a fail-safe behavior.
    search_model_.SetWouldTriggerLauncherSearchIph(false);
    return;
  }

  // To be on a safer side, query tracker instance again to minimize the
  // duration of holding a tracker object.
  raw_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile_);
  if (!tracker) {
    // Set false as a fail-safe behavior.
    search_model_.SetWouldTriggerLauncherSearchIph(false);
    return;
  }

  search_model_.SetWouldTriggerLauncherSearchIph(tracker->WouldTriggerHelpUI(
      feature_engagement::kIPHLauncherSearchHelpUiFeature));
}

void ChromeAppListModelUpdater::PublishSearchResults(
    const std::vector<raw_ptr<ChromeSearchResult, VectorExperimental>>& results,
    const std::vector<ash::AppListSearchResultCategory>& categories) {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::PublishSearchResults");
  published_results_ = results;

  for (ChromeSearchResult* const result : results) {
    result->set_model_updater(this);
  }

  std::vector<std::unique_ptr<ash::SearchResult>> ash_results;
  std::vector<std::unique_ptr<ash::SearchResultMetadata>> result_data;
  for (ChromeSearchResult* result : results) {
    auto ash_result = std::make_unique<ash::SearchResult>();
    ash_result->SetMetadata(result->CloneMetadata());
    ash_results.push_back(std::move(ash_result));
  }
  search_model_.PublishResults(std::move(ash_results), categories);
}

void ChromeAppListModelUpdater::ClearSearchResults() {
  published_results_.clear();
  search_model_.DeleteAllResults();
}

std::vector<raw_ptr<ChromeSearchResult, VectorExperimental>>
ChromeAppListModelUpdater::GetPublishedSearchResultsForTest() {
  return published_results_;
}

void ChromeAppListModelUpdater::ActivateChromeItem(const std::string& id,
                                                   int event_flags) {
  ChromeAppListItem* item = FindItem(id);
  if (!item)
    return;
  DCHECK(!item->is_folder());
  item->PerformActivate(event_flags);
}

void ChromeAppListModelUpdater::LoadAppIcon(const std::string& id) {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::LoadAppIcon");
  ChromeAppListItem* item = FindItem(id);
  if (!item)
    return;
  item->LoadIcon();
}

void ChromeAppListModelUpdater::UpdateProgress(const std::string& id,
                                               float progress) {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::UpdateProgress");
  ChromeAppListItem* item = FindItem(id);
  if (!item) {
    return;
  }
  std::unique_ptr<ash::AppListItemMetadata> data = item->CloneMetadata();
  data->progress = progress;
  model_.SetItemMetadata(id, std::move(data));
}

void ChromeAppListModelUpdater::SetAccessibleName(const std::string& id,
                                                  const std::string& name) {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::UpdateAccessibleName");
  ChromeAppListItem* item = FindItem(id);
  if (!item) {
    return;
  }
  std::unique_ptr<ash::AppListItemMetadata> data = item->CloneMetadata();
  data->accessible_name = name;
  model_.SetItemMetadata(id, std::move(data));
}

bool ChromeAppListModelUpdater::ModelHasBeenReorderedInThisSession() {
  return has_requested_move_item_position_;
}

////////////////////////////////////////////////////////////////////////////////
// Methods only used by ChromeAppListItem that talk to ash directly.

void ChromeAppListModelUpdater::SetItemIconVersion(const std::string& id,
                                                   int icon_version) {
  ash::AppListItem* item = model_.FindItem(id);
  if (item)
    item->SetIconVersion(icon_version);
}

void ChromeAppListModelUpdater::SetItemIconAndColor(
    const std::string& id,
    const gfx::ImageSkia& icon,
    const ash::IconColor& icon_color,
    bool is_placeholder_icon) {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::SetItemIconAndColor");
  if (icon.isNull())
    return;

  // TODO(https://crbug.com/1346386): it is awkward to check both `chrome_item`
  // and `item`. Maybe remove one of them or both after investigation.
  ChromeAppListItem* chrome_item = FindItem(id);
  if (!chrome_item)
    return;

  ash::AppListItem* item = model_.FindItem(id);
  if (!item)
    return;

  base::AutoReset auto_reset(&item_with_icon_update_, chrome_item->id());

  const bool color_change = (icon_color != item->GetDefaultIconColor());

  // Two similar icons may generate the same extracted icon color value.
  // Therefore, always update the app list item icon.
  item->SetDefaultIconAndColor(icon, icon_color, is_placeholder_icon);

  std::unique_ptr<ash::AppListItemMetadata> data = item->CloneMetadata();
  MaybeUpdatePositionWhenIconColorChange(data.get());

  model_.SetItemMetadata(id, std::move(data));

  // Sync the icon color if the color changes. Note that the icon is not synced.
  // Therefore, we only check whether the color changes here.
  // NOTE: before the code change that introduces this code block, the item
  // position updates before the icon color. Therefore, call
  // `OnAppListItemUpdated()` after `AppListModel::SetItemMetadata` to keep this
  // order.
  if (color_change)
    OnAppListItemUpdated(item);
}

void ChromeAppListModelUpdater::SetItemBadgeIcon(
    const std::string& id,
    const gfx::ImageSkia& badge_icon) {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::SetItemBadgeIcon");
  if (badge_icon.isNull()) {
    return;
  }
  ash::AppListItem* item = model_.FindItem(id);
  if (!item) {
    return;
  }
  item->SetHostBadgeIcon(badge_icon);
}

void ChromeAppListModelUpdater::SetItemName(const std::string& id,
                                            const std::string& name) {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::SetItemName");
  ash::AppListItem* item = model_.FindItem(id);
  if (!item)
    return;
  std::unique_ptr<ash::AppListItemMetadata> data = item->CloneMetadata();
  data->name = name;
  model_.SetItemMetadata(id, std::move(data));
}

void ChromeAppListModelUpdater::SetAppStatus(const std::string& id,
                                             ash::AppStatus app_status) {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::SetAppStatus");
  ash::AppListItem* item = model_.FindItem(id);
  if (!item)
    return;
  std::unique_ptr<ash::AppListItemMetadata> data = item->CloneMetadata();
  data->app_status = app_status;
  model_.SetItemMetadata(id, std::move(data));
}

void ChromeAppListModelUpdater::SetItemPosition(
    const std::string& id,
    const syncer::StringOrdinal& new_position) {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::SetItemPosition");
  ash::AppListItem* item = model_.FindItem(id);
  if (!item)
    return;
  DCHECK(new_position.IsValid());
  std::unique_ptr<ash::AppListItemMetadata> data = item->CloneMetadata();
  data->position = new_position;
  model_.SetItemMetadata(id, std::move(data));
}

void ChromeAppListModelUpdater::SetItemIsSystemFolder(const std::string& id,
                                                      bool is_system_folder) {
  ash::AppListItem* item = model_.FindItem(id);
  if (!item)
    return;
  std::unique_ptr<ash::AppListItemMetadata> data = item->CloneMetadata();
  data->is_system_folder = is_system_folder;
  model_.SetItemMetadata(id, std::move(data));
}

void ChromeAppListModelUpdater::SetIsNewInstall(const std::string& id,
                                                bool is_new_install) {
  ash::AppListItem* item = model_.FindItem(id);
  if (item)
    item->SetIsNewInstall(is_new_install);  // Notifies observers.
}

void ChromeAppListModelUpdater::SetItemFolderId(const std::string& id,
                                                const std::string& folder_id) {
  ash::AppListItem* item = model_.FindItem(id);
  if (!item)
    return;
  std::unique_ptr<ash::AppListItemMetadata> data = item->CloneMetadata();
  data->folder_id = folder_id;
  model_.SetItemMetadata(id, std::move(data));
}

void ChromeAppListModelUpdater::SetNotificationBadgeColor(const std::string& id,
                                                          const SkColor color) {
  ash::AppListItem* item = model_.FindItem(id);
  if (item)
    item->SetNotificationBadgeColor(color);
}

////////////////////////////////////////////////////////////////////////////////
// Methods only used by ChromeSearchResult that talk to ash directly.

void ChromeAppListModelUpdater::SetSearchResultMetadata(
    const std::string& id,
    std::unique_ptr<ash::SearchResultMetadata> metadata) {
  ash::SearchResult* result = search_model_.FindSearchResult(metadata->id);
  if (result)
    result->SetMetadata(std::move(metadata));
}

////////////////////////////////////////////////////////////////////////////////
// Methods for item querying

ChromeAppListItem* ChromeAppListModelUpdater::FindItem(const std::string& id) {
  return item_manager_->FindItem(id);
}

size_t ChromeAppListModelUpdater::ItemCount() {
  return item_manager_->ItemCount();
}

std::vector<const ChromeAppListItem*> ChromeAppListModelUpdater::GetItems()
    const {
  std::vector<const ChromeAppListItem*> item_pointers;
  const std::map<std::string, std::unique_ptr<ChromeAppListItem>>& items =
      item_manager_->items();
  for (auto& entry : items)
    item_pointers.push_back(entry.second.get());
  return item_pointers;
}

std::set<std::string> ChromeAppListModelUpdater::GetTopLevelItemIds() const {
  std::set<std::string> item_ids;
  ash::AppListItemList* item_list = model_.top_level_item_list();
  for (size_t i = 0; i < item_list->item_count(); ++i)
    item_ids.insert(item_list->item_at(i)->id());
  return item_ids;
}

std::vector<ChromeAppListItem*> ChromeAppListModelUpdater::GetTopLevelItems()
    const {
  return item_manager_->GetTopLevelItems();
}

ChromeAppListItem* ChromeAppListModelUpdater::ItemAtForTest(size_t index) {
  const std::map<std::string, std::unique_ptr<ChromeAppListItem>>& items =
      item_manager_->items();
  DCHECK_LT(index, items.size());
  DCHECK_LE(0u, index);
  auto it = items.cbegin();
  for (size_t i = 0; i < index; ++i)
    ++it;
  return it->second.get();
}

ChromeAppListItem* ChromeAppListModelUpdater::FindFolderItem(
    const std::string& folder_id) {
  ChromeAppListItem* item = FindItem(folder_id);
  return (item && item->is_folder()) ? item : nullptr;
}

bool ChromeAppListModelUpdater::FindItemIndexForTest(const std::string& id,
                                                     size_t* index) {
  const std::map<std::string, std::unique_ptr<ChromeAppListItem>>& items =
      item_manager_->items();

  *index = 0;
  for (auto it = items.begin(); it != items.end(); ++it) {
    if (it->second->id() == id)
      return true;
    ++(*index);
  }
  return false;
}

bool ChromeAppListModelUpdater::SearchEngineIsGoogle() {
  return search_engine_is_google_;
}

size_t ChromeAppListModelUpdater::BadgedItemCount() {
  return item_manager_->BadgedItemCount();
}

void ChromeAppListModelUpdater::GetContextMenuModel(
    const std::string& id,
    ash::AppListItemContext item_context,
    GetMenuModelCallback callback) {
  ChromeAppListItem* item = FindItem(id);
  // TODO(stevenjb/jennyz): Implement this for folder items.
  // TODO(newcomer): Add histograms for folder items.
  if (!item || item->is_folder()) {
    std::move(callback).Run(nullptr);
    return;
  }
  item->GetContextMenuModel(item_context, std::move(callback));
}

syncer::StringOrdinal ChromeAppListModelUpdater::GetPositionBeforeFirstItem()
    const {
  return GetPositionBeforeFirstItemInternal(GetTopLevelItems());
}

////////////////////////////////////////////////////////////////////////////////
// Methods for AppListSyncableService

void ChromeAppListModelUpdater::UpdateAppItemFromSyncItem(
    app_list::AppListSyncableService::SyncItem* sync_item,
    bool update_name,
    bool update_folder) {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::UpdateAppItemFromSyncItem");
  // In chrome & ash:
  ChromeAppListItem* chrome_item = FindItem(sync_item->item_id);
  if (!chrome_item)
    return;

  std::unique_ptr<ash::AppListItemMetadata> data = chrome_item->CloneMetadata();
  VLOG(2) << this << " UpdateAppItemFromSyncItem: " << sync_item->ToString();
  bool has_changes = false;
  const bool position_change =
      (sync_item->item_ordinal.IsValid() &&
       (!chrome_item->position().IsValid() ||
        !chrome_item->position().Equals(sync_item->item_ordinal)));
  if (position_change) {
    has_changes = true;
    data->position = sync_item->item_ordinal;
  }

  // Only update the item name if it is a Folder or the name is empty.
  if (update_name && sync_item->item_name != chrome_item->name() &&
      (chrome_item->is_folder() || chrome_item->name().empty())) {
    has_changes = true;
    data->name = sync_item->item_name;
  }

  const bool folder_change =
      (update_folder && chrome_item->folder_id() != sync_item->parent_id);
  if (folder_change) {
    has_changes = true;
    VLOG(2) << " Moving Item To Folder: " << sync_item->parent_id;
    data->folder_id = sync_item->parent_id;
  }

  // Note that the icon and the icon color are not set here because the update
  // on the icon as well the icon color is initiated from the ash side.

  if (!has_changes)
    return;

  // This updates the position in both chrome and ash:
  model_.SetItemMetadata(chrome_item->id(), std::move(data));

  // The code below handles position change or folder change under temporary
  // sort.

  // Note that `UpdateAppItemFromSyncItem()` can be called when temporary sort
  // order is committed. In this case, temporary sort is in the progress of
  // ending so nothing to do.
  // TODO(https://crbug.com/1268080): currently committing sort order could
  // change local item positions. It is due to the sync items that are not
  // existent on the local device. When this issue gets fixed, check that
  // `temporary_sort_manager_` is active when it is not null.
  const bool is_temporary_sort_active =
      (is_under_temporary_sort() && temporary_sort_manager_->is_active());
  if (!is_temporary_sort_active || (!position_change && !folder_change))
    return;

  // TODO(crbug.com/40201875): the features of temporary sort are
  // partially implemented. The cases of app installation/removal are not
  // handled right now. As a result, `temporary_sort_manager_` may not cover all
  // items. Therefore manually check the existence of `id` here. When all the
  // features are completed, replace with a DCHECK statement.
  const std::string& item_id = sync_item->item_id;
  if (position_change && temporary_sort_manager_->HasId(item_id)) {
    temporary_sort_manager_->SetPermanentPosition(item_id,
                                                  sync_item->item_ordinal);
  }

  // Revert the temporary sort order if an item's position or parent folder
  // changes on a remote device. Because the remote update may conflict with the
  // temporary sort order.
  // The item local positions are not committed because remote updates, usually
  // triggered by user in active ways on remote devices such as dragging then
  // dropping an item, are believed to reflect user expectation on item layout
  // so remote updates are more important than revertible local updates.
  EndTemporarySortAndTakeAction(EndAction::kRevert);
}

void ChromeAppListModelUpdater::AddObserver(
    AppListModelUpdaterObserver* observer) {
  observers_.AddObserver(observer);
}

void ChromeAppListModelUpdater::RemoveObserver(
    AppListModelUpdaterObserver* observer) {
  observers_.RemoveObserver(observer);
}

ash::AppListSortOrder ChromeAppListModelUpdater::GetTemporarySortOrderForTest()
    const {
  return temporary_sort_manager_->temporary_order();
}

////////////////////////////////////////////////////////////////////////////////
// Methods called from Ash:

void ChromeAppListModelUpdater::OnAppListItemAdded(ash::AppListItem* item) {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::OnAppListItemAdded");
  ChromeAppListItem* chrome_item = FindItem(item->id());
  // If the item already exists, we should have set its information properly.
  if (!chrome_item) {
    // Otherwise, we detect an item is created in Ash which is not added into
    // our Chrome list yet. This only happens when a folder is created or when a
    // page break is added.
    DCHECK(item->is_folder());
    std::unique_ptr<ChromeAppListItem> new_item =
        std::make_unique<ChromeAppListItem>(profile_, item->id(), this);
    new_item->SetMetadata(item->CloneMetadata());
    chrome_item = item_manager_->AddChromeItem(std::move(new_item));
  }

  // Notify observers that an item is added to the AppListModel in ash.
  // Note that items of apps are added from Chrome side so there would be an
  // existing |chrome_item| when running here.
  MaybeNotifyObserversOfItemChange(chrome_item, ItemChangeType::kAdd);
}

void ChromeAppListModelUpdater::OnAppListItemUpdated(ash::AppListItem* item) {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::OnAppListItemUpdated");
  ChromeAppListItem* chrome_item = FindItem(item->id());

  // Ignore the item if it does not exist. This happens when a race occurs
  // between the browser and ash. e.g. An item is removed on browser side while
  // there is an in-flight OnItemUpdated() call from ash.
  if (!chrome_item)
    return;

  // Do not update the icon or the color of `chrome_item` if `item` is not
  // in icon update process.
  if (!item_with_icon_update_ || *item_with_icon_update_ != item->id()) {
    item->SetDefaultIconAndColor(chrome_item->icon(), chrome_item->icon_color(),
                                 item->GetMetadata()->is_placeholder_icon);
  }

  const std::string copy_id = item->id();
  item_manager_->UpdateChromeItem(copy_id, item->CloneMetadata());
  MaybeNotifyObserversOfItemChange(chrome_item, ItemChangeType::kUpdate);
}

void ChromeAppListModelUpdater::OnAppListItemWillBeDeleted(
    ash::AppListItem* item) {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::OnAppListItemWillBeDeleted");
  if (is_under_temporary_sort()) {
    DCHECK(temporary_sort_manager_->HasId(item->id()));
    temporary_sort_manager_->DeletePermanentPosition(item->id());
  }

  if (!item->is_folder())
    return;

  ChromeAppListItem* chrome_item = FindItem(item->id());
  if (!chrome_item) {
    LOG(ERROR) << "OnAppListItemWillBeDeleted: " << item->id()
               << " does not exist.";
    return;
  }

  MaybeNotifyObserversOfItemChange(chrome_item, ItemChangeType::kDelete);
  item_manager_->RemoveChromeItem(item->id());
}

void ChromeAppListModelUpdater::RequestMoveItemToFolder(
    std::string id,
    const std::string& folder_id) {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::RequestMoveItemToFolder");
  DCHECK(!folder_id.empty());

  ash::AppListItem* item = model_.FindItem(id);
  if (item) {
    // Indicates the item's target position after moving to folder. The target
    // position relies on the items under the target folder. Therefore calculate
    // `target_position` before moving the item to the folder.
    syncer::StringOrdinal target_position;

    const syncer::StringOrdinal old_position =
        item_manager_->FindItem(id)->position();

    const bool is_sorted = is_under_temporary_sort() ||
                           order_delegate_->GetPermanentSortingOrder() !=
                               ash::AppListSortOrder::kCustom;

    // Verify that when the app list is under sorting, `old_position` should be
    // valid. But the case that `old_position` is invalid is handled for safety.
    DCHECK(!is_sorted || old_position.IsValid());

    if (is_sorted && old_position.IsValid()) {
      // When items are sorted, item positions are set so all items in the model
      // are in correct sort order (regardless of their parent IDs). Therefore,
      // item position will be in correct sort order relative to items already
      // in the target folder.
      target_position = old_position;
    } else {
      ChromeAppListItem* last_child =
          item_manager_->FindLastChildInFolder(folder_id);
      target_position = last_child
                            ? last_child->position().CreateAfter()
                            : syncer::StringOrdinal::CreateInitialOrdinal();
    }

    std::unique_ptr<ash::AppListItemMetadata> data = item->CloneMetadata();
    data->folder_id = folder_id;
    data->position = target_position;
    model_.SetItemMetadata(id, std::move(data));
    has_requested_move_item_position_ = true;
  }

  // When user moves a local item to a folder, the user is believed to accept
  // the item layout after reordering. Therefore local positions are
  // committed.
  if (is_under_temporary_sort()) {
    EndTemporarySortAndTakeAction(EndAction::kCommit);
  } else {
    // NOTE: Committing temporary sort will also reset page breaks, so they
    // don't have to be sanitized again in that case.
    sync_model_sanitizer_->SanitizePageBreaks(GetTopLevelItemIds(),
                                              /*reset_page_breaks=*/false);
  }
}

void ChromeAppListModelUpdater::RequestMoveItemToRoot(
    std::string id,
    syncer::StringOrdinal target_position) {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::RequestMoveItemToRoot");
  ash::AppListItem* item = model_.FindItem(id);
  if (!item)
    return;

  DCHECK(!item->folder_id().empty());

  std::unique_ptr<ash::AppListItemMetadata> data = item->CloneMetadata();
  data->folder_id = "";
  data->position = target_position;
  model_.SetItemMetadata(id, std::move(data));
  has_requested_move_item_position_ = true;

  if (is_under_temporary_sort()) {
    EndTemporarySortAndTakeAction(EndAction::kCommitAndClearSort);
  } else {
    ResetPrefSortOrderInNonTemporaryMode(
        ash::AppListOrderUpdateEvent::kItemMovedToRoot);
    sync_model_sanitizer_->SanitizePageBreaks(GetTopLevelItemIds(),
                                              /*reset_page_breaks=*/false);
  }
}

void ChromeAppListModelUpdater::RequestAppListSort(
    ash::AppListSortOrder order) {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::RequestAppListSort");
  CHECK_NE(ash::AppListSortOrder::kCustom, order);

  // Ignore sort requests if sorting makes no visual difference.
  if (item_manager_->ItemCount() < 2)
    return;

  if (is_under_temporary_sort()) {
    DCHECK(temporary_sort_manager_->is_active());

    // Sorting can be triggered when app list is under temporary sort.
    if (temporary_sort_manager_->temporary_order() == order) {
      // Order does not change so nothing to do.
      return;
    }

    // Permanent positions are already stored. Therefore only update
    // the temporary order here.
    temporary_sort_manager_->set_temporary_order(order);
  } else {
    // The app list was not under temporary sort - initialize it now.
    temporary_sort_manager_ =
        std::make_unique<TemporarySortManager>(order, item_manager_->items());
  }

  // The code below changes the item positions based on the new order.

  // Calculate item positions under temporary sort order.
  std::vector<app_list::reorder::ReorderParam> reorder_params =
      app_list::reorder::GenerateReorderParamsForAppListItems(order,
                                                              GetItems());

  // Return early if reordering is not required.
  if (!reorder_params.size())
    return;

  ash::AppListController::Get()->UpdateAppListWithNewTemporarySortOrder(
      order,
      /*animate=*/true,
      base::BindOnce(
          &ChromeAppListModelUpdater::UpdateItemPositionWithReorderParam,
          weak_ptr_factory_.GetWeakPtr(), std::move(reorder_params)));
}

void ChromeAppListModelUpdater::RequestAppListSortRevert() {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::RequestAppListSortRevert");
  if (!is_under_temporary_sort())
    return;

  EndTemporarySortAndTakeAction(EndAction::kRevert);
}

void ChromeAppListModelUpdater::RequestCommitTemporarySortOrder() {
  if (!is_under_temporary_sort()) {
    return;
  }

  EndTemporarySortAndTakeAction(EndAction::kCommit);
}

void ChromeAppListModelUpdater::RequestPositionUpdate(
    std::string id,
    const syncer::StringOrdinal& new_position,
    ash::RequestPositionUpdateReason reason) {
  DCHECK(FindItem(id));
  SetItemPosition(id, new_position);

  // Commit positions and clear the sort order if a local item is moved.
  if (reason == ash::RequestPositionUpdateReason::kMoveItem) {
    has_requested_move_item_position_ = true;
    if (is_under_temporary_sort()) {
      EndTemporarySortAndTakeAction(EndAction::kCommitAndClearSort);
    } else {
      ResetPrefSortOrderInNonTemporaryMode(
          ash::AppListOrderUpdateEvent::kItemMoved);

      // NOTE: Committing temporary sort will also reset page breaks, so they
      // don't have to be sanitized again in that case.
      sync_model_sanitizer_->SanitizePageBreaks(GetTopLevelItemIds(),
                                                /*reset_page_breaks=*/false);
    }
  }
}

void ChromeAppListModelUpdater::RequestDefaultPositionForModifiedOrder() {
  const std::map<std::string, std::unique_ptr<ChromeAppListItem>>& items =
      item_manager_->items();
  for (const auto& id_item_pair : items) {
    ChromeAppListItem* item = id_item_pair.second.get();
    RequestPositionUpdate(id_item_pair.first,
                          item->CalculateDefaultPositionForModifiedOrder(),
                          ash::RequestPositionUpdateReason::kMoveItem);
  }
}

std::string ChromeAppListModelUpdater::RequestFolderCreation(
    std::string merge_target_id,
    std::string item_to_merge_id) {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::RequestFolderCreation");
  // Folder creation is a user action, so temporary sort state should end.
  // If feature to position the folder to correct sorted position is disabled,
  // clear the sort.
  const bool under_temporary_sort = is_under_temporary_sort();
  if (under_temporary_sort) {
    EndTemporarySortAndTakeAction(EndAction::kCommit);
  }

  has_requested_move_item_position_ = true;

  ash::AppListItem* target_item = model_.FindItem(merge_target_id);
  DCHECK(target_item);
  DCHECK(!target_item->is_folder());
  DCHECK_EQ("", target_item->folder_id());

  ash::AppListItem* item_to_merge = model_.FindItem(item_to_merge_id);
  DCHECK(item_to_merge);
  DCHECK(!item_to_merge->is_folder());

  const ash::AppListSortOrder current_sort_order =
      order_delegate_->GetPermanentSortingOrder();

  // Create a new folder.
  const std::string new_folder_id = ash::AppListFolderItem::GenerateId();
  std::unique_ptr<ChromeAppListItem> new_folder_item =
      std::make_unique<ChromeAppListItem>(profile_, new_folder_id, this);
  new_folder_item->SetChromeIsFolder(true);

  // Calculate the new folder's sorted position - if apps grid is not sorted,
  // default to the original item position.
  syncer::StringOrdinal target_position = target_item->position();
  if (current_sort_order != ash::AppListSortOrder::kCustom) {
    syncer::StringOrdinal sorted_position;
    bool has_sorted_position =
        order_delegate_->CalculateItemPositionInPermanentSortOrder(
            new_folder_item->metadata(), &sorted_position);
    if (has_sorted_position)
      target_position = sorted_position;
  }
  new_folder_item->SetChromePosition(target_position);

  ChromeAppListItem* chrome_item =
      item_manager_->AddChromeItem(std::move(new_folder_item));
  model_.AddItem(CreateAppListItem(chrome_item->CloneMetadata(), this));

  // Adjust parent and position of the item getting mergrd into the target item.
  std::unique_ptr<ash::AppListItemMetadata> item_to_merge_data =
      item_to_merge->CloneMetadata();
  item_to_merge_data->folder_id = new_folder_id;

  // When sort is enabled, the item positing relative to `target_item` should
  // already be correct, otherwise move the item at the end of the folder.
  if (current_sort_order == ash::AppListSortOrder::kCustom)
    item_to_merge_data->position = target_item->position().CreateAfter();
  model_.SetItemMetadata(item_to_merge_id, std::move(item_to_merge_data));

  // Set the target item new folder ID.
  std::unique_ptr<ash::AppListItemMetadata> target_data =
      target_item->CloneMetadata();
  target_data->folder_id = new_folder_id;
  model_.SetItemMetadata(merge_target_id, std::move(target_data));

  sync_model_sanitizer_->SanitizePageBreaks(GetTopLevelItemIds(),
                                            /*reset_page_breaks=*/false);
  return new_folder_id;
}

void ChromeAppListModelUpdater::RequestFolderRename(
    std::string folder_id,
    const std::string& new_name) {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::RequestFolderRename");
  ChromeAppListItem* folder_item = FindItem(folder_id);
  if (!folder_item)
    return;

  ash::AppListSortOrder current_sort_order = ash::AppListSortOrder::kCustom;
  const bool under_temporary_sort = is_under_temporary_sort();
  if (under_temporary_sort) {
    current_sort_order = temporary_sort_manager_->temporary_order();
  } else {
    current_sort_order = order_delegate_->GetPermanentSortingOrder();
  }

  // If user tries to take an action, and rename a folder - commit temporary
  // sort.
  if (under_temporary_sort) {
    EndTemporarySortAndTakeAction(EndAction::kCommit);
  }

  folder_item->SetChromeName(new_name);

  bool position_changed = false;
  // If app list is sorted alphabetically, the folder name change impacts the
  // folder position within the sorted list.
  const bool is_name_sort =
      current_sort_order == ash::AppListSortOrder::kNameAlphabetical ||
      current_sort_order == ash::AppListSortOrder::kNameReverseAlphabetical ||
      current_sort_order ==
          ash::AppListSortOrder::kAlphabeticalEphemeralAppFirst;
  if (is_name_sort) {
    syncer::StringOrdinal sorted_position;
    position_changed =
        order_delegate_->CalculateItemPositionInPermanentSortOrder(
            folder_item->metadata(), &sorted_position);
    if (position_changed)
      folder_item->SetChromePosition(sorted_position);
  }

  model_.SetItemMetadata(folder_id, folder_item->CloneMetadata());

  if (position_changed) {
    sync_model_sanitizer_->SanitizePageBreaks(GetTopLevelItemIds(),
                                              /*reset_page_breaks=*/false);
  }
}

void ChromeAppListModelUpdater::OnAppListHidden() {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::OnAppListHidden");
  if (!is_under_temporary_sort())
    return;

  DCHECK(temporary_sort_manager_->is_active());

  // Commit the temporary sort order if app list gets hidden.
  EndTemporarySortAndTakeAction(EndAction::kCommit);
}

// Private methods -------------------------------------------------------------

void ChromeAppListModelUpdater::MaybeNotifyObserversOfItemChange(
    ChromeAppListItem* chrome_item,
    ItemChangeType type) {
  TRACE_EVENT0("ui",
               "ChromeAppListModelUpdater::MaybeNotifyObserversOfItemChange");
  // If `temporary_sort_manager_` is active, item changes are not propagated
  // to observers.
  if (is_under_temporary_sort() && temporary_sort_manager_->is_active())
    return;

  switch (type) {
    case ItemChangeType::kAdd:
      for (AppListModelUpdaterObserver& observer : observers_)
        observer.OnAppListItemAdded(chrome_item);
      break;
    case ItemChangeType::kUpdate:
      for (AppListModelUpdaterObserver& observer : observers_)
        observer.OnAppListItemUpdated(chrome_item);
      break;
    case ItemChangeType::kDelete:
      for (AppListModelUpdaterObserver& observer : observers_)
        observer.OnAppListItemWillBeDeleted(chrome_item);
      break;
  }
}

void ChromeAppListModelUpdater::EndTemporarySortAndTakeAction(
    EndAction action) {
  TRACE_EVENT0("ui",
               "ChromeAppListModelUpdater::EndTemporarySortAndTakeAction");
  CHECK(is_under_temporary_sort() && temporary_sort_manager_->is_active());

  // Allow item updates to be propagated to observers.
  temporary_sort_manager_->Deactivate();

  base::OnceClosure update_position_closure;
  switch (action) {
    case EndAction::kCommit:
      CommitTemporaryPositions();
      order_delegate_->SetAppListPreferredOrder(
          temporary_sort_manager_->temporary_order());
      break;
    case EndAction::kRevert:
      update_position_closure = base::BindOnce(
          &ChromeAppListModelUpdater::UpdateItemPositionWithReorderParam,
          weak_ptr_factory_.GetWeakPtr(),
          CalculateReorderParamsForRevertOrder());
      break;
    case EndAction::kCommitAndClearSort:
      CommitTemporaryPositions();
      order_delegate_->SetAppListPreferredOrder(ash::AppListSortOrder::kCustom);
      break;
  }

  temporary_sort_manager_.reset();

  const bool animate = !update_position_closure.is_null();
  ash::AppListController::Get()->UpdateAppListWithNewTemporarySortOrder(
      /*new_order=*/std::nullopt, animate, std::move(update_position_closure));
}

void ChromeAppListModelUpdater::CommitTemporaryPositions() {
  TRACE_EVENT0("ui", "ChromeAppListModelUpdater::CommitTemporaryPositions");
  const std::map<std::string, std::unique_ptr<ChromeAppListItem>>& items =
      item_manager_->items();
  for (const auto& id_item_pair : items) {
    const syncer::StringOrdinal& temporary_position =
        id_item_pair.second->position();

    if (!temporary_position.IsValid()) {
      // Not sure whether this branch can be executed. Handle this case for
      // safety. TODO(crbug.com/40203095): check whether the positions
      // stored in `item_manager_` are always valid. If so, remove this code.
      continue;
    }

    // TODO(crbug.com/40201875): the features of temporary sort are
    // partially implemented. The cases of app installation/removal are not
    // handled right now. As a result, the ids in `temporary_sort_manager_`
    // can be inconsistent with those in `item_manager_`. Therefore manually
    // check the existence of `id` here. When the issue gets addressed, replace
    // with a DCHECK statement.
    const std::string& id = id_item_pair.first;
    if (!temporary_sort_manager_->HasId(id))
      continue;

    const syncer::StringOrdinal& permanent_position =
        temporary_sort_manager_->GetPermanentPositionForId(id);
    if (permanent_position.IsValid() &&
        permanent_position.Equals(temporary_position)) {
      // The temporary sort order does not modify the item's position so no
      // work to do for this item.
      continue;
    }

    // Propagate the item position update.
    MaybeNotifyObserversOfItemChange(id_item_pair.second.get(),
                                     ItemChangeType::kUpdate);
  }
}

std::vector<app_list::reorder::ReorderParam>
ChromeAppListModelUpdater::CalculateReorderParamsForRevertOrder() const {
  TRACE_EVENT0(
      "ui", "ChromeAppListModelUpdater::CalculateReorderParamsForRevertOrder");
  std::vector<app_list::reorder::ReorderParam> reorder_params;

  const std::map<std::string, std::unique_ptr<ChromeAppListItem>>& items =
      item_manager_->items();
  for (const auto& id_item_pair : items) {
    const std::string& id = id_item_pair.first;

    // TODO(crbug.com/40201875): the features of temporary sort are
    // partially implemented. The cases of app installation/removal are not
    // handled right now. As a result, the ids in `temporary_sort_manager_`
    // can be inconsistent with those in `item_manager_`. Therefore manually
    // check the existence of `id` here. When the issue gets addressed, replace
    // with a DCHECK statement.
    if (!temporary_sort_manager_->HasId(id))
      continue;

    const syncer::StringOrdinal& temporary_position =
        id_item_pair.second->position();
    const syncer::StringOrdinal& permanent_position =
        temporary_sort_manager_->GetPermanentPositionForId(id);

    if (temporary_position.IsValid() && permanent_position.IsValid() &&
        temporary_position.Equals(permanent_position)) {
      // Skip if `temporary_position` is equal to `permanent_position`.
      continue;
    }

    reorder_params.emplace_back(id, permanent_position);
  }

  return reorder_params;
}

void ChromeAppListModelUpdater::UpdateItemPositionWithReorderParam(
    const std::vector<app_list::reorder::ReorderParam>& reorder_params) {
  TRACE_EVENT0("ui",
               "ChromeAppListModelUpdater::UpdateItemPositionWithReorderParam");
  for (const auto& reorder_param : reorder_params)
    SetItemPosition(reorder_param.sync_item_id, reorder_param.ordinal);
  has_requested_move_item_position_ = true;
}

void ChromeAppListModelUpdater::ResetPrefSortOrderInNonTemporaryMode(
    ash::AppListOrderUpdateEvent event) {
  TRACE_EVENT0(
      "ui", "ChromeAppListModelUpdater::ResetPrefSortOrderInNonTemporaryMode");
  if (!order_delegate_ || order_delegate_->GetPermanentSortingOrder() ==
                              ash::AppListSortOrder::kCustom) {
    return;
  }

  order_delegate_->SetAppListPreferredOrder(ash::AppListSortOrder::kCustom);

  ReportPrefOrderClearAction(event,
                             display::Screen::GetScreen()->InTabletMode());
}

void ChromeAppListModelUpdater::MaybeUpdatePositionWhenIconColorChange(
    ash::AppListItemMetadata* data) {
  TRACE_EVENT0(
      "ui",
      "ChromeAppListModelUpdater::MaybeUpdatePositionWhenIconColorChange");
  // No op if the color info is invalid.
  if (!data->icon_color.IsValid())
    return;

  syncer::StringOrdinal position_under_color_order;
  bool success = false;
  if (is_under_temporary_sort() && temporary_sort_manager_->temporary_order() ==
                                       ash::AppListSortOrder::kColor) {
    success = app_list::reorder::CalculateItemPositionInOrder(
        temporary_sort_manager_->temporary_order(), *data, GetItems(),
        /*global_items=*/nullptr, &position_under_color_order);
  } else if (order_delegate_ && order_delegate_->GetPermanentSortingOrder() ==
                                    ash::AppListSortOrder::kColor) {
    success = order_delegate_->CalculateItemPositionInPermanentSortOrder(
        *data, &position_under_color_order);
  }

  if (success)
    data->position = position_under_color_order;
}
