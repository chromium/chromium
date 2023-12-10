// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_TETHER_HOSTS_HEADER_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_LIST_TETHER_HOSTS_HEADER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// This class is used to create the header for the Tether Hosts
// section of Quick Settings.
class ASH_EXPORT NetworkListTetherHostsHeaderView
    : public NetworkListNetworkHeaderView {
  METADATA_HEADER(NetworkListTetherHostsHeaderView,
                  NetworkListNetworkHeaderView)

 public:
  explicit NetworkListTetherHostsHeaderView(
      NetworkListNetworkHeaderView::Delegate* delegate);
  NetworkListTetherHostsHeaderView(const NetworkListTetherHostsHeaderView&) =
      delete;
  NetworkListTetherHostsHeaderView& operator=(
      const NetworkListTetherHostsHeaderView&) = delete;
  ~NetworkListTetherHostsHeaderView() override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_TETHER_HOSTS_NETWORK_LIST_HEADER_VIEW_H_
