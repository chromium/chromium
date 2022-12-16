// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_MOBILE_HEADER_VIEW_IMPL_H_
#define ASH_SYSTEM_NETWORK_NETWORK_LIST_MOBILE_HEADER_VIEW_IMPL_H_

#include "ash/ash_export.h"
#include "ash/style/icon_button.h"
#include "ash/system/network/network_list_mobile_header_view.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ash/system/tray/tri_view.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/view.h"

namespace ash {

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

  // Used for testing.
  static constexpr int kAddESimButtonId =
      NetworkListNetworkHeaderView::kToggleButtonId + 1;

  // NetworkListNetworkHeaderView:
  void AddExtraButtons() override;
  void OnToggleToggled(bool is_on) override;

  // NetworkListMobileHeaderView:
  void SetAddESimButtonState(bool enabled, bool visible) override;

  void AddESimButtonPressed();

  // Button that navigates to the Settings mobile data subpage with the eSIM
  // setup dialog open. This is null when the device is not eSIM-capable.
  IconButton* add_esim_button_ = nullptr;

  base::WeakPtrFactory<NetworkListMobileHeaderViewImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_LIST_MOBILE_HEADER_VIEW_IMPL_H_
