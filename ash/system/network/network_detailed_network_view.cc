// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_detailed_network_view.h"

#include <memory>
#include "ash/system/network/network_detailed_network_view_impl.h"
#include "ash/system/tray/detailed_view_delegate.h"

namespace ash {
namespace {
NetworkDetailedNetworkView::Factory* g_test_factory = nullptr;
}  // namespace

NetworkDetailedNetworkView::Delegate::~Delegate() = default;

NetworkDetailedNetworkView::NetworkDetailedNetworkView(Delegate* delegate)
    : delegate_(delegate) {}

std::unique_ptr<NetworkDetailedNetworkView>
NetworkDetailedNetworkView::Factory::Create(
    DetailedViewDelegate* detailed_view_delegate,
    Delegate* delegate) {
  if (g_test_factory)
    return g_test_factory->CreateForTesting(delegate);  // IN-TEST
  return std::make_unique<NetworkDetailedNetworkViewImpl>(
      detailed_view_delegate, delegate);
}

void NetworkDetailedNetworkView::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  g_test_factory = test_factory;
}

int NetworkDetailedNetworkView::GetScrollPosition() {
  return 0;
}

}  // namespace ash