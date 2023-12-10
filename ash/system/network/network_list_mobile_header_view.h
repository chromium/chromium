// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_MOBILE_HEADER_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_LIST_MOBILE_HEADER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// Creates network list header for Mobile networks.
class ASH_EXPORT NetworkListMobileHeaderView
    : public NetworkListNetworkHeaderView {
  METADATA_HEADER(NetworkListMobileHeaderView, NetworkListNetworkHeaderView)

 public:
  explicit NetworkListMobileHeaderView(
      NetworkListNetworkHeaderView::Delegate* delegate);
  NetworkListMobileHeaderView(const NetworkListMobileHeaderView&) = delete;
  NetworkListMobileHeaderView& operator=(const NetworkListMobileHeaderView&) =
      delete;
  ~NetworkListMobileHeaderView() override;

  // NetworkListNetworkHeaderView:
  void SetToggleState(bool enabled, bool is_on, bool animate_toggle) override;
  void OnToggleToggled(bool is_on) override;

 private:
  friend class NetworkListMobileHeaderViewTest;
  friend class NetworkListViewControllerTest;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_LIST_MOBILE_HEADER_VIEW_H_
