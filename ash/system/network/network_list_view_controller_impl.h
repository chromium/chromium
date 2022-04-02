// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_VIEW_CONTROLLER_IMPL_H_
#define ASH_SYSTEM_NETWORK_NETWORK_LIST_VIEW_CONTROLLER_IMPL_H_

#include "ash/ash_export.h"

#include "ash/system/network/network_list_view_controller.h"
#include "ash/system/network/tray_network_state_observer.h"

namespace ash {

class NetworkDetailedNetworkView;

// Implementation of NetworkListViewController
class ASH_EXPORT NetworkListViewControllerImpl
    : public TrayNetworkStateObserver,
      public NetworkListViewController {
 public:
  NetworkListViewControllerImpl(
      NetworkDetailedNetworkView* network_detailed_network_view);
  NetworkListViewControllerImpl(const NetworkListViewController&) = delete;
  NetworkListViewControllerImpl& operator=(
      const NetworkListViewControllerImpl&) = delete;
  ~NetworkListViewControllerImpl() override;

 protected:
  NetworkDetailedNetworkView* network_detailed_network_view() {
    return network_detailed_network_view_;
  }

 private:
  // TrayNetworkStateObserver:
  void ActiveNetworkStateChanged() override;
  void NetworkListChanged() override;
  void DeviceStateListChanged() override;

  NetworkDetailedNetworkView* network_detailed_network_view_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_LIST_VIEW_CONTROLLER_IMPL_H_
