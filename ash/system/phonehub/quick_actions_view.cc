// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/quick_actions_view.h"

#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/phonehub/quick_action_item.h"
#include "ash/system/phonehub/silence_phone_quick_action_controller.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr gfx::Insets kQuickActionsViewPadding(16, 4);
constexpr int kQuickActionsItemSpacing = 36;

}  // namespace

QuickActionsView::QuickActionsView(
    chromeos::phonehub::PhoneHubManager* phone_hub_manager)
    : phone_hub_manager_(phone_hub_manager) {
  SetID(PhoneHubViewID::kQuickActionsView);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kQuickActionsViewPadding,
      kQuickActionsItemSpacing));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  InitQuickActionItems();
}

QuickActionsView::~QuickActionsView() = default;

void QuickActionsView::InitQuickActionItems() {
  silence_phone_ = AddItem(std::make_unique<SilencePhoneQuickActionController>(
      phone_hub_manager_->GetDoNotDisturbController()));
}

QuickActionItem* QuickActionsView::AddItem(
    std::unique_ptr<QuickActionControllerBase> controller) {
  auto* item = AddChildView(controller->CreateItem());
  quick_action_controllers_.push_back(std::move(controller));
  return item;
}

}  // namespace ash
