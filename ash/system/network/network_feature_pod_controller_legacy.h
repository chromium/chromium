// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_FEATURE_POD_CONTROLLER_LEGACY_H_
#define ASH_SYSTEM_NETWORK_NETWORK_FEATURE_POD_CONTROLLER_LEGACY_H_

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/unified/feature_pod_controller_base.h"

namespace ash {

class NetworkFeaturePodButtonLegacy;
class UnifiedSystemTrayController;

// Controller of network feature pod button.
class NetworkFeaturePodControllerLegacy : public FeaturePodControllerBase {
 public:
  NetworkFeaturePodControllerLegacy(
      UnifiedSystemTrayController* tray_controller);

  NetworkFeaturePodControllerLegacy(const NetworkFeaturePodControllerLegacy&) =
      delete;
  NetworkFeaturePodControllerLegacy& operator=(
      const NetworkFeaturePodControllerLegacy&) = delete;

  ~NetworkFeaturePodControllerLegacy() override;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  QsFeatureCatalogName GetCatalogName() override;
  void OnIconPressed() override;
  void OnLabelPressed() override;

 private:
  void UpdateButton();

  // Unowned.
  UnifiedSystemTrayController* tray_controller_;
  NetworkFeaturePodButtonLegacy* button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_FEATURE_POD_CONTROLLER_LEGACY_H_
