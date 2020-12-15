// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_connected_view.h"

#include <memory>

#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/notification_opt_in_view.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/phonehub/phone_status_view.h"
#include "ash/system/phonehub/quick_actions_view.h"
#include "ash/system/phonehub/task_continuation_view.h"
#include "ash/system/phonehub/ui_constants.h"
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

PhoneConnectedView::PhoneConnectedView(
    chromeos::phonehub::PhoneHubManager* phone_hub_manager) {
  SetID(PhoneHubViewID::kPhoneConnectedView);

  auto setup_layered_view = [](views::View* view) {
    view->SetPaintToLayer();
    view->layer()->SetFillsBoundsOpaquely(false);
  };

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(0, kBubbleHorizontalSidePaddingDip)));
  layout->SetDefaultFlex(1);

  AddChildView(std::make_unique<NotificationOptInView>(
      phone_hub_manager->GetNotificationAccessManager()));

  setup_layered_view(
      AddChildView(std::make_unique<QuickActionsView>(phone_hub_manager)));

  auto* phone_model = phone_hub_manager->GetPhoneModel();
  if (phone_model) {
    setup_layered_view(AddChildView(std::make_unique<TaskContinuationView>(
        phone_model, phone_hub_manager->GetUserActionRecorder())));
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

phone_hub_metrics::Screen PhoneConnectedView::GetScreenForMetrics() const {
  return phone_hub_metrics::Screen::kPhoneConnected;
}

}  // namespace ash
