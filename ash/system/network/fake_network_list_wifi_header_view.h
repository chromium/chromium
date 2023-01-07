// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_FAKE_NETWORK_LIST_WIFI_HEADER_VIEW_H_
#define ASH_SYSTEM_NETWORK_FAKE_NETWORK_LIST_WIFI_HEADER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/style/icon_button.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ash/system/network/network_list_wifi_header_view.h"
#include "ash/system/tray/tri_view.h"

namespace ash {

// Fake implementation of NetworkListWifiHeaderView
class ASH_EXPORT FakeNetworkListWifiHeaderView
    : public NetworkListWifiHeaderView {
 public:
  explicit FakeNetworkListWifiHeaderView(
      NetworkListNetworkHeaderView::Delegate* delegate);
  FakeNetworkListWifiHeaderView(const FakeNetworkListWifiHeaderView&) = delete;
  FakeNetworkListWifiHeaderView& operator=(
      const FakeNetworkListWifiHeaderView&) = delete;
  ~FakeNetworkListWifiHeaderView() override;

  bool is_toggle_enabled() { return is_toggle_enabled_; }

  bool is_toggle_on() { return is_toggle_on_; }

  size_t set_toggle_state_count() { return set_toggle_state_count_; }

  bool is_join_wifi_enabled() { return is_join_wifi_enabled_; }

  bool is_join_wifi_visible() { return is_join_wifi_visible_; }

  size_t set_join_wifi_button_state_count() {
    return set_join_wifi_button_state_count_;
  }

 private:
  // NetworkListNetworkHeaderView:
  void SetToggleState(bool enabled, bool visible, bool animate_toggle) override;

  // NetworkListWifiHeaderView:
  void SetJoinWifiButtonState(bool enabled, bool visible) override;

  bool is_toggle_enabled_;
  bool is_toggle_on_;
  size_t set_toggle_state_count_;

  bool is_join_wifi_enabled_;
  bool is_join_wifi_visible_;
  size_t set_join_wifi_button_state_count_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_FAKE_NETWORK_LIST_WIFI_HEADER_VIEW_H_
