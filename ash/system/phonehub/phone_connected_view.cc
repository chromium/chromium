// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_connected_view.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/app_stream_connection_error_dialog.h"
#include "ash/system/phonehub/camera_roll_view.h"
#include "ash/system/phonehub/multidevice_feature_opt_in_view.h"
#include "ash/system/phonehub/phone_hub_recent_apps_view.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/phonehub/phone_status_view.h"
#include "ash/system/phonehub/quick_action_item.h"
#include "ash/system/phonehub/quick_actions_view.h"
#include "ash/system/phonehub/task_continuation_view.h"
#include "ash/system/phonehub/ui_constants.h"
#include "ash/system/tray/tray_constants.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/multidevice_feature_access_manager.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/ping_manager.h"
#include "chromeos/ash/components/phonehub/user_action_recorder.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace {

constexpr auto kDarkLightModeEnabledPadding =
    gfx::Insets::TLBR(0,
                      kBubbleHorizontalSidePaddingDip,
                      16,
                      kBubbleHorizontalSidePaddingDip);

}  // namespace

PhoneConnectedView::PhoneConnectedView(
    phonehub::PhoneHubManager* phone_hub_manager)
    : phone_hub_manager_(phone_hub_manager) {
  SetID(PhoneHubViewID::kPhoneConnectedView);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kDarkLightModeEnabledPadding));

  layout->SetDefaultFlex(1);

  AddChildView(std::make_unique<MultideviceFeatureOptInView>(
      phone_hub_manager->GetMultideviceFeatureAccessManager()));

  quick_actions_view_ =
      AddChildView(std::make_unique<QuickActionsView>(phone_hub_manager));

  auto* phone_model = phone_hub_manager->GetPhoneModel();
  if (phone_model) {
    AddChildView(std::make_unique<TaskContinuationView>(
        phone_model, phone_hub_manager->GetUserActionRecorder()));
  }

  auto* camera_roll_manager = phone_hub_manager->GetCameraRollManager();
  if (features::IsPhoneHubCameraRollEnabled() && camera_roll_manager) {
    AddChildView(std::make_unique<CameraRollView>(
        camera_roll_manager, phone_hub_manager->GetUserActionRecorder()));
  }

  auto* recent_apps_handler =
      phone_hub_manager->GetRecentAppsInteractionHandler();
  if (features::IsEcheSWAEnabled() && recent_apps_handler) {
    AddChildView(std::make_unique<PhoneHubRecentAppsView>(
        recent_apps_handler, phone_hub_manager, this));
  }

  auto* ping_manager = phone_hub_manager->GetPingManager();
  if (features::IsPhoneHubPingOnBubbleOpenEnabled() && ping_manager) {
    ping_manager->SendPingRequest();
  }

  phone_hub_manager->GetUserActionRecorder()->RecordUiOpened();

  if (phone_hub_manager->GetAppStreamLauncherDataModel()) {
    phone_hub_manager->GetAppStreamLauncherDataModel()->SetLauncherSize(
        GetPreferredSize().height(), GetPreferredSize().width());
  }
}

PhoneConnectedView::~PhoneConnectedView() = default;

void PhoneConnectedView::ChildPreferredSizeChanged(View* child) {
  // Resize the bubble when the child change its size.
  PreferredSizeChanged();
  if (phone_hub_manager_ &&
      phone_hub_manager_->GetAppStreamLauncherDataModel()) {
    phone_hub_manager_->GetAppStreamLauncherDataModel()->SetLauncherSize(
        GetPreferredSize().height(), GetPreferredSize().width());
  }
}

void PhoneConnectedView::ChildVisibilityChanged(View* child) {
  // Resize the bubble when the child change its visibility.
  PreferredSizeChanged();
  if (phone_hub_manager_ &&
      phone_hub_manager_->GetAppStreamLauncherDataModel()) {
    phone_hub_manager_->GetAppStreamLauncherDataModel()->SetLauncherSize(
        GetPreferredSize().height(), GetPreferredSize().width());
  }
}

phone_hub_metrics::Screen PhoneConnectedView::GetScreenForMetrics() const {
  return phone_hub_metrics::Screen::kPhoneConnected;
}

void PhoneConnectedView::ShowAppStreamErrorDialog(bool is_different_network,
                                                  bool is_phone_on_cellular) {
  if (!features::IsEcheNetworkConnectionStateEnabled()) {
    return;
  }
  app_stream_error_dialog_ = std::make_unique<AppStreamConnectionErrorDialog>(
      this,
      base::BindOnce(&PhoneConnectedView::OnAppStreamErrorDialogClosed,
                     base::Unretained(this)),
      base::BindOnce(&PhoneConnectedView::OnAppStreamErrorDialogButtonClicked,
                     base::Unretained(this)),
      is_different_network, is_phone_on_cellular);
  app_stream_error_dialog_->UpdateBounds();
  app_stream_error_dialog_->widget()->Show();
}

void PhoneConnectedView::OnAppStreamErrorDialogClosed() {
  app_stream_error_dialog_.reset();
}

void PhoneConnectedView::OnAppStreamErrorDialogButtonClicked(
    const ui::Event& event) {
  auto* enable_hotspot = quick_actions_view_->GetEnableHotspotQuickActionItem();
  if (enable_hotspot->IsToggled()) {
    return;
  }
  enable_hotspot->icon_button()->NotifyClick(event);
}

BEGIN_METADATA(PhoneConnectedView)
END_METADATA

}  // namespace ash
