// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_FAKE_NETWORK_LIST_NETWORK_HEADER_VIEW_DELEGATE_H_
#define ASH_SYSTEM_NETWORK_FAKE_NETWORK_LIST_NETWORK_HEADER_VIEW_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/system/network/network_list_network_header_view.h"

namespace ash {

// Fake of NetworkListNetworkHeaderView::Delegate implementation.
class ASH_EXPORT FakeNetworkListNetworkHeaderViewDelegate
    : public NetworkListNetworkHeaderView::Delegate {
 public:
  FakeNetworkListNetworkHeaderViewDelegate();
  FakeNetworkListNetworkHeaderViewDelegate(
      const FakeNetworkListNetworkHeaderViewDelegate&) = delete;
  FakeNetworkListNetworkHeaderViewDelegate& operator=(
      const FakeNetworkListNetworkHeaderViewDelegate&) = delete;
  ~FakeNetworkListNetworkHeaderViewDelegate() override;

  // NetworkListNetworkHeaderView::Delegate
  void OnMobileToggleClicked(bool new_state) override;
  void OnWifiToggleClicked(bool new_state) override;

  size_t mobile_toggle_clicked_count() const {
    return mobile_toggle_clicked_count_;
  }

  size_t wifi_toggle_clicked_count() const {
    return wifi_toggle_clicked_count_;
  }

 private:
  size_t mobile_toggle_clicked_count_ = 0;
  size_t wifi_toggle_clicked_count_ = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_FAKE_NETWORK_LIST_NETWORK_HEADER_VIEW_DELEGATE_H_
