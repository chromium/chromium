// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_NETWORK_NETWORK_FEATURE_POD_CONTROLLER_H_

#include <string>

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/network/network_feature_pod_button.h"
#include "ash/system/network/network_icon_animation_observer.h"
#include "ash/system/network/tray_network_state_observer.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace ash {

class FeaturePodButton;
class UnifiedSystemTrayController;

// Controller of the feature pod button that allows users to toggle whether
// certain network technologies are enabled or disabled, and that allows users
// to navigate to a more detailed page with a network list.
class ASH_EXPORT NetworkFeaturePodController
    : public network_icon::AnimationObserver,
      public FeaturePodControllerBase,
      public NetworkFeaturePodButton::Delegate,
      public TrayNetworkStateObserver {
 public:
  explicit NetworkFeaturePodController(
      UnifiedSystemTrayController* tray_controller);
  NetworkFeaturePodController(const NetworkFeaturePodController&) = delete;
  NetworkFeaturePodController& operator=(const NetworkFeaturePodController&) =
      delete;
  ~NetworkFeaturePodController() override;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  QsFeatureCatalogName GetCatalogName() override;
  void OnIconPressed() override;
  void OnLabelPressed() override;

 private:
  // network_icon::AnimationObserver:
  void NetworkIconChanged() override;

  // NetworkFeaturePodButton::Delegate:
  void OnFeaturePodButtonThemeChanged() override;

  // TrayNetworkStateObserver:
  void ActiveNetworkStateChanged() override;

  std::u16string ComputeButtonLabel(
      const chromeos::network_config::mojom::NetworkStateProperties* network)
      const;
  std::u16string ComputeButtonSubLabel(
      const chromeos::network_config::mojom::NetworkStateProperties* network)
      const;

  // Updates |button_| state to reflect the current state of networks.
  void UpdateButtonStateIfExists();

  FeaturePodButton* button_ = nullptr;
  UnifiedSystemTrayController* tray_controller_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_FEATURE_POD_CONTROLLER_H_
