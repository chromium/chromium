// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_NOTIFIER_SETTINGS_CONTROLLER_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_NOTIFIER_SETTINGS_CONTROLLER_H_

#include <memory>

#include "ash/system/unified/detailed_view_controller.h"

namespace ash {

class DetailedViewDelegate;
class UnifiedSystemTrayController;

// Controller of notifier settings detailed view in UnifiedSystemTray.
class UnifiedNotifierSettingsController : public DetailedViewController {
 public:
  explicit UnifiedNotifierSettingsController(
      UnifiedSystemTrayController* tray_controller);

  UnifiedNotifierSettingsController(const UnifiedNotifierSettingsController&) =
      delete;
  UnifiedNotifierSettingsController& operator=(
      const UnifiedNotifierSettingsController&) = delete;

  ~UnifiedNotifierSettingsController() override;

  // DetailedViewController:
  std::unique_ptr<views::View> CreateView() override;
  std::u16string GetAccessibleName() const override;

 private:
  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_NOTIFIER_SETTINGS_CONTROLLER_H_
