// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_mobile_header_view_impl.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/network_list_mobile_header_view.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

NetworkListMobileHeaderViewImpl::NetworkListMobileHeaderViewImpl(
    NetworkListNetworkHeaderView::Delegate* delegate)
    : NetworkListMobileHeaderView(delegate) {}

NetworkListMobileHeaderViewImpl::~NetworkListMobileHeaderViewImpl() = default;

void NetworkListMobileHeaderViewImpl::SetToggleState(bool enabled,
                                                     bool is_on,
                                                     bool animate_toggle) {
  std::u16string tooltip_text = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NETWORK_TOGGLE_MOBILE,
      l10n_util::GetStringUTF16(
          is_on ? IDS_ASH_STATUS_TRAY_NETWORK_MOBILE_ENABLED
                : IDS_ASH_STATUS_TRAY_NETWORK_MOBILE_DISABLED));
  entry_row()->SetTooltipText(tooltip_text);
  toggle()->SetTooltipText(tooltip_text);
  NetworkListMobileHeaderView::SetToggleState(enabled, is_on, animate_toggle);
}

void NetworkListMobileHeaderViewImpl::OnToggleToggled(bool is_on) {
  delegate()->OnMobileToggleClicked(is_on);
}

}  // namespace ash
