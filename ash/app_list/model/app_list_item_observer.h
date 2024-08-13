// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_APP_LIST_ITEM_OBSERVER_H_
#define ASH_APP_LIST_MODEL_APP_LIST_ITEM_OBSERVER_H_

#include "ash/app_list/model/app_list_model_export.h"
#include "base/observer_list_types.h"

namespace ash {
enum class AppListConfigType;

class APP_LIST_MODEL_EXPORT AppListItemObserver : public base::CheckedObserver {
 public:
  // Invoked after item's icon is changed.
  // |config_type| The app list configuration type for which the item icon
  // changed.
  virtual void ItemIconChanged(AppListConfigType config_type) {}

  // Invoked after the item's default icon changes.
  virtual void ItemDefaultIconChanged() {}

  // Invoked after item's icon version number is changed.
  virtual void ItemIconVersionChanged() {}

  // Invoked after item's name is changed.
  virtual void ItemNameChanged() {}

  // Invoked after item's host badge icon is changed.
  virtual void ItemHostBadgeIconChanged() {}

  // Invoked when the item's notification badge visibility is changed.
  virtual void ItemBadgeVisibilityChanged() {}

  // Invoked when the item's notification badge color is changed.
  virtual void ItemBadgeColorChanged() {}

  // Invoked when the item's "new install" badge is added or removed.
  virtual void ItemIsNewInstallChanged() {}

  // Invoked when the item is about to be destroyed.
  virtual void ItemBeingDestroyed() {}

  // Invoked when the item progress is updated.
  virtual void ItemProgressUpdated() {}

  // Invoked when the item app status is updated.
  virtual void ItemAppStatusUpdated() {}

  // Invoked when the item app collection id is updated.
  virtual void ItemAppCollectionIdChanged() {}

 protected:
  ~AppListItemObserver() override;
};

}  // namespace ash

#endif  // ASH_APP_LIST_MODEL_APP_LIST_ITEM_OBSERVER_H_
