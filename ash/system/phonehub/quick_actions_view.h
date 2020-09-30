// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_QUICK_ACTIONS_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_QUICK_ACTIONS_VIEW_H_

#include "ash/ash_export.h"
#include "chromeos/components/phonehub/phone_hub_manager.h"
#include "ui/views/view.h"

namespace ash {

class QuickActionControllerBase;
class QuickActionItem;

// A view in Phone Hub bubble that contains toggle button for quick actions such
// as enable hotspot, silence phone and locate phone.
class ASH_EXPORT QuickActionsView : public views::View {
 public:
  explicit QuickActionsView(
      chromeos::phonehub::PhoneHubManager* phone_hub_manager);
  ~QuickActionsView() override;
  QuickActionsView(QuickActionsView&) = delete;
  QuickActionsView operator=(QuickActionsView&) = delete;

 private:
  FRIEND_TEST_ALL_PREFIXES(QuickActionsViewTest, QuickActionsToggle);

  // Add all the quick actions items to the view.
  void InitQuickActionItems();

  // Helper function to add an item to the view given its controller.
  QuickActionItem* AddItem(
      std::unique_ptr<QuickActionControllerBase> controller);

  // Controllers of quick actions items. Owned by this.
  std::vector<std::unique_ptr<QuickActionControllerBase>>
      quick_action_controllers_;

  chromeos::phonehub::PhoneHubManager* phone_hub_manager_ = nullptr;

  // QuickActionItem for unit testing. Owned by this view.
  QuickActionItem* silence_phone_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_QUICK_ACTIONS_VIEW_H_
