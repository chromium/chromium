// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_INPUT_DEVICE_SETTINGS_CONTROLLER_H_
#define ASH_PUBLIC_CPP_INPUT_DEVICE_SETTINGS_CONTROLLER_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "base/observer_list_types.h"

namespace ash {

// An interface, implemented by ash, which allows chrome to retrieve and update
// input device settings.
class ASH_PUBLIC_EXPORT InputDeviceSettingsController {
 public:
  using DeviceId = uint32_t;

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnKeyboardConnected(const mojom::Keyboard& keyboard) {}
    virtual void OnKeyboardDisconnected(const mojom::Keyboard& keyboard) {}
    virtual void OnKeyboardSettingsUpdated(const mojom::Keyboard& keyboard) {}

    virtual void OnTouchpadConnected(const mojom::Touchpad& touchpad) {}
    virtual void OnTouchpadDisconnected(const mojom::Touchpad& touchpad) {}
    virtual void OnTouchpadSettingsUpdated(const mojom::Touchpad& touchpad) {}

    virtual void OnMouseConnected(const mojom::Mouse& mouse) {}
    virtual void OnMouseDisconnected(const mojom::Mouse& mouse) {}
    virtual void OnMouseSettingsUpdated(const mojom::Mouse& mouse) {}

    virtual void OnPointingStickConnected(
        const mojom::PointingStick& pointing_stick) {}
    virtual void OnPointingStickDisconnected(
        const mojom::PointingStick& pointing_stick) {}
    virtual void OnPointingStickSettingsUpdated(
        const mojom::PointingStick& pointing_stick) {}
  };

  static InputDeviceSettingsController* Get();

  // Returns a list of currently connected keyboards and their settings.
  virtual std::vector<mojom::KeyboardPtr> GetConnectedKeyboards() = 0;
  // Returns a list of currently connected touchpads and their settings.
  virtual std::vector<mojom::TouchpadPtr> GetConnectedTouchpads() = 0;
  // Returns a list of currently connected mice and their settings.
  virtual std::vector<mojom::MousePtr> GetConnectedMice() = 0;
  // Returns a list of currently connected pointing sticks and their settings.
  virtual std::vector<mojom::PointingStickPtr> GetConnectedPointingSticks() = 0;

  // Configure the settings for keyboard of |id| with the provided |settings|.
  virtual void SetKeyboardSettings(DeviceId id,
                                   const mojom::KeyboardSettings& settings) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  InputDeviceSettingsController();
  virtual ~InputDeviceSettingsController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_INPUT_DEVICE_SETTINGS_CONTROLLER_H_
