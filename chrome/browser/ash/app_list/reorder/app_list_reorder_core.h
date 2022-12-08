// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_REORDER_APP_LIST_REORDER_CORE_H_
#define CHROME_BROWSER_ASH_APP_LIST_REORDER_APP_LIST_REORDER_CORE_H_

#include <vector>

#include "chrome/browser/ash/app_list/reorder/app_list_reorder_util.h"

namespace app_list {
namespace reorder {

// Returns reorder params which indicate the changes in ordinals to ensure
// `order` among the specified sync items. This function ensures that the
// number of updates is as few as possible.
std::vector<reorder::ReorderParam> GenerateReorderParamsForSyncItems(
    ash::AppListSortOrder order,
    const AppListSyncableService::SyncItemMap& sync_item_map);

// Similar to `GenerateReorderParamsForSyncItems()` but accepts a vector of
// `ChromeAppListItem` pointers as the parameter.
std::vector<reorder::ReorderParam> GenerateReorderParamsForAppListItems(
    ash::AppListSortOrder order,
    const std::vector<const ChromeAppListItem*>& app_list_items);

// Calcuates the target position so that `order` is still maintained among the
// specified items after:
// (1) the item matched by `metadata` is added to the model updater if it does
// not exist in the model updater yet, or
// (2) the item matched by `metadata` is set with the target position if it is
// already in the model updater. In this case, if the item's current position
// maintains the sort order, `target_position` should be the current position.
// Returns whether `target_position` is set. The return value is false if such
// a target position does not exist. In this case, `target_position` is not set.
// `local_items` indicates the elements in the active app list model before
// calling this function. Note that different devices may have different sets of
// app list items. It is why the parameter is named `local_items`.
// `global_items` is the sync data of the items across synced devices. If
// `global_items` is null, position is calculated only based on `local_items`.
bool CalculateItemPositionInOrder(
    ash::AppListSortOrder order,
    const ash::AppListItemMetadata& metadata,
    const std::vector<const ChromeAppListItem*>& local_items,
    const AppListSyncableService::SyncItemMap* global_items,
    syncer::StringOrdinal* target_position);

// Returns the foremost item position across syncable devices.
syncer::StringOrdinal CalculateFrontPosition(
    const AppListSyncableService::SyncItemMap& sync_item_map);

// Returns the entropy (i.e. the ratio of the out-of-order item number to
// the total number) based on the specified order.
float CalculateEntropyForTest(ash::AppListSortOrder order,
                              AppListModelUpdater* model_updater);

}  // namespace reorder
}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_REORDER_APP_LIST_REORDER_CORE_H_
