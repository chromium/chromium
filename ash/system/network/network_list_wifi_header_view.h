// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_WIFI_HEADER_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_LIST_WIFI_HEADER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// Creates network list header for Wifi networks.
class ASH_EXPORT NetworkListWifiHeaderView
    : public NetworkListNetworkHeaderView {
  METADATA_HEADER(NetworkListWifiHeaderView, NetworkListNetworkHeaderView)

 public:
  explicit NetworkListWifiHeaderView(
      NetworkListNetworkHeaderView::Delegate* delegate);
  NetworkListWifiHeaderView(const NetworkListWifiHeaderView&) = delete;
  NetworkListWifiHeaderView& operator=(const NetworkListWifiHeaderView&) =
      delete;
  ~NetworkListWifiHeaderView() override;

  // NetworkListNetworkHeaderView:
  void SetToggleState(bool enabled, bool is_on, bool animate_toggle) override;
  void OnToggleToggled(bool is_on) override;

 private:
  friend class NetworkListWifiHeaderViewTest;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_LIST_WIFI_HEADER_VIEW_H_
