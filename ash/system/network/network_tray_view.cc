// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_tray_view.h"

#include <utility>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/active_network_icon.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/network_icon_animation.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"

namespace ash {

NetworkTrayView::NetworkTrayView(Shelf* shelf, ActiveNetworkIcon::Type type)
    : TrayItemView(shelf), type_(type) {
  Shell::Get()->system_tray_model()->network_state_model()->AddObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);
  CreateImageView();

  GetViewAccessibility().SetRole(ax::mojom::Role::kImage);

  UpdateConnectionStatus(true /* notify_a11y */);
}

NetworkTrayView::~NetworkTrayView() {
  network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
  Shell::Get()->system_tray_model()->network_state_model()->RemoveObserver(
      this);
  Shell::Get()->session_controller()->RemoveObserver(this);
}

std::u16string NetworkTrayView::GetAccessibleNameString() const {
  return tooltip_;
}

views::View* NetworkTrayView::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  return GetLocalBounds().Contains(point) ? this : nullptr;
}

std::u16string NetworkTrayView::GetTooltipText(const gfx::Point& p) const {
  return tooltip_;
}

void NetworkTrayView::HandleLocaleChange() {
  UpdateConnectionStatus(false /* notify_a11y */);
}

void NetworkTrayView::OnThemeChanged() {
  TrayItemView::OnThemeChanged();
  UpdateNetworkStateHandlerIcon();
}

void NetworkTrayView::UpdateLabelOrImageViewColor(bool active) {
  TrayItemView::UpdateLabelOrImageViewColor(active);

  UpdateNetworkStateHandlerIcon();
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
          GetColorProvider(), type_, GetIconType(), &animating);
  bool show_in_tray = !image.isNull();
  UpdateIcon(show_in_tray, image);
  if (animating) {
    network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
  } else {
    network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
  }
}

void NetworkTrayView::UpdateConnectionStatus(bool notify_a11y) {
  std::u16string prev_accessible_name = GetViewAccessibility().GetCachedName();
  std::u16string accessible_name;
  Shell::Get()
      ->system_tray_model()
      ->active_network_icon()
      ->GetConnectionStatusStrings(type_, &accessible_name,
                                   &accessible_description_, &tooltip_);
  GetViewAccessibility().SetName(accessible_name);
  if (notify_a11y && !accessible_name.empty() &&
      accessible_name != prev_accessible_name) {
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  }

  if (!accessible_description_.empty()) {
    GetViewAccessibility().SetDescription(accessible_description_);
  } else {
    GetViewAccessibility().RemoveDescription();
  }
}

network_icon::IconType NetworkTrayView::GetIconType() {
  // OOBE has a white background that makes regular tray icons not visible.
  if (Shell::Get()->session_controller()->GetSessionState() ==
      session_manager::SessionState::OOBE) {
    return network_icon::ICON_TYPE_TRAY_OOBE;
  }
  // Active tray has a different icon color.
  if (is_active()) {
    return network_icon::ICON_TYPE_TRAY_ACTIVE;
  }
  return network_icon::ICON_TYPE_TRAY_REGULAR;
}

BEGIN_METADATA(NetworkTrayView)
END_METADATA

}  // namespace ash
