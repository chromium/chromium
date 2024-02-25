// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_UNIFIED_VPN_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_NETWORK_UNIFIED_VPN_DETAILED_VIEW_CONTROLLER_H_

#include <memory>

#include "ash/system/unified/detailed_view_controller.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class DetailedViewDelegate;
class UnifiedSystemTrayController;
class VpnDetailedView;

// Controller of VPN detailed view in UnifiedSystemTray.
class UnifiedVPNDetailedViewController : public DetailedViewController {
 public:
  explicit UnifiedVPNDetailedViewController(
      UnifiedSystemTrayController* tray_controller);

  UnifiedVPNDetailedViewController(const UnifiedVPNDetailedViewController&) =
      delete;
  UnifiedVPNDetailedViewController& operator=(
      const UnifiedVPNDetailedViewController&) = delete;

  ~UnifiedVPNDetailedViewController() override;

  // DetailedViewController:
  std::unique_ptr<views::View> CreateView() override;
  std::u16string GetAccessibleName() const override;

 private:
  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;

  raw_ptr<VpnDetailedView, DanglingUntriaged> view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_UNIFIED_VPN_DETAILED_VIEW_CONTROLLER_H_
