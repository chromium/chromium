// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_WIFI_HEADER_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_LIST_WIFI_HEADER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

// This class is the interface used to create network list header for Wifi
// networks, and is responsible for the creation of wifi-specific buttons.
// TODO(b/251724646): remove this interface after the QsRevamp clean up.
class ASH_EXPORT NetworkListWifiHeaderView
    : public NetworkListNetworkHeaderView {
 public:
  METADATA_HEADER(NetworkListWifiHeaderView);

  explicit NetworkListWifiHeaderView(
      NetworkListNetworkHeaderView::Delegate* delegate);
  NetworkListWifiHeaderView(const NetworkListWifiHeaderView&) = delete;
  NetworkListWifiHeaderView& operator=(const NetworkListWifiHeaderView&) =
      delete;
  ~NetworkListWifiHeaderView() override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_LIST_WIFI_HEADER_VIEW_H_
