// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BRIGHTNESS_QUICK_SETTINGS_DISPLAY_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_BRIGHTNESS_QUICK_SETTINGS_DISPLAY_DETAILED_VIEW_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class DisplayDetailedView;
class DetailedViewDelegate;
class UnifiedSystemTrayController;

// Controller of `DisplayDetailedView` in `UnifiedSystemTray`.
class ASH_EXPORT QuickSettingsDisplayDetailedViewController
    : public DetailedViewController {
 public:
  explicit QuickSettingsDisplayDetailedViewController(
      UnifiedSystemTrayController* tray_controller);

  QuickSettingsDisplayDetailedViewController(
      const QuickSettingsDisplayDetailedViewController&) = delete;
  QuickSettingsDisplayDetailedViewController& operator=(
      const QuickSettingsDisplayDetailedViewController&) = delete;

  ~QuickSettingsDisplayDetailedViewController() override;

  // DetailedViewController:
  std::unique_ptr<views::View> CreateView() override;
  std::u16string GetAccessibleName() const override;

 private:
  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;
  const raw_ptr<UnifiedSystemTrayController> tray_controller_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BRIGHTNESS_QUICK_SETTINGS_DISPLAY_DETAILED_VIEW_CONTROLLER_H_
