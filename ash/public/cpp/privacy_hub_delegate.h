// Copyright 2022 The Chromium Authors
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
  // Signals that the state of the microphone hardware toggle changed
  virtual void MicrophoneHardwareToggleChanged(bool muted) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PRIVACY_HUB_DELEGATE_H_
