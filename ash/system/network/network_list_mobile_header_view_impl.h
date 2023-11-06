// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_MOBILE_HEADER_VIEW_IMPL_H_
#define ASH_SYSTEM_NETWORK_NETWORK_LIST_MOBILE_HEADER_VIEW_IMPL_H_

#include "ash/ash_export.h"
#include "ash/system/network/network_list_mobile_header_view.h"

namespace ash {

class NetworkListNetworkHeaderView;

// Implementation of NetworkListMobileHeaderView.
class ASH_EXPORT NetworkListMobileHeaderViewImpl
    : public NetworkListMobileHeaderView {
 public:
  explicit NetworkListMobileHeaderViewImpl(
      NetworkListNetworkHeaderView::Delegate* delegate);
  NetworkListMobileHeaderViewImpl(const NetworkListMobileHeaderViewImpl&) =
      delete;
  NetworkListMobileHeaderViewImpl& operator=(
      const NetworkListMobileHeaderViewImpl&) = delete;
  ~NetworkListMobileHeaderViewImpl() override;

 private:
  friend class NetworkListMobileHeaderViewTest;
  friend class NetworkListViewControllerTest;

  // NetworkListNetworkHeaderView:
  void SetToggleState(bool enabled, bool is_on, bool animate_toggle) override;
  void OnToggleToggled(bool is_on) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_LIST_MOBILE_HEADER_VIEW_IMPL_H_
