// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_WIFI_HEADER_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_LIST_WIFI_HEADER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/style/icon_button.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ash/system/tray/tri_view.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/view.h"

namespace ash {

// This class is the implementation of the network list header for Wifi
// networks, and is responsible for the creation of wifi-specific buttons.
class ASH_EXPORT NetworkListWifiHeaderView
    : public NetworkListNetworkHeaderView {
 public:
  explicit NetworkListWifiHeaderView(
      NetworkListNetworkHeaderView::Delegate* delegate);
  NetworkListWifiHeaderView(const NetworkListWifiHeaderView&) = delete;
  NetworkListWifiHeaderView& operator=(const NetworkListWifiHeaderView&) =
      delete;
  ~NetworkListWifiHeaderView() override;

 private:
  friend class NetworkListWifiHeaderViewTest;

  // Used for testing.
  static constexpr int kJoinWifiButtonId =
      NetworkListNetworkHeaderView::kToggleButtonId + 1;

  // NetworkListNetworkHeaderView:
  void AddExtraButtons() override;
  void SetToggleState(bool enabled, bool is_on) override;
  void OnToggleToggled(bool is_on) override;

  void JoinWifiButtonPressed();
  void SetJoinWifiButtonState(bool enabled, bool visible);

  // A button to invoke "Join Wi-Fi network" dialog.
  IconButton* join_wifi_button_ = nullptr;

  base::WeakPtrFactory<NetworkListWifiHeaderView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_LIST_WIFI_HEADER_VIEW_H_
