// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PRIVACY_HUB_DELEGATE_H_
#define ASH_PUBLIC_CPP_PRIVACY_HUB_DELEGATE_H_

#include "base/values.h"

namespace cros::mojom {
enum class CameraPrivacySwitchState : int32_t;
}

namespace ash {

// This class serves as a callback into webui for Privacy Hub backend in
// //ash/system.
class PrivacyHubDelegate {
 public:
  // Signals that the availability of microphone changed
  virtual void AvailabilityOfMicrophoneChanged(
      bool has_active_input_device) = 0;
  // Signals that the state of the microphone hardware toggle changed
  virtual void MicrophoneHardwareToggleChanged(bool muted) = 0;
  // Signals that the state of the camera hardware toggle changed
  virtual void CameraHardwareToggleChanged(
      cros::mojom::CameraPrivacySwitchState state) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PRIVACY_HUB_DELEGATE_H_
