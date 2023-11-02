// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_HEADER_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_LIST_HEADER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/tray/tri_view.h"
#include "ui/views/view.h"

namespace ash {

// This class is used for the headers of both networks and VPNs, and will be
// responsible for initializing the core views for a header.
class ASH_EXPORT NetworkListHeaderView : public views::View {
 public:
  NetworkListHeaderView(const NetworkListHeaderView&) = delete;
  NetworkListHeaderView& operator=(const NetworkListHeaderView&) = delete;
  ~NetworkListHeaderView() override = default;

 protected:
  explicit NetworkListHeaderView(int label_id);

  TriView* container() const { return container_; }

  // Used for testing. This is 1 because view IDs should not be 0.
  static constexpr int kTitleLabelViewId = 1;

 private:
  friend class NetworkListNetworkHeaderViewTest;
  friend class NetworkListMobileHeaderViewTest;
  friend class NetworkListWifiHeaderViewTest;

  void AddTitleView(int label_id);

  TriView* container_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_LIST_HEADER_VIEW_H_
