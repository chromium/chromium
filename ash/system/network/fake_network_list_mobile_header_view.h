// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_FAKE_NETWORK_LIST_MOBILE_HEADER_VIEW_H_
#define ASH_SYSTEM_NETWORK_FAKE_NETWORK_LIST_MOBILE_HEADER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/network/network_list_mobile_header_view.h"
#include "ash/system/network/network_list_network_header_view.h"

namespace ash {

// Fake implementation of NetworkListMobileHeaderView
class ASH_EXPORT FakeNetworkListMobileHeaderView
    : public NetworkListMobileHeaderView {
 public:
  explicit FakeNetworkListMobileHeaderView(
      NetworkListNetworkHeaderView::Delegate* delegate);
  FakeNetworkListMobileHeaderView(const FakeNetworkListMobileHeaderView&) =
      delete;
  FakeNetworkListMobileHeaderView& operator=(
      const FakeNetworkListMobileHeaderView&) = delete;
  ~FakeNetworkListMobileHeaderView() override;

  bool is_toggle_enabled() { return is_toggle_enabled_; }

  bool is_toggle_on() { return is_toggle_on_; }

  size_t set_toggle_state_count() { return set_toggle_state_count_; }

 private:
  // NetworkListNetworkHeaderView:
  void SetToggleState(bool enabled, bool is_on, bool animate_toggle) override;


  bool is_toggle_enabled_;
  bool is_toggle_on_;
  size_t set_toggle_state_count_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_FAKE_NETWORK_LIST_MOBIL_HEADER_VIEW_H_
