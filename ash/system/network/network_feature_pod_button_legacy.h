// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_FEATURE_POD_BUTTON_LEGACY_H_
#define ASH_SYSTEM_NETWORK_NETWORK_FEATURE_POD_BUTTON_LEGACY_H_

#include "ash/system/network/network_icon_animation_observer.h"
#include "ash/system/network/tray_network_state_observer.h"
#include "ash/system/unified/feature_pod_button.h"

namespace ash {

// Button view class for network feature pod button. It uses network_icon
// animation to implement network connecting animation on feature pod button.
class NetworkFeaturePodButtonLegacy : public FeaturePodButton,
                                      public network_icon::AnimationObserver,
                                      public TrayNetworkStateObserver {
 public:
  explicit NetworkFeaturePodButtonLegacy(FeaturePodControllerBase* controller);

  NetworkFeaturePodButtonLegacy(const NetworkFeaturePodButtonLegacy&) = delete;
  NetworkFeaturePodButtonLegacy& operator=(
      const NetworkFeaturePodButtonLegacy&) = delete;

  ~NetworkFeaturePodButtonLegacy() override;

  // Updates the button's icon and tooltip based on the current state of the
  // system.
  void Update();

  // network_icon::AnimationObserver:
  void NetworkIconChanged() override;

  // TrayNetworkStateObserver:
  void ActiveNetworkStateChanged() override;

  // views::Button:
  void OnThemeChanged() override;
  const char* GetClassName() const override;

 private:
  void UpdateTooltip(const std::u16string& connection_state_message);
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_FEATURE_POD_BUTTON_LEGACY_H_
