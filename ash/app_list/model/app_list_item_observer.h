// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_APP_LIST_ITEM_OBSERVER_H_
#define ASH_APP_LIST_MODEL_APP_LIST_ITEM_OBSERVER_H_

#include "ash/app_list/model/app_list_model_export.h"

namespace ash {
enum class AppListConfigType;

class APP_LIST_MODEL_EXPORT AppListItemObserver {
 public:
  // Invoked after item's icon is changed.
  // |config_type| The app list configuration type for which the item icon
  // changed.
  virtual void ItemIconChanged(ash::AppListConfigType config_type) {}

  // Invoked after item's name is changed.
  virtual void ItemNameChanged() {}

  // Invoked after item begins or finishes installing.
  virtual void ItemIsInstallingChanged() {}

  // Invoked after item's download percentage changes.
  virtual void ItemPercentDownloadedChanged() {}

  // Invoked when the item is about to be destroyed.
  virtual void ItemBeingDestroyed() {}

 protected:
  virtual ~AppListItemObserver() {}
};

}  // namespace ash

#endif  // ASH_APP_LIST_MODEL_APP_LIST_ITEM_OBSERVER_H_
