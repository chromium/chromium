// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_WIFI_HEADER_VIEW_IMPL_H_
#define ASH_SYSTEM_NETWORK_NETWORK_LIST_WIFI_HEADER_VIEW_IMPL_H_

#include "ash/ash_export.h"
#include "ash/style/icon_button.h"
#include "ash/system/network/network_list_wifi_header_view.h"
#include "ash/system/tray/tri_view.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/view.h"

namespace ash {

// Implementation of NetworkListWifiHeaderView.
class ASH_EXPORT NetworkListWifiHeaderViewImpl
    : public NetworkListWifiHeaderView {
 public:
  explicit NetworkListWifiHeaderViewImpl(
      NetworkListNetworkHeaderView::Delegate* delegate);
  NetworkListWifiHeaderViewImpl(const NetworkListWifiHeaderViewImpl&) = delete;
  NetworkListWifiHeaderViewImpl& operator=(
      const NetworkListWifiHeaderViewImpl&) = delete;
  ~NetworkListWifiHeaderViewImpl() override;

 private:
  friend class NetworkListWifiHeaderViewTest;

  // NetworkListNetworkHeaderView:
  void SetToggleState(bool enabled, bool is_on, bool animate_toggle) override;
  void OnToggleToggled(bool is_on) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_LIST_WIFI_HEADER_VIEW_IMPL_H_
