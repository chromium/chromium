// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NEARBY_SHARE_NEARBY_SHARE_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_NEARBY_SHARE_NEARBY_SHARE_DETAILED_VIEW_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/unified/detailed_view_controller.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class DetailedViewDelegate;
class UnifiedSystemTrayController;

// Creates and passes the Nearby Share detailed view.
class ASH_EXPORT NearbyShareDetailedViewController
    : public DetailedViewController {
 public:
  explicit NearbyShareDetailedViewController(
      UnifiedSystemTrayController* tray_controller);
  NearbyShareDetailedViewController(const NearbyShareDetailedViewController&) =
      delete;
  NearbyShareDetailedViewController& operator=(
      const NearbyShareDetailedViewController&) = delete;
  ~NearbyShareDetailedViewController() override;

 private:
  // DetailedViewController
  std::unique_ptr<views::View> CreateView() override;
  std::u16string GetAccessibleName() const override;

  std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NEARBY_SHARE_NEARBY_SHARE_DETAILED_VIEW_CONTROLLER_H_
