// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_MOBILE_HEADER_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_LIST_MOBILE_HEADER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/style/icon_button.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ash/system/tray/tri_view.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/view.h"

namespace ash {

// This class is the implementation of the network list header for Mobile
// networks, and is responsible for the creation of mobile-specific buttons.
class ASH_EXPORT NetworkListMobileHeaderView
    : public NetworkListNetworkHeaderView {
 public:
  explicit NetworkListMobileHeaderView(
      NetworkListNetworkHeaderView::Delegate* delegate);
  NetworkListMobileHeaderView(const NetworkListMobileHeaderView&) = delete;
  NetworkListMobileHeaderView& operator=(const NetworkListMobileHeaderView&) =
      delete;
  ~NetworkListMobileHeaderView() override;

 private:
  friend class NetworkListMobileHeaderViewTest;

  // Used for testing.
  static constexpr int kAddESimButtonId =
      NetworkListNetworkHeaderView::kToggleButtonId + 1;

  // NetworkListNetworkHeaderView:
  void AddExtraButtons() override;
  void OnToggleToggled(bool is_on) override;

  void AddESimButtonPressed();
  void SetAddESimButtonState(bool enabled, bool visible);

  // Button that navigates to the Settings mobile data subpage with the eSIM
  // setup dialog open. This is null when the device is not eSIM-capable.
  IconButton* add_esim_button_ = nullptr;

  base::WeakPtrFactory<NetworkListMobileHeaderView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_LIST_MOBILE_HEADER_VIEW_H_
