// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_REORDER_APP_LIST_REORDER_DELEGATE_H_
#define CHROME_BROWSER_ASH_APP_LIST_REORDER_APP_LIST_REORDER_DELEGATE_H_

#include "components/sync/model/string_ordinal.h"

namespace ash {
enum class AppListSortOrder;
struct AppListItemMetadata;
}  // namespace ash

namespace app_list {
namespace reorder {

// An interface to provide functions for app list sorting.
class AppListReorderDelegate {
 public:
  virtual ~AppListReorderDelegate() {}

  // Sets the preferred sorting order.
  virtual void SetAppListPreferredOrder(ash::AppListSortOrder order) = 0;

  // Returns the front position among all sync items.
  virtual syncer::StringOrdinal CalculateGlobalFrontPosition() const = 0;

  // Calcuates the target position so that the permanent sorting order is still
  // maintained on all sync items after:
  // (1) the item matched by `metadata` is added to the model updater if it does
  // not exist in the model updater yet, or
  // (2) the item matched by `metadata` is set with the target position if it is
  // already in the model updater. In this case, if the item's current position
  // maintains the sort order, `target_position` should be the current position.
  // The target position is returned through the parameter. Returns whether such
  // a target position could exist, in which case `target_position` is set.
  virtual bool CalculateItemPositionInPermanentSortOrder(
      const ash::AppListItemMetadata& metadata,
      syncer::StringOrdinal* target_position) const = 0;

  // Returns the sorting order that is saved in perf service and gets shared
  // among synced devices.
  virtual ash::AppListSortOrder GetPermanentSortingOrder() const = 0;
};

}  // namespace reorder
}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_REORDER_APP_LIST_REORDER_DELEGATE_H_
