// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/hid_detection/bluetooth_hid_detector.h"
#include "ash/constants/ash_features.h"

namespace ash {
namespace hid_detection {

BluetoothHidDetector::BluetoothHidMetadata::BluetoothHidMetadata(
    std::string name,
    BluetoothHidType type)
    : name(std::move(name)), type(type) {}

BluetoothHidDetector::BluetoothHidMetadata::BluetoothHidMetadata(
    BluetoothHidMetadata&& other) {
  name = std::move(other.name);
  type = other.type;
}

BluetoothHidDetector::BluetoothHidMetadata&
BluetoothHidDetector::BluetoothHidMetadata::operator=(
    BluetoothHidMetadata&& other) {
  name = std::move(other.name);
  type = other.type;
  return *this;
}

BluetoothHidDetector::BluetoothHidMetadata::~BluetoothHidMetadata() = default;

BluetoothHidDetector::BluetoothHidDetectionStatus::BluetoothHidDetectionStatus(
    absl::optional<BluetoothHidDetector::BluetoothHidMetadata>
        current_pairing_device)
    : current_pairing_device(std::move(current_pairing_device)) {}

BluetoothHidDetector::BluetoothHidDetectionStatus::BluetoothHidDetectionStatus(
    BluetoothHidDetectionStatus&& other) {
  current_pairing_device = std::move(other.current_pairing_device);
}

BluetoothHidDetector::BluetoothHidDetectionStatus&
BluetoothHidDetector::BluetoothHidDetectionStatus::operator=(
    BluetoothHidDetectionStatus&& other) {
  current_pairing_device = std::move(other.current_pairing_device);
  return *this;
}

BluetoothHidDetector::BluetoothHidDetectionStatus::
    ~BluetoothHidDetectionStatus() = default;

BluetoothHidDetector::BluetoothHidDetector() {
  DCHECK(ash::features::IsBluetoothRevampEnabled());
}

BluetoothHidDetector::~BluetoothHidDetector() = default;

}  // namespace hid_detection
}  // namespace ash
