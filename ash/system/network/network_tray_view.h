// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_TRAY_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_TRAY_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/network/active_network_icon.h"
#include "ash/system/network/network_icon_animation_observer.h"
#include "ash/system/network/tray_network_state_observer.h"
#include "ash/system/tray/tray_item_view.h"

namespace ash {

// View class containing an ImageView for a network icon in the tray.
// The ActiveNetworkIcon::Type parameter determines what type of icon is
// displayed. Generation and update of the icon is handled by ActiveNetworkIcon.
class ASH_EXPORT NetworkTrayView : public TrayItemView,
                                   public network_icon::AnimationObserver,
                                   public SessionObserver,
                                   public TrayNetworkStateObserver {
  METADATA_HEADER(NetworkTrayView, TrayItemView)

 public:
  NetworkTrayView(const NetworkTrayView&) = delete;
  NetworkTrayView& operator=(const NetworkTrayView&) = delete;

  ~NetworkTrayView() override;

  NetworkTrayView(Shelf* shelf, ActiveNetworkIcon::Type type);

  std::u16string GetAccessibleNameString() const;

  // views::View:
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;

  // TrayItemView:
  void HandleLocaleChange() override;
  void OnThemeChanged() override;
  void UpdateLabelOrImageViewColor(bool active) override;

  // network_icon::AnimationObserver:
  void NetworkIconChanged() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // TrayNetworkStateObserver:
  void ActiveNetworkStateChanged() override;
  void NetworkListChanged() override;

 private:
  friend class NetworkTrayViewTest;

  void UpdateIcon(bool tray_icon_visible, const gfx::ImageSkia& image);

  void UpdateNetworkStateHandlerIcon();

  // Updates the tooltip and calls NotifyAccessibilityEvent when necessary.
  void UpdateConnectionStatus(bool notify_a11y);

  // Gets the icon type to paint different icons for different states.
  network_icon::IconType GetIconType();

  ActiveNetworkIcon::Type type_;

  // The description provided by GetAccessibleNodeData. For wifi networks this
  // is the signal strength of the network. Otherwise it is empty.
  std::u16string accessible_description_;

  // The tooltip for the icon. Includes the network name and signal strength
  // (for wireless networks).
  std::u16string tooltip_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_TRAY_VIEW_H_
