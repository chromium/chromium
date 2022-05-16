// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/hid_detection/fake_hid_detection_manager.h"

namespace ash::hid_detection {

FakeHidDetectionManager::FakeHidDetectionManager() = default;

FakeHidDetectionManager::~FakeHidDetectionManager() = default;

bool FakeHidDetectionManager::HasPendingIsHidDetectionRequiredCallback() const {
  return !is_hid_detection_required_callback_.is_null();
}

void FakeHidDetectionManager::InvokePendingIsHidDetectionRequiredCallback(
    bool required) {
  std::move(is_hid_detection_required_callback_).Run(required);
}

void FakeHidDetectionManager::GetIsHidDetectionRequired(
    base::OnceCallback<void(bool)> callback) {
  is_hid_detection_required_callback_ = std::move(callback);
}

}  // namespace ash::hid_detection
