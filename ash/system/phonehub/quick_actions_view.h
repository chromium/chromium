// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_QUICK_ACTIONS_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_QUICK_ACTIONS_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class QuickActionControllerBase;
class QuickActionItem;

// A view in Phone Hub bubble that contains toggle button for quick actions such
// as enable hotspot, silence phone and locate phone.
class ASH_EXPORT QuickActionsView : public views::View {
  METADATA_HEADER(QuickActionsView, views::View)

 public:
  explicit QuickActionsView(phonehub::PhoneHubManager* phone_hub_manager);
  ~QuickActionsView() override;
  QuickActionsView(QuickActionsView&) = delete;
  QuickActionsView operator=(QuickActionsView&) = delete;

  // Return enable_hotspot to start tethering from Phone
  // Hub error dialog.
  QuickActionItem* GetEnableHotspotQuickActionItem() { return enable_hotspot_; }

  // Used for testing only
  QuickActionItem* silence_phone_for_testing() { return silence_phone_; }
  QuickActionItem* locate_phone_for_testing() { return locate_phone_; }

  void OnThemeChanged() override;

 private:
  // Add all the quick actions items to the view.
  void InitQuickActionItems();

  // Controllers of quick actions items. Owned by this.
  std::vector<std::unique_ptr<QuickActionControllerBase>>
      quick_action_controllers_;

  raw_ptr<phonehub::PhoneHubManager> phone_hub_manager_ = nullptr;

  // QuickActionItem for unit testing. Owned by this view.
  raw_ptr<QuickActionItem> enable_hotspot_ = nullptr;
  raw_ptr<QuickActionItem> silence_phone_ = nullptr;
  raw_ptr<QuickActionItem> locate_phone_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_QUICK_ACTIONS_VIEW_H_
