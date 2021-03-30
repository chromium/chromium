// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MEDIA_UNIFIED_MEDIA_CONTROLS_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_MEDIA_UNIFIED_MEDIA_CONTROLS_DETAILED_VIEW_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/unified/detailed_view_controller.h"

namespace ash {

class DetailedViewDelegate;
class UnifiedSystemTrayController;

// Controller of UnifiedMediaControlsDetailedView in UnifiedSystemTray.
class ASH_EXPORT UnifiedMediaControlsDetailedViewController
    : public DetailedViewController {
 public:
  explicit UnifiedMediaControlsDetailedViewController(
      UnifiedSystemTrayController* tray_controller);
  ~UnifiedMediaControlsDetailedViewController() override;

  // DetailedViewController implementations.
  views::View* CreateView() override;
  std::u16string GetAccessibleName() const override;

 private:
  friend class UnifiedMediaControlsDetailedViewControllerTest;

  static bool detailed_view_has_shown_;

  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MEDIA_UNIFIED_MEDIA_CONTROLS_DETAILED_VIEW_CONTROLLER_H_
