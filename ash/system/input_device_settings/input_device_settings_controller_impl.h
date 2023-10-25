// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_CONTROLLER_IMPL_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_CONTROLLER_IMPL_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_duplicate_id_finder.h"
#include "ash/system/input_device_settings/input_device_notifier.h"
#include "ash/system/input_device_settings/input_device_settings_metrics_manager.h"
#include "ash/system/input_device_settings/input_device_settings_policy_handler.h"
#include "ash/system/input_device_settings/pref_handlers/graphics_tablet_pref_handler.h"
#include "ash/system/input_device_settings/pref_handlers/keyboard_pref_handler.h"
#include "ash/system/input_device_settings/pref_handlers/mouse_pref_handler.h"
#include "ash/system/input_device_settings/pref_handlers/pointing_stick_pref_handler.h"
#include "ash/system/input_device_settings/pref_handlers/touchpad_pref_handler.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"

class AccountId;
class PrefChangeRegistrar;
class PrefRegistrySimple;

namespace ash {

// Controller to manage input device settings.
class ASH_EXPORT InputDeviceSettingsControllerImpl
    : public InputDeviceSettingsController,
      public SessionObserver {
 public:
  explicit InputDeviceSettingsControllerImpl(PrefService* local_state);
  InputDeviceSettingsControllerImpl(
      PrefService* local_state,
      std::unique_ptr<KeyboardPrefHandler> keyboard_pref_handler,
      std::unique_ptr<TouchpadPrefHandler> touchpad_pref_handler,
      std::unique_ptr<MousePrefHandler> mouse_pref_handler,
      std::unique_ptr<PointingStickPrefHandler> pointing_stick_pref_handler,
      std::unique_ptr<GraphicsTabletPrefHandler> graphics_tablet_pref_handler,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  InputDeviceSettingsControllerImpl(const InputDeviceSettingsControllerImpl&) =
      delete;
  InputDeviceSettingsControllerImpl& operator=(
      const InputDeviceSettingsControllerImpl&) = delete;
  ~InputDeviceSettingsControllerImpl() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* pref_registry);

  // InputDeviceSettingsController:
  std::vector<mojom::KeyboardPtr> GetConnectedKeyboards() override;
  std::vector<mojom::TouchpadPtr> GetConnectedTouchpads() override;
  std::vector<mojom::MousePtr> GetConnectedMice() override;
  std::vector<mojom::PointingStickPtr> GetConnectedPointingSticks() override;
  std::vector<mojom::GraphicsTabletPtr> GetConnectedGraphicsTablets() override;
  const mojom::KeyboardSettings* GetKeyboardSettings(DeviceId id) override;
  const mojom::MouseSettings* GetMouseSettings(DeviceId id) override;
  const mojom::TouchpadSettings* GetTouchpadSettings(DeviceId id) override;
  const mojom::PointingStickSettings* GetPointingStickSettings(
      DeviceId id) override;
  const mojom::GraphicsTabletSettings* GetGraphicsTabletSettings(
      DeviceId id) override;
  const mojom::KeyboardPolicies& GetKeyboardPolicies() override;
  const mojom::MousePolicies& GetMousePolicies() override;
  bool SetKeyboardSettings(DeviceId id,
                           mojom::KeyboardSettingsPtr settings) override;
  bool SetTouchpadSettings(DeviceId id,
                           mojom::TouchpadSettingsPtr settings) override;
  bool SetMouseSettings(DeviceId id, mojom::MouseSettingsPtr settings) override;
  bool SetPointingStickSettings(
      DeviceId id,
      mojom::PointingStickSettingsPtr settings) override;
  bool SetGraphicsTabletSettings(
      DeviceId id,
      mojom::GraphicsTabletSettingsPtr settings) override;
  void OnLoginScreenFocusedPodChanged(const AccountId& account_id) override;
  void StartObservingButtons(DeviceId id) override;
  void StopObservingButtons() override;
  void OnMouseButtonPressed(DeviceId device_id,
                            const mojom::Button& button) override;
  void OnGraphicsTabletButtonPressed(DeviceId device_id,
                                     const mojom::Button& button) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  void OnKeyboardListUpdated(std::vector<ui::KeyboardDevice> keyboards_to_add,
                             std::vector<DeviceId> keyboard_ids_to_remove);
  void OnTouchpadListUpdated(std::vector<ui::TouchpadDevice> touchpads_to_add,
                             std::vector<DeviceId> touchpad_ids_to_remove);
  void OnMouseListUpdated(std::vector<ui::InputDevice> mice_to_add,
                          std::vector<DeviceId> mouse_ids_to_remove);
  void OnPointingStickListUpdated(
      std::vector<ui::InputDevice> pointing_sticks_to_add,
      std::vector<DeviceId> pointing_stick_ids_to_remove);
  void OnGraphicsTabletListUpdated(
      std::vector<ui::InputDevice> graphics_tablets_to_add,
      std::vector<DeviceId> graphics_tablet_ids_to_remove);
  bool GetGeneralizedTopRowAreFKeys();
  void RestoreDefaultKeyboardRemappings(DeviceId id) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

 private:
  void Init();

  void ScheduleDeviceSettingsRefresh();
  void RefreshAllDeviceSettings();

  void RecordComboDeviceMetric(const mojom::Keyboard& keyboard);
  void RecordComboDeviceMetric(const mojom::Mouse& keyboard);

  void DispatchKeyboardConnected(DeviceId id);
  void DispatchKeyboardDisconnectedAndEraseFromList(DeviceId id);
  void DispatchKeyboardSettingsChanged(DeviceId id);

  void DispatchTouchpadConnected(DeviceId id);
  void DispatchTouchpadDisconnectedAndEraseFromList(DeviceId id);
  void DispatchTouchpadSettingsChanged(DeviceId id);

  void DispatchMouseConnected(DeviceId id);
  void DispatchMouseDisconnectedAndEraseFromList(DeviceId id);
  void DispatchMouseSettingsChanged(DeviceId id);

  void DispatchPointingStickConnected(DeviceId id);
  void DispatchPointingStickDisconnectedAndEraseFromList(DeviceId id);
  void DispatchPointingStickSettingsChanged(DeviceId id);

  void DispatchGraphicsTabletConnected(DeviceId id);
  void DispatchGraphicsTabletDisconnectedAndEraseFromList(DeviceId id);
  void DispatchGraphicsTabletSettingsChanged(DeviceId id);

  void DispatchCustomizableMouseButtonPressed(const mojom::Mouse& mouse,
                                              const mojom::Button& button);
  void DispatchCustomizableTabletButtonPressed(
      const mojom::GraphicsTablet& graphics_tablet,
      const mojom::Button& button);
  void DispatchCustomizablePenButtonPressed(
      const mojom::GraphicsTablet& graphics_tablet,
      const mojom::Button& button);

  void InitializePolicyHandler();
  void OnKeyboardPoliciesChanged();
  void OnMousePoliciesChanged();

  // Correctly initializes settings depending on whether we have an active
  // user session or not.
  void InitializeGraphicsTabletSettings(mojom::GraphicsTablet* graphics_tablet);
  void InitializeKeyboardSettings(mojom::Keyboard* keyboard);
  void InitializeMouseSettings(mojom::Mouse* mouse);
  void InitializePointingStickSettings(mojom::PointingStick* pointing_stick);
  void InitializeTouchpadSettings(mojom::Touchpad* touchpad);

  // Update the cached per-user keyboard settings on the login screen using the
  // most recently connected internal/external device (if applicable). This
  // needs to be done in the following cases in order to keep our settings up
  // to date:
  // - A device is connected/disconnected.
  // - A user makes an update to a device setting.
  // - The active pref service changes.
  void RefreshStoredLoginScreenGraphicsTabletSettings();
  void RefreshStoredLoginScreenKeyboardSettings();
  void RefreshStoredLoginScreenMouseSettings();
  void RefreshStoredLoginScreenPointingStickSettings();
  void RefreshStoredLoginScreenTouchpadSettings();

  // Refreshes all internal settings. Called whenever prefs are updated.
  void RefreshInternalPointingStickSettings();
  void RefreshInternalTouchpadSettings();

  // Updates the default settings based on the most recently connected device.
  // This is called whenever a device is connected/disconnected or if settings
  // are updated.
  void RefreshMouseDefaultSettings();
  void RefreshKeyboardDefaultSettings();
  void RefreshTouchpadDefaultSettings();

  // Refreshes all cached settings which includes defaults and login screen
  // settings.
  void RefreshCachedMouseSettings();
  void RefreshCachedKeyboardSettings();
  void RefreshCachedTouchpadSettings();

  // Get the mouse customization restriction. There are three different cases:
  // 1. If the mouse is customizable and there is no duplicate ids in the
  // keyboards, return kAllowCustomizations.
  // 2. If the mouse is customizable but there exists
  // duplicate ids in the keyboards, return kDisableKeyEventRewrites.
  // 3. If the mouse is not customizable, return kDisallowCustomizations.
  mojom::CustomizationRestriction GetMouseCustomizationRestriction(
      const ui::InputDevice& mouse);

  // Update the restriction for currently connected mice once a keyboard with
  // the same id connects to disable the key event rewrite for the mice.
  void ApplyCustomizationRestrictionFromKeyboard(DeviceId keyboard_id);

  mojom::Mouse* FindMouse(DeviceId id);
  mojom::Touchpad* FindTouchpad(DeviceId id);
  mojom::Keyboard* FindKeyboard(DeviceId id);
  mojom::GraphicsTablet* FindGraphicsTablet(DeviceId id);
  mojom::PointingStick* FindPointingStick(DeviceId id);

  base::ObserverList<Observer> observers_;

  std::unique_ptr<InputDeviceSettingsPolicyHandler> policy_handler_;

  raw_ptr<PrefService> local_state_ = nullptr;  // Not owned.

  std::unique_ptr<KeyboardPrefHandler> keyboard_pref_handler_;
  std::unique_ptr<TouchpadPrefHandler> touchpad_pref_handler_;
  std::unique_ptr<MousePrefHandler> mouse_pref_handler_;
  std::unique_ptr<PointingStickPrefHandler> pointing_stick_pref_handler_;
  std::unique_ptr<GraphicsTabletPrefHandler> graphics_tablet_pref_handler_;

  base::flat_map<DeviceId, mojom::KeyboardPtr> keyboards_;
  base::flat_map<DeviceId, mojom::TouchpadPtr> touchpads_;
  base::flat_map<DeviceId, mojom::MousePtr> mice_;
  base::flat_map<DeviceId, mojom::PointingStickPtr> pointing_sticks_;
  base::flat_map<DeviceId, mojom::GraphicsTabletPtr> graphics_tablets_;

  // Notifiers must be declared after the `flat_map` objects as the notifiers
  // depend on these objects.
  std::unique_ptr<InputDeviceNotifier<mojom::KeyboardPtr, ui::KeyboardDevice>>
      keyboard_notifier_;
  std::unique_ptr<InputDeviceNotifier<mojom::TouchpadPtr, ui::TouchpadDevice>>
      touchpad_notifier_;
  std::unique_ptr<InputDeviceNotifier<mojom::MousePtr, ui::InputDevice>>
      mouse_notifier_;
  std::unique_ptr<InputDeviceNotifier<mojom::PointingStickPtr, ui::InputDevice>>
      pointing_stick_notifier_;
  std::unique_ptr<
      InputDeviceNotifier<mojom::GraphicsTabletPtr, ui::InputDevice>>
      graphics_tablet_notifier_;
  std::unique_ptr<InputDeviceSettingsMetricsManager> metrics_manager_;

  std::unique_ptr<InputDeviceDuplicateIdFinder> duplicate_id_finder_;

  raw_ptr<PrefService> active_pref_service_ = nullptr;  // Not owned.
  absl::optional<AccountId> active_account_id_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Boolean which notes whether or not there is a settings update in progress.
  bool settings_refresh_pending_ = false;

  // Task runner where settings refreshes are scheduled to run.
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  base::WeakPtrFactory<InputDeviceSettingsControllerImpl> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_CONTROLLER_IMPL_H_
