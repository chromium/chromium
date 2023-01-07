// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_FAKE_NETWORK_DETAILED_NETWORK_VIEW_H_
#define ASH_SYSTEM_NETWORK_FAKE_NETWORK_DETAILED_NETWORK_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/network/network_detailed_network_view.h"
#include "ash/system/network/network_list_item_view.h"
#include "ash/system/network/network_list_mobile_header_view_impl.h"
#include "ash/system/network/network_list_network_item_view.h"
#include "ash/system/network/network_list_wifi_header_view_impl.h"
#include "ui/views/view.h"

namespace ash {

// Fake implementation of NetworkDetailedNetworkView.
class ASH_EXPORT FakeNetworkDetailedNetworkView
    : public NetworkDetailedNetworkView,
      public views::View,
      public ViewClickListener {
 public:
  explicit FakeNetworkDetailedNetworkView(Delegate* delegate);
  FakeNetworkDetailedNetworkView(const FakeNetworkDetailedNetworkView&) =
      delete;
  FakeNetworkDetailedNetworkView& operator=(
      const FakeNetworkDetailedNetworkView&) = delete;
  ~FakeNetworkDetailedNetworkView() override;

  size_t notify_network_list_changed_call_count() {
    return notify_network_list_changed_call_count_;
  }

  bool last_scan_bar_visibility() { return last_scan_bar_visibility_; }

  const NetworkListItemView* last_clicked_network_list_item() const {
    return last_clicked_network_list_item_;
  }

  views::View* network_list() override;

 private:
  // NetworkDetailedNetworkView:
  void NotifyNetworkListChanged() override;
  views::View* GetAsView() override;
  NetworkListNetworkItemView* AddNetworkListItem() override;
  NetworkListWifiHeaderView* AddWifiSectionHeader() override;
  NetworkListMobileHeaderView* AddMobileSectionHeader() override;
  void UpdateScanningBarVisibility(bool visible) override;

  // ViewClickListener:
  void OnViewClicked(views::View* view) override;

  std::unique_ptr<views::View> network_list_;
  size_t notify_network_list_changed_call_count_ = 0;
  bool last_scan_bar_visibility_;
  NetworkListItemView* last_clicked_network_list_item_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_FAKE_NETWORK_DETAILED_NETWORK_VIEW_H_
