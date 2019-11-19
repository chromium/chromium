// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_tray_view.h"

#include <utility>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/network_icon_animation.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/controls/image_view.h"

namespace ash {
namespace tray {

namespace {

// OOBE has a white background that makes regular tray icons not visible.
network_icon::IconType GetIconType() {
  if (Shell::Get()->session_controller()->GetSessionState() ==
      session_manager::SessionState::OOBE) {
    return network_icon::ICON_TYPE_TRAY_OOBE;
  }
  return network_icon::ICON_TYPE_TRAY_REGULAR;
}

}  // namespace

NetworkTrayView::NetworkTrayView(Shelf* shelf, ActiveNetworkIcon::Type type)
    : TrayItemView(shelf), type_(type) {
  Shell::Get()->system_tray_model()->network_state_model()->AddObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);
  CreateImageView();
  UpdateNetworkStateHandlerIcon();
  UpdateConnectionStatus(true /* notify_a11y */);
}

NetworkTrayView::~NetworkTrayView() {
  network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
  Shell::Get()->system_tray_model()->network_state_model()->RemoveObserver(
      this);
  Shell::Get()->session_controller()->RemoveObserver(this);
}

const char* NetworkTrayView::GetClassName() const {
  return "NetworkTrayView";
}

void NetworkTrayView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->SetName(accessible_name_);
  node_data->SetDescription(accessible_description_);
  node_data->role = ax::mojom::Role::kButton;
}

views::View* NetworkTrayView::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  return GetLocalBounds().Contains(point) ? this : nullptr;
}

base::string16 NetworkTrayView::GetTooltipText(const gfx::Point& p) const {
  return tooltip_;
}

void NetworkTrayView::NetworkIconChanged() {
  UpdateNetworkStateHandlerIcon();
  UpdateConnectionStatus(false /* notify_a11y */);
}

void NetworkTrayView::OnSessionStateChanged(
    session_manager::SessionState state) {
  UpdateNetworkStateHandlerIcon();
}

void NetworkTrayView::ActiveNetworkStateChanged() {
  UpdateNetworkStateHandlerIcon();
  UpdateConnectionStatus(true /* notify _a11y */);
}

void NetworkTrayView::NetworkListChanged() {
  UpdateNetworkStateHandlerIcon();
}

void NetworkTrayView::UpdateIcon(bool tray_icon_visible,
                                 const gfx::ImageSkia& image) {
  image_view()->SetImage(image);
  SetVisible(tray_icon_visible);
  SchedulePaint();
}

void NetworkTrayView::UpdateNetworkStateHandlerIcon() {
  bool animating = false;
  gfx::ImageSkia image =
      Shell::Get()->system_tray_model()->active_network_icon()->GetImage(
          type_, GetIconType(), &animating);
  bool show_in_tray = !image.isNull();
  UpdateIcon(show_in_tray, image);
  if (animating)
    network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
  else
    network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
}

void NetworkTrayView::UpdateConnectionStatus(bool notify_a11y) {
  base::string16 prev_accessible_name = accessible_name_;
  Shell::Get()
      ->system_tray_model()
      ->active_network_icon()
      ->GetConnectionStatusStrings(type_, &accessible_name_,
                                   &accessible_description_, &tooltip_);
  if (notify_a11y && !accessible_name_.empty() &&
      accessible_name_ != prev_accessible_name) {
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  }
}

}  // namespace tray
}  // namespace ash
