// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/quick_actions_view.h"

#include "ash/system/phonehub/enable_hotspot_quick_action_controller.h"
#include "ash/system/phonehub/locate_phone_quick_action_controller.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/phonehub/quick_action_item.h"
#include "ash/system/phonehub/silence_phone_quick_action_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr auto kQuickActionsViewPadding = gfx::Insets::TLBR(16, 4, 12, 4);

}  // namespace

QuickActionsView::QuickActionsView(phonehub::PhoneHubManager* phone_hub_manager)
    : phone_hub_manager_(phone_hub_manager) {
  SetID(PhoneHubViewID::kQuickActionsView);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kQuickActionsViewPadding));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  InitQuickActionItems();
}

QuickActionsView::~QuickActionsView() = default;

void QuickActionsView::InitQuickActionItems() {
  auto enable_hotspot_controller =
      std::make_unique<EnableHotspotQuickActionController>(
          phone_hub_manager_->GetTetherController());
  enable_hotspot_ = AddChildView(enable_hotspot_controller->CreateItem());
  quick_action_controllers_.push_back(std::move(enable_hotspot_controller));

  auto silence_phone_controller =
      std::make_unique<SilencePhoneQuickActionController>(
          phone_hub_manager_->GetDoNotDisturbController());
  silence_phone_ = AddChildView(silence_phone_controller->CreateItem());

  auto locate_phone_controller =
      std::make_unique<LocatePhoneQuickActionController>(
          phone_hub_manager_->GetFindMyDeviceController());
  locate_phone_ = AddChildView(locate_phone_controller->CreateItem());

  quick_action_controllers_.push_back(std::move(silence_phone_controller));
  quick_action_controllers_.push_back(std::move(locate_phone_controller));
}

void QuickActionsView::OnThemeChanged() {
  views::View::OnThemeChanged();
  for (auto& controller : quick_action_controllers_) {
    controller->UpdateQuickActionItemUi();
  }
}

BEGIN_METADATA(QuickActionsView)
END_METADATA

}  // namespace ash
