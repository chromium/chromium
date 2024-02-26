// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_TRAVERSABLE_ITEM_CONTAINER_H_
#define ASH_PICKER_VIEWS_PICKER_TRAVERSABLE_ITEM_CONTAINER_H_

#include "ash/ash_export.h"

namespace ash {

class PickerItemView;

// Interface implemented by Picker item containers that support item traversal.
class ASH_EXPORT PickerTraversableItemContainer {
 public:
  virtual ~PickerTraversableItemContainer() = default;

  // Returns the item to highlight to when navigating to this container from the
  // top, or nullptr if the container is empty.
  virtual PickerItemView* GetTopItem() = 0;

  // Returns the item to highlight to when navigating to this container from the
  // bottom, or nullptr if the container is empty.
  virtual PickerItemView* GetBottomItem() = 0;

  // Returns the item directly above `item`, or nullptr if there is no such item
  // in the container.
  virtual PickerItemView* GetItemAbove(PickerItemView* item) = 0;

  // Returns the item directly below `item`, or nullptr if there is no such item
  // in the container.
  virtual PickerItemView* GetItemBelow(PickerItemView* item) = 0;

  // Returns the item directly to the left of `item`, or nullptr if there is no
  // such item in the container.
  virtual PickerItemView* GetItemLeftOf(PickerItemView* item) = 0;

  // Returns the item directly to the right of `item`, or nullptr if there is no
  // such item in the container.
  virtual PickerItemView* GetItemRightOf(PickerItemView* item) = 0;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_TRAVERSABLE_ITEM_CONTAINER_H_
