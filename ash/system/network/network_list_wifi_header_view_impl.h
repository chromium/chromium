// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_WIFI_HEADER_VIEW_IMPL_H_
#define ASH_SYSTEM_NETWORK_NETWORK_LIST_WIFI_HEADER_VIEW_IMPL_H_

#include "ash/ash_export.h"
#include "ash/style/icon_button.h"
#include "ash/system/network/network_list_wifi_header_view.h"
#include "ash/system/tray/tri_view.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/view.h"

namespace ash {

// Implementation of NetworkListWifiHeaderView.
class ASH_EXPORT NetworkListWifiHeaderViewImpl
    : public NetworkListWifiHeaderView {
 public:
  explicit NetworkListWifiHeaderViewImpl(
      NetworkListNetworkHeaderView::Delegate* delegate);
  NetworkListWifiHeaderViewImpl(const NetworkListWifiHeaderViewImpl&) = delete;
  NetworkListWifiHeaderViewImpl& operator=(
      const NetworkListWifiHeaderViewImpl&) = delete;
  ~NetworkListWifiHeaderViewImpl() override;

 private:
  friend class NetworkListWifiHeaderViewTest;

  // Used for testing.
  static constexpr int kJoinWifiButtonId =
      NetworkListNetworkHeaderView::kToggleButtonId + 2;

  // NetworkListNetworkHeaderView:
  void AddExtraButtons() override;
  void SetToggleState(bool enabled, bool is_on, bool animate_toggle) override;
  void OnToggleToggled(bool is_on) override;

  // NetworkListWifiHeaderView:
  void SetJoinWifiButtonState(bool enabled, bool visible) override;

  void JoinWifiButtonPressed();

  // A button to invoke "Join Wi-Fi network" dialog.
  raw_ptr<IconButton, ExperimentalAsh> join_wifi_button_ = nullptr;

  base::WeakPtrFactory<NetworkListWifiHeaderViewImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_LIST_WIFI_HEADER_VIEW_IMPL_H_
