// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_connected_view.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/camera_roll_view.h"
#include "ash/system/phonehub/multidevice_feature_opt_in_view.h"
#include "ash/system/phonehub/phone_hub_recent_apps_view.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/phonehub/phone_status_view.h"
#include "ash/system/phonehub/quick_actions_view.h"
#include "ash/system/phonehub/task_continuation_view.h"
#include "ash/system/phonehub/ui_constants.h"
#include "ash/system/tray/tray_constants.h"
#include "chromeos/ash/components/phonehub/multidevice_feature_access_manager.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/ping_manager.h"
#include "chromeos/ash/components/phonehub/user_action_recorder.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr auto kDarkLightModeEnabledPadding =
    gfx::Insets::TLBR(0,
                      kBubbleHorizontalSidePaddingDip,
                      16,
                      kBubbleHorizontalSidePaddingDip);

constexpr auto kDarkLightModeDisabledPadding =
    gfx::Insets::VH(0, kBubbleHorizontalSidePaddingDip);

}  // namespace

PhoneConnectedView::PhoneConnectedView(
    phonehub::PhoneHubManager* phone_hub_manager) {
  SetID(PhoneHubViewID::kPhoneConnectedView);

  auto setup_layered_view = [](views::View* view) {
    // In dark light mode, we switch TrayBubbleView to use a textured layer
    // instead of solid color layer, so no need to create an extra layer here.
    if (features::IsDarkLightModeEnabled())
      return;
    view->SetPaintToLayer();
    view->layer()->SetFillsBoundsOpaquely(false);
  };

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      features::IsDarkLightModeEnabled() ? kDarkLightModeEnabledPadding
                                         : kDarkLightModeDisabledPadding));

  layout->SetDefaultFlex(1);

  AddChildView(std::make_unique<MultideviceFeatureOptInView>(
      phone_hub_manager->GetMultideviceFeatureAccessManager()));

  setup_layered_view(
      AddChildView(std::make_unique<QuickActionsView>(phone_hub_manager)));

  auto* phone_model = phone_hub_manager->GetPhoneModel();
  if (phone_model) {
    setup_layered_view(AddChildView(std::make_unique<TaskContinuationView>(
        phone_model, phone_hub_manager->GetUserActionRecorder())));
  }

  auto* camera_roll_manager = phone_hub_manager->GetCameraRollManager();
  if (features::IsPhoneHubCameraRollEnabled() && camera_roll_manager) {
    setup_layered_view(AddChildView(std::make_unique<CameraRollView>(
        camera_roll_manager, phone_hub_manager->GetUserActionRecorder())));
  }

  auto* recent_apps_handler =
      phone_hub_manager->GetRecentAppsInteractionHandler();
  if (features::IsEcheSWAEnabled() && recent_apps_handler) {
    setup_layered_view(AddChildView(std::make_unique<PhoneHubRecentAppsView>(
        recent_apps_handler, phone_hub_manager)));
  }

  auto* ping_manager = phone_hub_manager->GetPingManager();
  if (features::IsPhoneHubPingOnBubbleOpenEnabled() && ping_manager) {
    ping_manager->SendPingRequest();
  }

  phone_hub_manager->GetUserActionRecorder()->RecordUiOpened();
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
