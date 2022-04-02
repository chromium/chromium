// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/fake_network_detailed_network_view.h"

#include "ash/system/network/network_detailed_network_view.h"

namespace ash {

FakeNetworkDetailedNetworkView::FakeNetworkDetailedNetworkView(
    Delegate* delegate)
    : NetworkDetailedNetworkView(delegate) {}

FakeNetworkDetailedNetworkView::~FakeNetworkDetailedNetworkView() = default;

views::View* FakeNetworkDetailedNetworkView::GetAsView() {
  return this;
}

}  // namespace ash