// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/initializers/fake_input_device_tracker.h"

#include "base/containers/contains.h"

namespace ash {

FakeInputDeviceTracker::FakeInputDeviceTracker() = default;
FakeInputDeviceTracker::~FakeInputDeviceTracker() = default;

void FakeInputDeviceTracker::RecordDeviceConnected(
    InputDeviceCategory category,
    const base::StringPiece& device_key) {
  tracker_data_.emplace_back(category, device_key);
}

bool FakeInputDeviceTracker::WasDeviceRecorded(
    InputDeviceCategory category,
    const base::StringPiece& device_key) {
  return base::Contains(tracker_data_,
                        InputDeviceTrackerData(category, device_key));
}

}  // namespace ash
