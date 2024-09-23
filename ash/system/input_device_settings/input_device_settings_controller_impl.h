// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_CONTROLLER_IMPL_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_CONTROLLER_IMPL_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/peripherals_app_delegate.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/device_image.h"
#include "ash/system/input_device_settings/input_device_duplicate_id_finder.h"
#include "ash/system/input_device_settings/input_device_notifier.h"
#include "ash/system/input_device_settings/input_device_settings_metadata_manager.h"
#include "ash/system/input_device_settings/input_device_settings_metrics_manager.h"
#include "ash/system/input_device_settings/input_device_settings_notification_controller.h"
#include "ash/system/input_device_settings/input_device_settings_policy_handler.h"
#include "ash/system/input_device_settings/modifier_split_bypass_checker.h"
#include "ash/system/input_device_settings/pref_handlers/graphics_tablet_pref_handler.h"
#include "ash/system/input_device_settings/pref_handlers/keyboard_pref_handler.h"
#include "ash/system/input_device_settings/pref_handlers/mouse_pref_handler.h"
#include "ash/system/input_device_settings/pref_handlers/pointing_stick_pref_handler.h"
#include "ash/system/input_device_settings/pref_handlers/touchpad_pref_handler.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/message_center/message_center_observer.h"

class AccountId;
class PrefChangeRegistrar;
class PrefRegistrySimple;

namespace ash {

// Controller to manage input device settings.
class ASH_EXPORT InputDeviceSettingsControllerImpl
    : public InputDeviceSettingsController,
      public input_method::InputMethodManager::Observer,
      public SessionObserver,
      public device::BluetoothAdapter::Observer,
      public LoginDataDispatcher::Observer,
      public apps::AppRegistryCache::Observer,
      public message_center::MessageCenterObserver {
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

  // Refreshes keyboard info and settings. To be used when the feature is first
  // forcibly enabled.
  void ForceKeyboardSettingRefreshWhenFeatureEnabled();

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
  const mojom::Keyboard* GetKeyboard(DeviceId id) override;
  const mojom::Mouse* GetMouse(DeviceId id) override;
  const mojom::Touchpad* GetTouchpad(DeviceId id) override;
  const mojom::PointingStick* GetPointingStick(DeviceId id) override;
  const mojom::GraphicsTablet* GetGraphicsTablet(DeviceId id) override;
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
  void GetDeviceImageDataUrl(
      const std::string& device_key,
      base::OnceCallback<void(const std::optional<std::string>&)> callback)
      override;
  void ResetNotificationDeviceTracking() override;

  void AddObserver(InputDeviceSettingsController::Observer* observer) override;
  void RemoveObserver(
      InputDeviceSettingsController::Observer* observer) override;

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
  const mojom::Keyboard* GetGeneralizedKeyboard();
  bool GetGeneralizedTopRowAreFKeys();
  void RestoreDefaultKeyboardRemappings(DeviceId id) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  void OnSessionStateChanged(session_manager::SessionState state) override;

  // input_method::InputMethodManager::Observer:
  void InputMethodChanged(input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

  // device::BluetoothAdapter::Observer:
  void DeviceBatteryChanged(device::BluetoothAdapter* adapter,
                            device::BluetoothDevice* device,
                            device::BluetoothDevice::BatteryType type) override;

  // LoginDataDispatcher::Observer:
  void OnOobeDialogStateChanged(OobeDialogState state) override;

  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  // message_center::MessageCenterObserver:
  void OnNotificationClicked(
      const std::string& notification_id,
      const std::optional<int>& button_index,
      const std::optional<std::u16string>& reply) override;

  InputDeviceDuplicateIdFinder& duplicate_id_finder() {
    CHECK(duplicate_id_finder_);
    return *duplicate_id_finder_;
  }

  void SetPeripheralsAppDelegate(PeripheralsAppDelegate* delegate);

  void AddWelcomeNotificationDeviceKeyForTesting(
      const std::string& device_key) {
    welcome_notification_clicked_device_keys_.insert(device_key);
  }

 private:
  void Init();

  void ScheduleDeviceSettingsRefresh();
  void RefreshAllDeviceSettings();
  void ShowFirstTimeConnectedNotifications();

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

  void DispatchKeyboardBatteryInfoChanged(DeviceId id);
  void DispatchGraphicsTabletBatteryInfoChanged(DeviceId id);
  void DispatchMouseBatteryInfoChanged(DeviceId id);
  void DispatchTouchpadBatteryInfoChanged(DeviceId id);

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

  // Refreshes the settings for the device to match the default settings.
  void ForceInitializeDefaultChromeOSKeyboardSettings();
  void ForceInitializeDefaultNonChromeOSKeyboardSettings();
  void ForceInitializeDefaultSplitModifierKeyboardSettings();
  void ForceInitializeDefaultTouchpadSettings();
  void ForceInitializeDefaultMouseSettings();

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

  // Refreshes all companion app info for connected devices.
  void RefreshCompanionAppInfoForConnectedDevices();
  void OnCompanionAppInfoReceived(
      DeviceId id,
      const std::string& device_key,
      const std::optional<mojom::CompanionAppInfo>& info);

  void DispatchMouseCompanionAppInfoChanged(const mojom::Mouse& mouse);
  void DispatchKeyboardCompanionAppInfoChanged(const mojom::Keyboard& keyboard);
  void DispatchTouchpadCompanionAppInfoChanged(const mojom::Touchpad& touchpad);
  void DispatchGraphicsTabletCompanionAppInfoChanged(
      const mojom::GraphicsTablet& graphics_tablet);

  // Get the mouse customization restriction based on the mouse metadata. Return
  // kDisableKeyEventRewrites by default if there is no mouse metadata.
  mojom::CustomizationRestriction GetMouseCustomizationRestriction(
      const ui::InputDevice& mouse);

  // Get the graphics tablet customization restriction based on the graphics
  // tablet metadata. Return kAllowCustomizations by default if there is no
  // graphics tablet metadata.
  mojom::CustomizationRestriction GetGraphicsTabletCustomizationRestriction(
      const ui::InputDevice& graphics_tablet);

  // Refreshes the key display values within the button remappings to match the
  // current input method.
  void RefreshKeyDisplay();

  // Refresh meta and modifier keys when they potentially changed due to flags
  // being enabled.
  void RefreshMetaAndModifierKeys();

  // Get the mouse button config based on the mouse metadata. Return
  // kDefault by default if there is no mouse metadata.
  mojom::MouseButtonConfig GetMouseButtonConfig(const ui::InputDevice& mouse);

  // Get the graphics tablet button config based on the tablet metadata. Return
  // kDefault by default if there is no metadata.
  mojom::GraphicsTabletButtonConfig GetGraphicsTabletButtonConfig(
      const ui::InputDevice& graphics_tablet);

  // Determines whether a device image should be fetched.
  // Returns true if the following conditions are met:
  //  1. The welcome experience feature is enabled.
  //  2. An active account ID is available.
  //  3. An active preference service is available.
  bool ShouldFetchDeviceImage();

  // Initiates the process of fetching an image associated with a specific
  // input device.
  void GetDeviceImage(const std::string& device_key, DeviceId id);

  // Callback function triggered when a device image has been downloaded.
  // The DeviceId is used to identify the type of input device the image is
  // associated with.
  void OnDeviceNotificationImageDownloaded(DeviceId id,
                                           const DeviceImage& device_image);

  // Callback function triggered when a device image to be displayed in the
  // Settings UI has been downloaded.
  void OnDeviceImageForSettingsDownloaded(
      base::OnceCallback<void(const std::optional<std::string>&)> callback,
      const DeviceImage& device_image);

  mojom::Mouse* FindMouse(DeviceId id);
  mojom::Touchpad* FindTouchpad(DeviceId id);
  mojom::Keyboard* FindKeyboard(DeviceId id);
  mojom::GraphicsTablet* FindGraphicsTablet(DeviceId id);
  mojom::PointingStick* FindPointingStick(DeviceId id);

  void InitializeOnBluetoothReady(
      scoped_refptr<device::BluetoothAdapter> adapter);

  bool IsOobe() const;
  void RefreshBatteryInfoForConnectedDevices();

  base::ObserverList<InputDeviceSettingsController::Observer> observers_;

  std::unique_ptr<InputDeviceSettingsPolicyHandler> policy_handler_;

  raw_ptr<PrefService> local_state_ = nullptr;  // Not owned.

  std::unique_ptr<ModifierSplitBypassChecker> modifier_split_bypass_checker_;

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
  // A map that stores associations between package IDs (e.g.,
  // "com.example.app") and the corresponding device IDs where the package is
  // installed or used. This map is used to track installations and removals for
  // devies with companion apps.
  base::flat_map<std::string, DeviceId> package_id_to_device_id_map_;
  // A set to track unique device keys of devices where the user clicked on the
  // welcome notification displayed during initial device connection.
  // This information is used for recording metrics:
  // - If a user modifies device settings AFTER clicking the notification,
  //   the presence of the device key in this set indicates the notification
  //   was seen before the setting change.
  // - This helps measure the impact of the welcome notification on user
  // behavior.
  base::flat_set<std::string> welcome_notification_clicked_device_keys_;

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

  std::unique_ptr<InputDeviceSettingsNotificationController>
      notification_controller_;

  std::unique_ptr<InputDeviceSettingsMetadataManager> metadata_manager_;
  // Observe bluetooth device change events.
  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;

  raw_ptr<PrefService> active_pref_service_ = nullptr;  // Not owned.
  raw_ptr<PeripheralsAppDelegate> delegate_ = nullptr;  // Not owned.
  std::optional<AccountId> active_account_id_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Boolean which notes whether or not there is a settings update in progress.
  bool settings_refresh_pending_ = false;

  OobeDialogState oobe_state_ = OobeDialogState::HIDDEN;
  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  session_manager::SessionState last_session_ =
      session_manager::SessionState::UNKNOWN;

  // Task runner where settings refreshes are scheduled to run.
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  base::WeakPtrFactory<InputDeviceSettingsControllerImpl> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_CONTROLLER_IMPL_H_
