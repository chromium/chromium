// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAST_UNIFIED_CAST_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_CAST_UNIFIED_CAST_DETAILED_VIEW_CONTROLLER_H_

#include "ash/public/cpp/cast_config_controller.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class DetailedViewDelegate;
class UnifiedSystemTrayController;

// Controller of Cast detailed view in UnifiedSystemTray.
class UnifiedCastDetailedViewController : public DetailedViewController {
 public:
  explicit UnifiedCastDetailedViewController(
      UnifiedSystemTrayController* tray_controller);

  UnifiedCastDetailedViewController(const UnifiedCastDetailedViewController&) =
      delete;
  UnifiedCastDetailedViewController& operator=(
      const UnifiedCastDetailedViewController&) = delete;

  ~UnifiedCastDetailedViewController() override;

  // DetailedViewController:
  std::unique_ptr<views::View> CreateView() override;
  std::u16string GetAccessibleName() const override;

 private:
  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAST_UNIFIED_CAST_DETAILED_VIEW_CONTROLLER_H_
