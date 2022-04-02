// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_view_controller_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_detailed_network_view.h"
#include "ash/system/network/tray_network_state_model.h"

namespace ash {

NetworkListViewControllerImpl::NetworkListViewControllerImpl(
    NetworkDetailedNetworkView* network_detailed_network_view)
    : network_detailed_network_view_(network_detailed_network_view) {
  DCHECK(ash::features::IsQuickSettingsNetworkRevampEnabled());
  DCHECK(network_detailed_network_view_);
  Shell::Get()->system_tray_model()->network_state_model()->AddObserver(this);
}

NetworkListViewControllerImpl::~NetworkListViewControllerImpl() {
  Shell::Get()->system_tray_model()->network_state_model()->RemoveObserver(
      this);
}

void NetworkListViewControllerImpl::ActiveNetworkStateChanged() {
  // TODO(b/207089013): Implement this function.
}

void NetworkListViewControllerImpl::NetworkListChanged() {
  // TODO(b/207089013): Implement this function.
}

void NetworkListViewControllerImpl::DeviceStateListChanged() {
  // TODO(b/207089013): Implement this function.
}

}  // namespace ash
