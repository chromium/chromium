// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_FAKE_NETWORK_LIST_TETHER_HOSTS_HEADER_VIEW_H_
#define ASH_SYSTEM_NETWORK_FAKE_NETWORK_LIST_TETHER_HOSTS_HEADER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ash/system/network/network_list_tether_hosts_header_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// Fake implementation of NetworkListTetherHostsHeaderView
class ASH_EXPORT FakeNetworkListTetherHostsHeaderView
    : public NetworkListTetherHostsHeaderView {
  METADATA_HEADER(FakeNetworkListTetherHostsHeaderView,
                  NetworkListTetherHostsHeaderView)

 public:
  explicit FakeNetworkListTetherHostsHeaderView(OnExpandedStateToggle callback);
  FakeNetworkListTetherHostsHeaderView(
      const FakeNetworkListTetherHostsHeaderView&) = delete;
  FakeNetworkListTetherHostsHeaderView& operator=(
      const FakeNetworkListTetherHostsHeaderView&) = delete;
  ~FakeNetworkListTetherHostsHeaderView() override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_FAKE_NETWORK_LIST_TETHER_HOSTS_HEADER_VIEW_H_
