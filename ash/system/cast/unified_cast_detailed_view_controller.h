// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAST_UNIFIED_CAST_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_CAST_UNIFIED_CAST_DETAILED_VIEW_CONTROLLER_H_

#include "ash/public/cpp/cast_config_controller.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "base/macros.h"

namespace ash {
namespace tray {
class CastDetailedView;
}  // namespace tray

class DetailedViewDelegate;
class UnifiedSystemTrayController;

// Controller of Cast detailed view in UnifiedSystemTray.
class UnifiedCastDetailedViewController : public DetailedViewController {
 public:
  explicit UnifiedCastDetailedViewController(
      UnifiedSystemTrayController* tray_controller);
  ~UnifiedCastDetailedViewController() override;

  // DetailedViewControllerBase:
  views::View* CreateView() override;

 private:
  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;

  tray::CastDetailedView* view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(UnifiedCastDetailedViewController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAST_UNIFIED_CAST_DETAILED_VIEW_CONTROLLER_H_
