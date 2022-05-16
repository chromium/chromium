// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/hid_detection/hid_detection_manager.h"

#include "ash/constants/ash_features.h"

namespace ash::hid_detection {

HidDetectionManager::HidDetectionManager() {
  DCHECK(ash::features::IsOobeHidDetectionRevampEnabled());
}

HidDetectionManager::~HidDetectionManager() = default;

}  // namespace ash::hid_detection
