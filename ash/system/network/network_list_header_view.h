// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_HEADER_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_LIST_HEADER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/tray/tri_view.h"
#include "ash/system/tray/view_click_listener.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class HoverHighlightView;

// This class is used for the headers of both networks and VPNs, and will be
// responsible for initializing the core views for a header.
class ASH_EXPORT NetworkListHeaderView : public views::View,
                                         public ViewClickListener {
  METADATA_HEADER(NetworkListHeaderView, views::View)

 public:
  NetworkListHeaderView(const NetworkListHeaderView&) = delete;
  NetworkListHeaderView& operator=(const NetworkListHeaderView&) = delete;
  ~NetworkListHeaderView() override = default;

  // ViewClickListener:
  void OnViewClicked(views::View* sender) final;

 protected:
  explicit NetworkListHeaderView();

  // The callback called when the toggle button is pressed. Here it's used on
  // the entry row, so pressing on this entry will also turn on/off the toggle.
  // The toggle will update its state based on the new state. If `has_new_state`
  // is true, the toggle's current state is the new state. Otherwise, the
  // opposite of the toggle's current state will be the new state.
  virtual void UpdateToggleState(bool has_new_state) = 0;

  HoverHighlightView* entry_row() const { return entry_row_; }

 private:
  friend class NetworkListNetworkHeaderViewTest;
  friend class NetworkListMobileHeaderViewTest;
  friend class NetworkListWifiHeaderViewTest;
  friend class NetworkListTetherHostsHeaderViewTest;

  // Owned by the views hierarchy.
  raw_ptr<HoverHighlightView> entry_row_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_LIST_HEADER_VIEW_H_
