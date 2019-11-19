// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_TRAY_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_TRAY_VIEW_H_

#include <memory>

#include "ash/session/session_observer.h"
#include "ash/system/network/active_network_icon.h"
#include "ash/system/network/network_icon_animation_observer.h"
#include "ash/system/network/tray_network_state_observer.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/macros.h"

namespace ash {
namespace tray {

// View class containing an ImageView for a network icon in the tray.
// The ActiveNetworkIcon::Type parameter determines what type of icon is
// displayed. Generation and update of the icon is handled by ActiveNetworkIcon.
class NetworkTrayView : public TrayItemView,
                        public network_icon::AnimationObserver,
                        public SessionObserver,
                        public TrayNetworkStateObserver {
 public:
  ~NetworkTrayView() override;

  NetworkTrayView(Shelf* shelf, ActiveNetworkIcon::Type type);

  const char* GetClassName() const override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  base::string16 GetTooltipText(const gfx::Point& p) const override;

  // network_icon::AnimationObserver:
  void NetworkIconChanged() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // TrayNetworkStateObserver:
  void ActiveNetworkStateChanged() override;
  void NetworkListChanged() override;

 private:
  void UpdateIcon(bool tray_icon_visible, const gfx::ImageSkia& image);

  void UpdateNetworkStateHandlerIcon();

  // Updates the tooltip and calls NotifyAccessibilityEvent when necessary.
  void UpdateConnectionStatus(bool notify_a11y);

  ActiveNetworkIcon::Type type_;

  // The name provided by GetAccessibleNodeData, which includes the network
  // name and connection state.
  base::string16 accessible_name_;

  // The description provided by GetAccessibleNodeData. For wifi networks this
  // is the signal strength of the network. Otherwise it is empty.
  base::string16 accessible_description_;

  // The tooltip for the icon. Includes the network name and signal strength
  // (for wireless networks).
  base::string16 tooltip_;

  DISALLOW_COPY_AND_ASSIGN(NetworkTrayView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_TRAY_VIEW_H_
