// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PRIVACY_HUB_DELEGATE_H_
#define ASH_PUBLIC_CPP_PRIVACY_HUB_DELEGATE_H_

#include <cstdint>

#include "ash/constants/geolocation_access_level.h"

namespace cros::mojom {
enum class CameraPrivacySwitchState : int32_t;
}

namespace ash {

// This class serves as a callback into webui for Privacy Hub backend in
// //ash/system.
class PrivacyHubDelegate {
 public:
  virtual ~PrivacyHubDelegate() = default;

  // Signals that the state of the microphone hardware toggle changed
  virtual void MicrophoneHardwareToggleChanged(bool muted) = 0;

  // Enable or disable ('gray out') the camera switch in the UI.
  virtual void SetForceDisableCameraSwitch(bool disabled) = 0;

  // Signals that the system location setting has changed.
  // NOTE: UI can't simply observe the `prefs.ash.user.geolocation_access_level`
  // because in multi-user scenario the system location setting is exclusively
  // controlled by the primary user of the session.
  virtual void SystemGeolocationAccessLevelChanged(
      GeolocationAccessLevel access_level) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PRIVACY_HUB_DELEGATE_H_
