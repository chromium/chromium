// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_HID_DETECTION_HID_DETECTION_MANAGER_H_
#define ASH_COMPONENTS_HID_DETECTION_HID_DETECTION_MANAGER_H_

#include "base/callback.h"

namespace ash::hid_detection {

// Manages detecting and automatically connecting to human interface devices.
class HidDetectionManager {
 public:
  virtual ~HidDetectionManager();

  // Invokes |callback| with a result indicating whether HID detection is
  // required or not. If both a keyboard and pointer are connected, HID
  // detection is not required, otherwise it is.
  virtual void GetIsHidDetectionRequired(
      base::OnceCallback<void(bool)> callback) = 0;

 protected:
  HidDetectionManager();
};

}  // namespace ash::hid_detection

#endif  // ASH_COMPONENTS_HID_DETECTION_HID_DETECTION_MANAGER_H_
