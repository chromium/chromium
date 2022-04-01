// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/hid_detection/bluetooth_hid_detector.h"
#include "ash/constants/ash_features.h"

namespace ash {
namespace hid_detection {

BluetoothHidDetector::BluetoothHidDetector() {
  DCHECK(ash::features::IsBluetoothRevampEnabled());
}

BluetoothHidDetector::~BluetoothHidDetector() = default;

}  // namespace hid_detection
}  // namespace ash
