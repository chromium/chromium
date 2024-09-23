// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_INPUT_DEVICE_SETTINGS_CONTROLLER_H_
#define ASH_PUBLIC_CPP_INPUT_DEVICE_SETTINGS_CONTROLLER_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/scoped_singleton_resetter_for_test.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list_types.h"

class AccountId;

namespace ash {

// An interface, implemented by ash, which allows chrome to retrieve and update
// input device settings.
class ASH_PUBLIC_EXPORT InputDeviceSettingsController {
 public:
  using DeviceId = uint32_t;
  using ScopedResetterForTest =
      ScopedSingletonResetterForTest<InputDeviceSettingsController>;

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnKeyboardConnected(const mojom::Keyboard& keyboard) {}
    virtual void OnKeyboardDisconnected(const mojom::Keyboard& keyboard) {}
    virtual void OnKeyboardSettingsUpdated(const mojom::Keyboard& keyboard) {}
    virtual void OnKeyboardPoliciesUpdated(
        const mojom::KeyboardPolicies& keyboard_policies) {}

    virtual void OnTouchpadConnected(const mojom::Touchpad& touchpad) {}
    virtual void OnTouchpadDisconnected(const mojom::Touchpad& touchpad) {}
    virtual void OnTouchpadSettingsUpdated(const mojom::Touchpad& touchpad) {}

    virtual void OnMouseConnected(const mojom::Mouse& mouse) {}
    virtual void OnMouseDisconnected(const mojom::Mouse& mouse) {}
    virtual void OnMouseSettingsUpdated(const mojom::Mouse& mouse) {}
    virtual void OnMousePoliciesUpdated(
        const mojom::MousePolicies& keyboard_policies) {}

    virtual void OnPointingStickConnected(
        const mojom::PointingStick& pointing_stick) {}
    virtual void OnPointingStickDisconnected(
        const mojom::PointingStick& pointing_stick) {}
    virtual void OnPointingStickSettingsUpdated(
        const mojom::PointingStick& pointing_stick) {}

    virtual void OnGraphicsTabletConnected(
        const mojom::GraphicsTablet& graphics_tablet) {}
    virtual void OnGraphicsTabletDisconnected(
        const mojom::GraphicsTablet& graphics_tablet) {}
    virtual void OnGraphicsTabletSettingsUpdated(
        const mojom::GraphicsTablet& graphics_tablet) {}

    virtual void OnCustomizableMouseButtonPressed(const mojom::Mouse& mouse,
                                                  const mojom::Button& button) {
    }
    virtual void OnCustomizableTabletButtonPressed(
        const mojom::GraphicsTablet& mouse,
        const mojom::Button& button) {}
    virtual void OnCustomizablePenButtonPressed(
        const mojom::GraphicsTablet& mouse,
        const mojom::Button& button) {}

    virtual void OnCustomizableMouseObservingStarted(
        const mojom::Mouse& mouse) {}
    virtual void OnCustomizableMouseObservingStopped() {}

    virtual void OnKeyboardBatteryInfoChanged(const mojom::Keyboard& keyboard) {
    }
    virtual void OnGraphicsTabletBatteryInfoChanged(
        const mojom::GraphicsTablet& graphics_tablet) {}
    virtual void OnMouseBatteryInfoChanged(const mojom::Mouse& mouse) {}
    virtual void OnTouchpadBatteryInfoChanged(const mojom::Touchpad& touchpad) {
    }
    virtual void OnMouseCompanionAppInfoChanged(const mojom::Mouse& mouse) {}
    virtual void OnKeyboardCompanionAppInfoChanged(
        const mojom::Keyboard& keyboard) {}
    virtual void OnTouchpadCompanionAppInfoChanged(
        const mojom::Touchpad& touchpad) {}
    virtual void OnGraphicsTabletCompanionAppInfoChanged(
        const mojom::GraphicsTablet& graphics_tablet) {}
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
  // Returns a list of currently connected graphics tablets and their settings.
  virtual std::vector<mojom::GraphicsTabletPtr>
  GetConnectedGraphicsTablets() = 0;

  // Returns the settings of the keyboard with a device id of `id` or nullptr if
  // no such device exists.
  virtual const mojom::KeyboardSettings* GetKeyboardSettings(DeviceId id) = 0;
  // Returns the settings of the touchpad with a device id of `id` or nullptr if
  // no such device exists.
  virtual const mojom::TouchpadSettings* GetTouchpadSettings(DeviceId id) = 0;
  // Returns the settings of the mouse with a device id of `id` or nullptr if
  // no such device exists.
  virtual const mojom::MouseSettings* GetMouseSettings(DeviceId id) = 0;
  // Returns the settings of the pointing stick with a device id of `id` or
  // nullptr if no such device exists.
  virtual const mojom::PointingStickSettings* GetPointingStickSettings(
      DeviceId id) = 0;
  // Returns the settings of the pointing stick with a device id of `id` or
  // nullptr if no such device exists.
  virtual const mojom::GraphicsTabletSettings* GetGraphicsTabletSettings(
      DeviceId id) = 0;

  // Returns the keyboard that maps to the given id. Returns nullptr if no
  // keyboard exists.
  virtual const mojom::Keyboard* GetKeyboard(DeviceId id) = 0;
  // Returns the touchpad that maps to the given id. Returns nullptr if no
  // touchpad exists.
  virtual const mojom::Touchpad* GetTouchpad(DeviceId id) = 0;
  // Returns the mouse that maps to the given id. Returns nullptr if no
  // mouse exists.
  virtual const mojom::Mouse* GetMouse(DeviceId id) = 0;
  // Returns the pointing stick that maps to the given id. Returns nullptr if no
  // pointing stick exists.
  virtual const mojom::PointingStick* GetPointingStick(DeviceId id) = 0;
  // Returns the graphics tablet that maps to the given id. Returns nullptr if
  // no graphics tablet exists.
  virtual const mojom::GraphicsTablet* GetGraphicsTablet(DeviceId id) = 0;

  // Returns the current set of enterprise policies which control keyboard
  // settings.
  virtual const mojom::KeyboardPolicies& GetKeyboardPolicies() = 0;
  // Returns the current set of enterprise policies which control mouse
  // settings.
  virtual const mojom::MousePolicies& GetMousePolicies() = 0;

  // Restore the keyboard remappings to its default mappings for
  // keyboard of `id`.
  virtual void RestoreDefaultKeyboardRemappings(DeviceId id) = 0;
  // Configure the settings for keyboard of `id` with the provided
  // `settings`.
  virtual bool SetKeyboardSettings(DeviceId id,
                                   mojom::KeyboardSettingsPtr settings) = 0;
  // Configure the settings for touchpad of `id` with the provided `settings`.
  virtual bool SetTouchpadSettings(DeviceId id,
                                   mojom::TouchpadSettingsPtr settings) = 0;
  // Configure the settings for mouse of `id` with the provided `settings`.
  virtual bool SetMouseSettings(DeviceId id,
                                mojom::MouseSettingsPtr settings) = 0;
  // Configure the settings for pointing stick of `id` with the provided
  // `settings`.
  virtual bool SetPointingStickSettings(
      DeviceId id,
      mojom::PointingStickSettingsPtr settings) = 0;
  // Configure the settings for graphics tablet of `id` with the provided
  // `settings`.
  virtual bool SetGraphicsTabletSettings(
      DeviceId id,
      mojom::GraphicsTabletSettingsPtr settings) = 0;

  // Used to configure device settings on the login screen.
  virtual void OnLoginScreenFocusedPodChanged(const AccountId& account_id) = 0;

  // Used to start observing customizable buttons from the given `id`.
  virtual void StartObservingButtons(DeviceId id) = 0;
  // Stops observing customizable buttons from all devices.
  virtual void StopObservingButtons() = 0;

  // Called when a mouse which is currently being "observed" via
  // `StartObservingButtons` has pressed a customizable button.
  virtual void OnMouseButtonPressed(DeviceId device_id,
                                    const mojom::Button& button) = 0;
  // Called when a graphics tablet which is currently being "observed" via
  // `StartObservingButtons` has pressed a customizable button.
  virtual void OnGraphicsTabletButtonPressed(DeviceId device_id,
                                             const mojom::Button& button) = 0;

  // Returns the device image as a Data URL. Returns an empty string if
  // no device image exists.
  virtual void GetDeviceImageDataUrl(
      const std::string& device_key,
      base::OnceCallback<void(const std::optional<std::string>&)> callback) = 0;

  // Resets the tracking of device IDs associated with notification clicks.
  virtual void ResetNotificationDeviceTracking() = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  InputDeviceSettingsController();
  virtual ~InputDeviceSettingsController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_INPUT_DEVICE_SETTINGS_CONTROLLER_H_
