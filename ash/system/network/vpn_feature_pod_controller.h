// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_VPN_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_NETWORK_VPN_FEATURE_POD_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/network/tray_network_state_observer.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace ash {

class UnifiedSystemTrayController;

// Controller of vpn feature tile.
class ASH_EXPORT VPNFeaturePodController : public FeaturePodControllerBase,
                                           public TrayNetworkStateObserver {
 public:
  explicit VPNFeaturePodController(
      UnifiedSystemTrayController* tray_controller);

  VPNFeaturePodController(const VPNFeaturePodController&) = delete;
  VPNFeaturePodController& operator=(const VPNFeaturePodController&) = delete;

  ~VPNFeaturePodController() override;

  // FeaturePodControllerBase:
  std::unique_ptr<FeatureTile> CreateTile(bool compact = false) override;
  QsFeatureCatalogName GetCatalogName() override;
  void OnIconPressed() override;

  // TrayNetworkStateObserver:
  void ActiveNetworkStateChanged() override;

 private:
  void Update();

  const raw_ptr<UnifiedSystemTrayController> tray_controller_;

  // Owned by views hierarchy.
  raw_ptr<FeatureTile, DanglingUntriaged> tile_ = nullptr;

  base::WeakPtrFactory<VPNFeaturePodController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_VPN_FEATURE_POD_CONTROLLER_H_
