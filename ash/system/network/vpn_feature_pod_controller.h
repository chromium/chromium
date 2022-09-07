// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_VPN_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_NETWORK_VPN_FEATURE_POD_CONTROLLER_H_

#include "ash/system/network/tray_network_state_observer.h"
#include "ash/system/unified/feature_pod_controller_base.h"

namespace ash {

class UnifiedSystemTrayController;

// Controller of vpn feature pod button.
class VPNFeaturePodController : public FeaturePodControllerBase,
                                public TrayNetworkStateObserver {
 public:
  explicit VPNFeaturePodController(
      UnifiedSystemTrayController* tray_controller);

  VPNFeaturePodController(const VPNFeaturePodController&) = delete;
  VPNFeaturePodController& operator=(const VPNFeaturePodController&) = delete;

  ~VPNFeaturePodController() override;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  void OnIconPressed() override;
  SystemTrayItemUmaType GetUmaType() const override;

  // TrayNetworkStateObserver:
  void ActiveNetworkStateChanged() override;

 private:
  void Update();

  // Unowned.
  UnifiedSystemTrayController* const tray_controller_;
  FeaturePodButton* button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_VPN_FEATURE_POD_CONTROLLER_H_
