// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_connected_view.h"

#include <memory>

#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/notification_opt_in_view.h"
#include "ash/system/phonehub/phone_status_view.h"
#include "ash/system/phonehub/quick_actions_view.h"
#include "ash/system/phonehub/task_continuation_view.h"
#include "ash/system/tray/tray_constants.h"
#include "chromeos/components/phonehub/notification_access_manager.h"
#include "chromeos/components/phonehub/phone_hub_manager.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {
constexpr int kPaddingBetweenTitleAndSeparator = 3;
}  // namespace

PhoneConnectedView::PhoneConnectedView(
    TrayBubbleView* bubble_view,
    chromeos::phonehub::PhoneHubManager* phone_hub_manager) {
  auto setup_layered_view = [](views::View* view) {
    view->SetPaintToLayer();
    view->layer()->SetFillsBoundsOpaquely(false);
  };

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(0, 0, 0, 0)));
  layout->SetDefaultFlex(1);

  AddSeparator();

  // TODO(meilinw): handle the case when the user has dismissed this opt in
  // view once, we shouldn't show it again.
  if (!phone_hub_manager->GetNotificationAccessManager()
           ->HasAccessBeenGranted()) {
    AddChildView(std::make_unique<NotificationOptInView>(bubble_view));
  }

  setup_layered_view(
      AddChildView(std::make_unique<QuickActionsView>(phone_hub_manager)));

  AddSeparator();

  auto* phone_model = phone_hub_manager->GetPhoneModel();
  if (phone_model) {
    setup_layered_view(
        AddChildView(std::make_unique<TaskContinuationView>(phone_model)));
  }
}

PhoneConnectedView::~PhoneConnectedView() = default;

void PhoneConnectedView::ChildPreferredSizeChanged(View* child) {
  // Resize the bubble when the child change its size.
  PreferredSizeChanged();
}

void PhoneConnectedView::ChildVisibilityChanged(View* child) {
  // Resize the bubble when the child change its visibility.
  PreferredSizeChanged();
}

const char* PhoneConnectedView::GetClassName() const {
  return "PhoneConnectedView";
}

void PhoneConnectedView::AddSeparator() {
  auto* separator = AddChildView(std::make_unique<views::Separator>());
  separator->SetPaintToLayer();
  separator->layer()->SetFillsBoundsOpaquely(false);
  separator->SetColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSeparatorColor));
  separator->SetBorder(views::CreateEmptyBorder(gfx::Insets(
      kPaddingBetweenTitleAndSeparator, 0, kMenuSeparatorVerticalPadding, 0)));
}

}  // namespace ash
