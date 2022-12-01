// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INITIALIZERS_FAKE_INPUT_DEVICE_TRACKER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INITIALIZERS_FAKE_INPUT_DEVICE_TRACKER_H_

#include <vector>

#include "ash/system/input_device_settings/initializers/input_device_tracker.h"
#include "ash/system/input_device_settings/input_device_settings_controller.h"

namespace ash {

// Data received by the InputDeviceTracker, used only for unit testing.
struct InputDeviceTrackerData {
  InputDeviceTrackerData(InputDeviceCategory category,
                         const base::StringPiece& device_key)
      : category(category), device_key(device_key) {}

  bool operator==(const InputDeviceTrackerData& other) const {
    return category == other.category && device_key == other.device_key;
  }

  InputDeviceCategory category;
  std::string device_key;
};

// Fake implementation of InputDeviceTracker to be used in unit tests.
class FakeInputDeviceTracker : public InputDeviceTracker {
 public:
  FakeInputDeviceTracker();
  FakeInputDeviceTracker(const FakeInputDeviceTracker&) = delete;
  FakeInputDeviceTracker& operator=(const FakeInputDeviceTracker&) = delete;
  ~FakeInputDeviceTracker() override;

  void RecordDeviceConnected(InputDeviceCategory category,
                             const base::StringPiece& device_key) override;

  // Checks whether a call with matching args was made to RecordDeviceConnected.
  bool WasDeviceRecorded(InputDeviceCategory category,
                         const base::StringPiece& device_key);

 private:
  std::vector<InputDeviceTrackerData> tracker_data_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INITIALIZERS_FAKE_INPUT_DEVICE_TRACKER_H_
