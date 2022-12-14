// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_LIST_MODEL_UPDATER_OBSERVER_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_LIST_MODEL_UPDATER_OBSERVER_H_

#include "base/observer_list_types.h"

class ChromeAppListItem;

namespace ash {
enum class AppListSortOrder;
}

// An observer interface for AppListModelUpdater to perform additional work on
// ChromeAppListItem changes.
class AppListModelUpdaterObserver : public base::CheckedObserver {
 public:
  // Triggered after an item has been added to the model.
  virtual void OnAppListItemAdded(ChromeAppListItem* item) {}

  // Triggered just before an item is deleted from the model.
  virtual void OnAppListItemWillBeDeleted(ChromeAppListItem* item) {}

  // Triggered after an item has moved, changed folders, or changed properties.
  virtual void OnAppListItemUpdated(ChromeAppListItem* item) {}
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_LIST_MODEL_UPDATER_OBSERVER_H_
