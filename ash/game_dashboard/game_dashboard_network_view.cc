// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_network_view.h"

#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/active_network_icon.h"
#include "ash/system/network/network_icon_animation.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

GameDashboardNetworkView::GameDashboardNetworkView() {
  Shell::Get()->system_tray_model()->network_state_model()->AddObserver(this);
  UpdateConnectionStatus(/*notify_a11y=*/true);
}

GameDashboardNetworkView::~GameDashboardNetworkView() {
  network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
  Shell::Get()->system_tray_model()->network_state_model()->RemoveObserver(
      this);
}

void GameDashboardNetworkView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateNetworkStateHandlerIcon();
}

void GameDashboardNetworkView::NetworkIconChanged() {
  UpdateConnectionStatus(/*notify_a11y=*/false);
  UpdateNetworkStateHandlerIcon();
}

void GameDashboardNetworkView::ActiveNetworkStateChanged() {
  UpdateConnectionStatus(/*notify _a11y=*/true);
  UpdateNetworkStateHandlerIcon();
}

void GameDashboardNetworkView::UpdateNetworkStateHandlerIcon() {
  bool animating = false;
  gfx::ImageSkia image =
      Shell::Get()->system_tray_model()->active_network_icon()->GetImage(
          GetColorProvider(), ActiveNetworkIcon::Type::kSingle,
          network_icon::ICON_TYPE_TRAY_REGULAR, &animating);

  const bool image_exists = !image.isNull();
  if (image_exists) {
    SetImage(image);
  }
  SetVisible(image_exists);

  if (animating) {
    network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
  } else {
    network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
  }
}

void GameDashboardNetworkView::UpdateConnectionStatus(bool notify_a11y) {
  std::u16string accessible_description;
  std::u16string accessible_name;
  std::u16string tooltip;

  Shell::Get()
      ->system_tray_model()
      ->active_network_icon()
      ->GetConnectionStatusStrings(ActiveNetworkIcon::Type::kSingle,
                                   &accessible_name, &accessible_description,
                                   &tooltip);
  SetTooltipText(tooltip);
  if (notify_a11y && !accessible_name.empty() &&
      accessible_name != GetViewAccessibility().GetCachedName()) {
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  }

  if (accessible_description.empty()) {
    GetViewAccessibility().RemoveDescription();
  } else {
    GetViewAccessibility().SetDescription(accessible_description);
  }
}

BEGIN_METADATA(GameDashboardNetworkView);
END_METADATA

}  // namespace ash
