// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_detailed_network_view_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/system/network/network_detailed_view.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

NetworkDetailedNetworkViewImpl::NetworkDetailedNetworkViewImpl(
    DetailedViewDelegate* detailed_view_delegate,
    NetworkDetailedNetworkView::Delegate* delegate)
    : NetworkDetailedView(detailed_view_delegate,
                          delegate,
                          NetworkDetailedView::ListType::LIST_TYPE_NETWORK),
      NetworkDetailedNetworkView(delegate) {
  DCHECK(ash::features::IsQuickSettingsNetworkRevampEnabled());
}

NetworkDetailedNetworkViewImpl::~NetworkDetailedNetworkViewImpl() = default;

views::View* NetworkDetailedNetworkViewImpl::GetAsView() {
  return this;
}

BEGIN_METADATA(NetworkDetailedNetworkViewImpl, views::View)
END_METADATA

}  // namespace ash