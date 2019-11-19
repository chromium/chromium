// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_NETWORK_NETWORK_FEATURE_POD_CONTROLLER_H_

#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/macros.h"
#include "base/strings/string16.h"

namespace ash {

class NetworkFeaturePodButton;
class UnifiedSystemTrayController;

// Controller of network feature pod button.
class NetworkFeaturePodController : public FeaturePodControllerBase {
 public:
  NetworkFeaturePodController(UnifiedSystemTrayController* tray_controller);
  ~NetworkFeaturePodController() override;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  void OnIconPressed() override;
  void OnLabelPressed() override;
  SystemTrayItemUmaType GetUmaType() const override;

 private:
  void UpdateButton();

  // Unowned.
  UnifiedSystemTrayController* tray_controller_;
  NetworkFeaturePodButton* button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(NetworkFeaturePodController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_FEATURE_POD_CONTROLLER_H_
