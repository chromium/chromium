// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "ash/system/network/unified_vpn_detailed_view_controller.h"

#include <memory>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/vpn_detailed_view.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

UnifiedVPNDetailedViewController::UnifiedVPNDetailedViewController(
    UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)) {
}

UnifiedVPNDetailedViewController::~UnifiedVPNDetailedViewController() = default;

std::unique_ptr<views::View> UnifiedVPNDetailedViewController::CreateView() {
  DCHECK(!view_);
  auto view = std::make_unique<VpnDetailedView>(
      detailed_view_delegate_.get(),
      Shell::Get()->session_controller()->login_status());
  view_ = view.get();
  view_->Init();
  return view;
}

std::u16string UnifiedVPNDetailedViewController::GetAccessibleName() const {
  return l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_VPN_SETTINGS_ACCESSIBLE_DESCRIPTION);
}

}  // namespace ash
