// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_DETAILED_NETWORK_VIEW_IMPL_H_
#define ASH_SYSTEM_NETWORK_NETWORK_DETAILED_NETWORK_VIEW_IMPL_H_

#include "ash/ash_export.h"

#include "ash/system/network/network_detailed_network_view.h"

namespace ash {

class DetailedViewDelegate;

namespace tray {

// This class is an implementation for NetworkDetailedNetworkView.
// TODO(b/207089013): extend and implement
// NetworkListNetworkHeaderView::Delegate when available.
class ASH_EXPORT NetworkDetailedNetworkViewImpl
    : public NetworkDetailedView,
      public NetworkDetailedNetworkView {
 public:
  NetworkDetailedNetworkViewImpl(
      DetailedViewDelegate* detailed_view_delegate,
      NetworkDetailedNetworkView::Delegate* delegate);
  NetworkDetailedNetworkViewImpl(const NetworkDetailedNetworkViewImpl&) =
      delete;
  NetworkDetailedNetworkViewImpl& operator=(
      const NetworkDetailedNetworkViewImpl&) = delete;
  ~NetworkDetailedNetworkViewImpl() override;

 private:
  // NetworkDetailedNetworkView:
  views::View* GetAsView() override;
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_DETAILED_NETWORK_VIEW_H_
