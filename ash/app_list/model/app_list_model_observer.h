// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_APP_LIST_MODEL_OBSERVER_H_
#define ASH_APP_LIST_MODEL_APP_LIST_MODEL_OBSERVER_H_

#include "ash/app_list/model/app_list_model_export.h"
#include "base/observer_list_types.h"

namespace ash {

class AppListItem;
enum class AppListState;

class APP_LIST_MODEL_EXPORT AppListModelObserver
    : public base::CheckedObserver {
 public:
  // Triggered after AppListModel's status has changed.
  virtual void OnAppListModelStatusChanged() {}

  // Triggered after |item| has been added to the model.
  virtual void OnAppListItemAdded(AppListItem* item) {}

  // Triggered just before an item is deleted from the model.
  virtual void OnAppListItemWillBeDeleted(AppListItem* item) {}

  // Triggered after |item| has moved, changed folders, or changed properties.
  virtual void OnAppListItemUpdated(AppListItem* item) {}

  // Triggered when the custom launcher page enabled state is changed.
  virtual void OnCustomLauncherPageEnabledStateChanged(bool enabled) {}

 protected:
  ~AppListModelObserver() override;
};

}  // namespace ash

#endif  // ASH_APP_LIST_MODEL_APP_LIST_MODEL_OBSERVER_H_
