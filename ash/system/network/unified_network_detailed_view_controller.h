// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_UNIFIED_NETWORK_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_NETWORK_UNIFIED_NETWORK_DETAILED_VIEW_CONTROLLER_H_

#include <memory>

#include "ash/system/unified/detailed_view_controller.h"

namespace ash {

class DetailedViewDelegate;
class NetworkListView;
class UnifiedSystemTrayController;

// Controller of Network detailed view in UnifiedSystemTray.
class UnifiedNetworkDetailedViewController : public DetailedViewController {
 public:
  explicit UnifiedNetworkDetailedViewController(
      UnifiedSystemTrayController* tray_controller);

  UnifiedNetworkDetailedViewController(
      const UnifiedNetworkDetailedViewController&) = delete;
  UnifiedNetworkDetailedViewController& operator=(
      const UnifiedNetworkDetailedViewController&) = delete;

  ~UnifiedNetworkDetailedViewController() override;

  // DetailedViewController:
  std::unique_ptr<views::View> CreateView() override;
  std::u16string GetAccessibleName() const override;

 private:
  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;

  NetworkListView* view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_UNIFIED_NETWORK_DETAILED_VIEW_CONTROLLER_H_
