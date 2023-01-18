// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_TRACKER_IMPL_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_TRACKER_IMPL_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/input_device_settings/input_device_tracker.h"
#include "components/prefs/pref_member.h"

class PrefService;
class PrefRegistrySimple;

namespace ash {

// TODO(dpad@): Remove once transitioned to per-device settings.
class ASH_EXPORT InputDeviceTrackerImpl : public InputDeviceTracker {
 public:
  InputDeviceTrackerImpl();
  InputDeviceTrackerImpl(const InputDeviceTrackerImpl&) = delete;
  InputDeviceTrackerImpl& operator=(const InputDeviceTrackerImpl&) = delete;
  ~InputDeviceTrackerImpl() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* pref_registry);

  // InputDeviceTracker:
  void Init(PrefService* pref_service) override;
  void RecordDeviceConnected(InputDeviceCategory category,
                             const base::StringPiece& device_key) override;

 private:
  void ResetPrefMembers();

  std::unique_ptr<StringListPrefMember> keyboard_observed_devices_;
  std::unique_ptr<StringListPrefMember> mouse_observed_devices_;
  std::unique_ptr<StringListPrefMember> touchpad_observed_devices_;
  std::unique_ptr<StringListPrefMember> pointing_stick_observed_devices_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_TRACKER_IMPL_H_
