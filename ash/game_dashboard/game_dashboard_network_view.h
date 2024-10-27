// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_NETWORK_VIEW_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_NETWORK_VIEW_H_

#include "ash/system/network/network_icon_animation_observer.h"
#include "ash/system/network/tray_network_state_observer.h"
#include "ui/views/controls/image_view.h"

namespace ash {
class ASH_EXPORT GameDashboardNetworkView
    : public network_icon::AnimationObserver,
      public TrayNetworkStateObserver,
      public views::ImageView {
  METADATA_HEADER(GameDashboardNetworkView, views::ImageView)

 public:
  GameDashboardNetworkView();
  GameDashboardNetworkView(const GameDashboardNetworkView&) = delete;
  GameDashboardNetworkView& operator=(const GameDashboardNetworkView) = delete;
  ~GameDashboardNetworkView() override;

  // views::View:
  void OnThemeChanged() override;

  // network_icon::AnimationObserver:
  void NetworkIconChanged() override;

  // TrayNetworkStateObserver:
  void ActiveNetworkStateChanged() override;

 private:
  // Update the Network State icon depending on color, animation, and type.
  void UpdateNetworkStateHandlerIcon();

  // Updates the tooltip and calls NotifyAccessibilityEvent, if `notify_ally` is
  // true.
  void UpdateConnectionStatus(bool notify_a11y);
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_NETWORK_VIEW_H_
