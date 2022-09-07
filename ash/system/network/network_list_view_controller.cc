// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_view_controller.h"

#include "ash/system/network/network_detailed_network_view.h"
#include "ash/system/network/network_list_view_controller_impl.h"

namespace ash {

namespace {
NetworkListViewController::Factory* g_test_factory = nullptr;
}  // namespace

std::unique_ptr<NetworkListViewController>
NetworkListViewController::Factory::Create(
    NetworkDetailedNetworkView* network_detailed_network_view) {
  if (g_test_factory)
    return g_test_factory->CreateForTesting();  // IN-TEST
  return std::make_unique<NetworkListViewControllerImpl>(
      network_detailed_network_view);
}

void NetworkListViewController::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  g_test_factory = test_factory;
}

}  // namespace ash
