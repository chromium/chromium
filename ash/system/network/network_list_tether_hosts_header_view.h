// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_TETHER_HOSTS_HEADER_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_LIST_TETHER_HOSTS_HEADER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/style/rounded_container.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ash/system/tray/view_click_listener.h"
#include "base/functional/callback_forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/image_view.h"

namespace ash {

// This class is used to create the header for the Tether Hosts
// section of Quick Settings.
class ASH_EXPORT NetworkListTetherHostsHeaderView : public RoundedContainer,
                                                    public ViewClickListener {
  METADATA_HEADER(NetworkListTetherHostsHeaderView, RoundedContainer)

 public:
  using OnExpandedStateToggle = base::RepeatingClosure;

  explicit NetworkListTetherHostsHeaderView(OnExpandedStateToggle callback);
  NetworkListTetherHostsHeaderView(const NetworkListTetherHostsHeaderView&) =
      delete;
  NetworkListTetherHostsHeaderView& operator=(
      const NetworkListTetherHostsHeaderView&) = delete;
  ~NetworkListTetherHostsHeaderView() override;

  bool is_expanded() { return is_expanded_; }

 private:
  friend class NetworkListTetherHostsHeaderViewTest;

  enum class NetworkListTetherHostsHeaderViewChildId {
    kEntryRow = 1,
    kChevron = 2,
  };

  // ViewClickListener:
  void OnViewClicked(views::View* sender) override;

  void ToggleExpandedState();

  raw_ptr<views::ImageView> chevron_;
  bool is_expanded_ = true;
  OnExpandedStateToggle callback_;

  base::WeakPtrFactory<NetworkListTetherHostsHeaderView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_LIST_TETHER_HOSTS_HEADER_VIEW_H_
