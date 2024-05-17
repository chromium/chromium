// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_header_view.h"

#include <string>

#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "ash/system/tray/view_click_listener.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace ash {

NetworkListHeaderView::NetworkListHeaderView() {
  TrayPopupUtils::ConfigureHeader(this);
  // TODO(crbug.com/327247047): SetUseDefaultFillLayout(true) ignore insets by
  // default. But we need insets for border.
  SetLayoutManager(std::make_unique<views::FillLayout>());
  entry_row_ =
      AddChildView(std::make_unique<HoverHighlightView>(/*listener=*/this));
  entry_row_->SetFocusBehavior(FocusBehavior::NEVER);
}

void NetworkListHeaderView::OnViewClicked(views::View* sender) {
  // Handle clicks on the on/off entry row.
  if (sender == entry_row_) {
    // Not pressing on the toggle directly, there's no new state.
    UpdateToggleState(/*has_new_state=*/false);
  }
}

BEGIN_METADATA(NetworkListHeaderView)
END_METADATA

}  // namespace ash
