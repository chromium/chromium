// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INITIALIZERS_INPUT_DEVICE_TRACKER_IMPL_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INITIALIZERS_INPUT_DEVICE_TRACKER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/system/input_device_settings/initializers/input_device_tracker.h"
#include "ash/system/input_device_settings/input_device_settings_controller.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_service.h"

namespace ash {

// TODO(dpad@): Remove once transitioned to per-device settings.
class ASH_EXPORT InputDeviceTrackerImpl : public InputDeviceTracker {
 public:
  explicit InputDeviceTrackerImpl(PrefService* pref_service);
  InputDeviceTrackerImpl(const InputDeviceTrackerImpl&) = delete;
  InputDeviceTrackerImpl& operator=(const InputDeviceTrackerImpl&) = delete;
  ~InputDeviceTrackerImpl() override;

  void RecordDeviceConnected(InputDeviceCategory category,
                             const base::StringPiece& device_key) override;

 private:
  base::raw_ptr<PrefService> pref_service_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INITIALIZERS_INPUT_DEVICE_TRACKER_IMPL_H_
