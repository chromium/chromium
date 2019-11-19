// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_FEATURE_POD_BUTTON_H_
#define ASH_SYSTEM_NETWORK_NETWORK_FEATURE_POD_BUTTON_H_

#include "ash/system/network/network_icon_animation_observer.h"
#include "ash/system/network/tray_network_state_observer.h"
#include "ash/system/unified/feature_pod_button.h"

namespace ash {

// Button view class for network feature pod button. It uses network_icon
// animation to implement network connecting animation on feature pod button.
class NetworkFeaturePodButton : public FeaturePodButton,
                                public network_icon::AnimationObserver,
                                public TrayNetworkStateObserver {
 public:
  explicit NetworkFeaturePodButton(FeaturePodControllerBase* controller);
  ~NetworkFeaturePodButton() override;

  // Updates the button's icon and tooltip based on the current state of the
  // system.
  void Update();

  // network_icon::AnimationObserver:
  void NetworkIconChanged() override;

  // TrayNetworkStateObserver:
  void ActiveNetworkStateChanged() override;

  // views::Button:
  const char* GetClassName() const override;

 private:
  void UpdateTooltip(const base::string16& connection_state_message);

  DISALLOW_COPY_AND_ASSIGN(NetworkFeaturePodButton);
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_FEATURE_POD_BUTTON_H_
