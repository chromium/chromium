// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_MOCK_INPUT_DEVICE_SETTINGS_CONTROLLER_H_
#define ASH_PUBLIC_CPP_TEST_MOCK_INPUT_DEVICE_SETTINGS_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class ASH_PUBLIC_EXPORT MockInputDeviceSettingsController
    : public InputDeviceSettingsController {
 public:
  MockInputDeviceSettingsController();
  MockInputDeviceSettingsController(const MockInputDeviceSettingsController&) =
      delete;
  MockInputDeviceSettingsController& operator=(
      const MockInputDeviceSettingsController&) = delete;
  ~MockInputDeviceSettingsController() override;

  // InputDeviceSettingsController:
  MOCK_METHOD(std::vector<mojom::KeyboardPtr>,
              GetConnectedKeyboards,
              (),
              (override));
  MOCK_METHOD(std::vector<mojom::TouchpadPtr>,
              GetConnectedTouchpads,
              (),
              (override));
  MOCK_METHOD(std::vector<mojom::MousePtr>, GetConnectedMice, (), (override));
  MOCK_METHOD(std::vector<mojom::PointingStickPtr>,
              GetConnectedPointingSticks,
              (),
              (override));
  MOCK_METHOD(std::vector<mojom::GraphicsTabletPtr>,
              GetConnectedGraphicsTablets,
              (),
              (override));
  MOCK_METHOD(const mojom::KeyboardSettings*,
              GetKeyboardSettings,
              (DeviceId id),
              (override));
  MOCK_METHOD(const mojom::MouseSettings*,
              GetMouseSettings,
              (DeviceId id),
              (override));
  MOCK_METHOD(const mojom::TouchpadSettings*,
              GetTouchpadSettings,
              (DeviceId id),
              (override));
  MOCK_METHOD(const mojom::PointingStickSettings*,
              GetPointingStickSettings,
              (DeviceId id),
              (override));
  MOCK_METHOD(const mojom::GraphicsTabletSettings*,
              GetGraphicsTabletSettings,
              (DeviceId id),
              (override));
  MOCK_METHOD(const mojom::Keyboard*, GetKeyboard, (DeviceId id), (override));
  MOCK_METHOD(const mojom::Mouse*, GetMouse, (DeviceId id), (override));
  MOCK_METHOD(const mojom::Touchpad*, GetTouchpad, (DeviceId id), (override));
  MOCK_METHOD(const mojom::PointingStick*,
              GetPointingStick,
              (DeviceId id),
              (override));
  MOCK_METHOD(const mojom::GraphicsTablet*,
              GetGraphicsTablet,
              (DeviceId id),
              (override));
  MOCK_METHOD(const mojom::KeyboardPolicies&,
              GetKeyboardPolicies,
              (),
              (override));
  MOCK_METHOD(const mojom::MousePolicies&, GetMousePolicies, (), (override));
  MOCK_METHOD(bool,
              SetKeyboardSettings,
              (DeviceId id, mojom::KeyboardSettingsPtr settings),
              (override));
  MOCK_METHOD(void,
              RestoreDefaultKeyboardRemappings,
              (DeviceId id),
              (override));
  MOCK_METHOD(bool,
              SetTouchpadSettings,
              (DeviceId id, mojom::TouchpadSettingsPtr settings),
              (override));
  MOCK_METHOD(bool,
              SetMouseSettings,
              (DeviceId id, mojom::MouseSettingsPtr settings),
              (override));
  MOCK_METHOD(bool,
              SetPointingStickSettings,
              (DeviceId id, mojom::PointingStickSettingsPtr settings),
              (override));
  MOCK_METHOD(bool,
              SetGraphicsTabletSettings,
              (DeviceId id, mojom::GraphicsTabletSettingsPtr settings),
              (override));
  MOCK_METHOD(void,
              OnLoginScreenFocusedPodChanged,
              (const AccountId&),
              (override));
  MOCK_METHOD(void, StartObservingButtons, (DeviceId id), (override));
  MOCK_METHOD(void, StopObservingButtons, (), (override));
  MOCK_METHOD(void,
              OnMouseButtonPressed,
              (DeviceId device_id, const mojom::Button& button),
              (override));
  MOCK_METHOD(void,
              OnGraphicsTabletButtonPressed,
              (DeviceId device_id, const mojom::Button& button),
              (override));
  MOCK_METHOD(
      void,
      GetDeviceImageDataUrl,
      (const std::string& device_key,
       base::OnceCallback<void(const std::optional<std::string>&)> callback),
      (override));
  MOCK_METHOD(void, ResetNotificationDeviceTracking, (), (override));
  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_MOCK_INPUT_DEVICE_SETTINGS_CONTROLLER_H_
