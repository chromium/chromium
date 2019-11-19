// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_UNIFIED_VPN_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_NETWORK_UNIFIED_VPN_DETAILED_VIEW_CONTROLLER_H_

#include <memory>

#include "ash/system/unified/detailed_view_controller.h"
#include "base/macros.h"

namespace ash {

namespace tray {
class VPNListView;
}  // namespace tray

class DetailedViewDelegate;
class UnifiedSystemTrayController;

// Controller of VPN detailed view in UnifiedSystemTray.
class UnifiedVPNDetailedViewController : public DetailedViewController {
 public:
  explicit UnifiedVPNDetailedViewController(
      UnifiedSystemTrayController* tray_controller);
  ~UnifiedVPNDetailedViewController() override;

  // DetailedViewControllerBase:
  views::View* CreateView() override;

 private:
  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;

  tray::VPNListView* view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(UnifiedVPNDetailedViewController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_UNIFIED_VPN_DETAILED_VIEW_CONTROLLER_H_
