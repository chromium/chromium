// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/initializers/input_device_tracker_impl.h"

#include "ash/system/input_device_settings/input_device_settings_controller.h"
#include "base/notreached.h"
#include "components/prefs/pref_service.h"

namespace ash {

InputDeviceTrackerImpl::InputDeviceTrackerImpl(PrefService* pref_service)
    : pref_service_(pref_service) {}
InputDeviceTrackerImpl::~InputDeviceTrackerImpl() = default;

// TODO(dpad@): Implement storing of observed device pref.
void InputDeviceTrackerImpl::RecordDeviceConnected(
    InputDeviceCategory category,
    const base::StringPiece& device_key) {
  NOTIMPLEMENTED();
}

}  // namespace ash
