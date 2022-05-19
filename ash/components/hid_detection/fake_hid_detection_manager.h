// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_HID_DETECTION_FAKE_HID_DETECTION_MANAGER_H_
#define ASH_COMPONENTS_HID_DETECTION_FAKE_HID_DETECTION_MANAGER_H_

#include "ash/components/hid_detection/hid_detection_manager.h"

#include "base/callback.h"

namespace ash::hid_detection {

class FakeHidDetectionManager : public HidDetectionManager {
 public:
  FakeHidDetectionManager();
  ~FakeHidDetectionManager() override;

  bool HasPendingIsHidDetectionRequiredCallback() const;
  void InvokePendingIsHidDetectionRequiredCallback(bool required);

  // Mocks the HID detection status being updated.
  void SetHidDetectionStatus(HidDetectionManager::HidDetectionStatus status);

  bool is_hid_detection_active() const { return is_hid_detection_active_; }

 private:
  // HidDetectionManager:
  void GetIsHidDetectionRequired(
      base::OnceCallback<void(bool)> callback) override;
  void PerformStartHidDetection() override;
  void PerformStopHidDetection() override;
  HidDetectionManager::HidDetectionStatus ComputeHidDetectionStatus()
      const override;

  base::OnceCallback<void(bool)> is_hid_detection_required_callback_;
  bool is_hid_detection_active_ = false;
  HidDetectionManager::HidDetectionStatus hid_detection_status_;
};

}  // namespace ash::hid_detection

#endif  // ASH_COMPONENTS_HID_DETECTION_FAKE_HID_DETECTION_MANAGER_H_
