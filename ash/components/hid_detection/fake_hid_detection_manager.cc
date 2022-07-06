// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/hid_detection/fake_hid_detection_manager.h"

namespace ash::hid_detection {
namespace {

bool IsInputMissing(const HidDetectionManager::InputMetadata& metadata) {
  return metadata.state == HidDetectionManager::InputState::kSearching ||
         metadata.state ==
             HidDetectionManager::InputState::kPairingViaBluetooth;
}

}  // namespace

FakeHidDetectionManager::FakeHidDetectionManager() = default;

FakeHidDetectionManager::~FakeHidDetectionManager() = default;

void FakeHidDetectionManager::SetHidStatusTouchscreenDetected(
    bool touchscreen_detected) {
  hid_detection_status_.touchscreen_detected = touchscreen_detected;
  if (!is_hid_detection_active_)
    return;

  NotifyHidDetectionStatusChanged();
}

void FakeHidDetectionManager::SetHidStatusPointerMetadata(
    InputMetadata metadata) {
  hid_detection_status_.pointer_metadata = metadata;
  if (!is_hid_detection_active_)
    return;

  NotifyHidDetectionStatusChanged();
}

void FakeHidDetectionManager::SetHidStatusKeyboardMetadata(
    InputMetadata metadata) {
  hid_detection_status_.keyboard_metadata = metadata;
  if (!is_hid_detection_active_)
    return;

  NotifyHidDetectionStatusChanged();
}

void FakeHidDetectionManager::GetIsHidDetectionRequired(
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(
      IsInputMissing(hid_detection_status_.pointer_metadata) ||
      IsInputMissing(hid_detection_status_.keyboard_metadata));
}

void FakeHidDetectionManager::PerformStartHidDetection() {
  DCHECK(!is_hid_detection_active_);
  is_hid_detection_active_ = true;
  NotifyHidDetectionStatusChanged();
}

void FakeHidDetectionManager::PerformStopHidDetection() {
  DCHECK(is_hid_detection_active_);
  is_hid_detection_active_ = false;
}

HidDetectionManager::HidDetectionStatus
FakeHidDetectionManager::ComputeHidDetectionStatus() const {
  return hid_detection_status_;
}

}  // namespace ash::hid_detection
