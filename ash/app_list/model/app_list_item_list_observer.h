// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_APP_LIST_ITEM_LIST_OBSERVER_H_
#define ASH_APP_LIST_MODEL_APP_LIST_ITEM_LIST_OBSERVER_H_

#include <stddef.h>

#include "ash/app_list/model/app_list_model_export.h"

namespace ash {

class AppListItem;

class APP_LIST_MODEL_EXPORT AppListItemListObserver {
 public:
  // Triggered after |item| has been added to the list at |index|.
  virtual void OnListItemAdded(size_t index, AppListItem* item) {}

  // Triggered after an item has been removed from the list at |index|, just
  // before the item is deleted.
  virtual void OnListItemRemoved(size_t index, AppListItem* item) {}

  // Triggered after |item| has been moved from |from_index| to |to_index|.
  // Note: |from_index| may equal |to_index| if only the ordinal has changed.
  virtual void OnListItemMoved(size_t from_index,
                               size_t to_index,
                               AppListItem* item) {}

  // Triggered after the item at the corresponding index in the top level has
  // started or completed installing and should be highlighted.
  virtual void OnAppListItemHighlight(size_t index, bool highlight) {}

 protected:
  virtual ~AppListItemListObserver() {}
};

}  // namespace ash

#endif  // ASH_APP_LIST_MODEL_APP_LIST_ITEM_LIST_OBSERVER_H_
