// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_QUICK_ACTION_CONTROLLER_BASE_H_
#define ASH_SYSTEM_PHONEHUB_QUICK_ACTION_CONTROLLER_BASE_H_

#include "ash/ash_export.h"
#include "ash/system/phonehub/quick_action_item.h"

namespace ash {

class QuickActionItem;

// Base class for controllers of quick action item.
// To add a new quick action item, implement this class, and add to the list in
// QuickActionsView::InitQuickActionItems().
class ASH_EXPORT QuickActionControllerBase : public QuickActionItem::Delegate {
 public:
  virtual ~QuickActionControllerBase() = default;

  // Create the view. Subclasses instantiate QuickActionItem.
  // The view will be owned by views hierarchy. The view will be always deleted
  // after the controller is destructed.
  virtual QuickActionItem* CreateItem() = 0;

  // Used to update the QuickActionItem UI properties.
  virtual void UpdateQuickActionItemUi() = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_QUICK_ACTION_CONTROLLER_BASE_H_
