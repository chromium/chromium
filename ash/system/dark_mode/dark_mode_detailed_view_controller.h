// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DARK_MODE_DARK_MODE_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_DARK_MODE_DARK_MODE_DETAILED_VIEW_CONTROLLER_H_

#include "ash/system/dark_mode/color_mode_observer.h"
#include "ash/system/unified/detailed_view_controller.h"

namespace ash {
class DarkModeDetailedView;
class DetailedViewDelegate;
class UnifiedSystemTrayController;

// Controller of dark mode detailed view in UnifiedSystemTray.
class DarkModeDetailedViewController : public DetailedViewController,
                                       public ColorModeObserver {
 public:
  explicit DarkModeDetailedViewController(
      UnifiedSystemTrayController* tray_controller);
  DarkModeDetailedViewController(const DarkModeDetailedViewController& other) =
      delete;
  DarkModeDetailedViewController& operator=(
      const DarkModeDetailedViewController& other) = delete;
  ~DarkModeDetailedViewController() override;

  // DetailedViewControllerBase:
  views::View* CreateView() override;
  base::string16 GetAccessibleName() const override;

  // ColorModeObserver:
  void OnColorModeChanged(bool dark_mode_enabled) override;
  void OnColorModeThemed(bool is_themed) override;

 private:
  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;

  DarkModeDetailedView* view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_DARK_MODE_DARK_MODE_DETAILED_VIEW_CONTROLLER_H_
