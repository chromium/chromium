// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_ACCELERATOR_CONFIGURATION_PROVIDER_H_
#define ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_ACCELERATOR_CONFIGURATION_PROVIDER_H_

#include <map>
#include <memory>

#include "ash/accelerators/accelerator_alias_converter.h"
#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/public/cpp/accelerator_configuration.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/accelerator_configuration.mojom-forward.h"
#include "ash/public/mojom/accelerator_info.mojom-forward.h"
#include "ash/webui/shortcut_customization_ui/backend/accelerator_layout_table.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/crosapi/cpp/lacros_startup_state.h"
#include "components/prefs/pref_member.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/accelerators/accelerator_map.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/devices/keyboard_device.h"

class PrefService;

namespace ash::shortcut_ui {

class AcceleratorConfigurationProvider
    : public shortcut_customization::mojom::AcceleratorConfigurationProvider,
      public ui::InputDeviceEventObserver,
      public input_method::InputMethodManager::Observer,
      public InputDeviceSettingsController::Observer {
 public:
  using ActionIdToAcceleratorsInfoMap =
      base::flat_map<AcceleratorActionId,
                     std::vector<mojom::AcceleratorInfoPtr>>;
  using AcceleratorConfigurationMap =
      base::flat_map<mojom::AcceleratorSource, ActionIdToAcceleratorsInfoMap>;
  using AcceleratorSourceMap = base::flat_map<
      mojom::AcceleratorSource,
      std::map<AcceleratorActionId, std::vector<ui::Accelerator>>>;

  // This Observer class is used to observe changes to the accelerator config.
  class AcceleratorsUpdatedObserver : public base::CheckedObserver {
   public:
    ~AcceleratorsUpdatedObserver() override = default;
    virtual void OnAcceleratorsUpdated(AcceleratorConfigurationMap config) = 0;
  };

  explicit AcceleratorConfigurationProvider(PrefService* pref_service);
  AcceleratorConfigurationProvider(const AcceleratorConfigurationProvider&) =
      delete;
  AcceleratorConfigurationProvider& operator=(
      const AcceleratorConfigurationProvider&) = delete;
  ~AcceleratorConfigurationProvider() override;

  // Observer for non-mojo classes
  void AddObserver(AcceleratorsUpdatedObserver* observer);
  void RemoveObserver(AcceleratorsUpdatedObserver* observer);

  // shortcut_customization::mojom::AcceleratorConfigurationProvider:
  void IsMutable(ash::mojom::AcceleratorSource source,
                 IsMutableCallback callback) override;
  void HasLauncherButton(HasLauncherButtonCallback callback) override;
  void GetConflictAccelerator(mojom::AcceleratorSource source,
                              uint32_t action_id,
                              const ui::Accelerator& accelerator,
                              GetConflictAcceleratorCallback callback) override;
  void GetDefaultAcceleratorsForId(
      uint32_t action_id,
      GetDefaultAcceleratorsForIdCallback callback) override;
  void GetAccelerators(GetAcceleratorsCallback callback) override;
  void AddObserver(mojo::PendingRemote<
                   shortcut_customization::mojom::AcceleratorsUpdatedObserver>
                       observer) override;
  void GetAcceleratorLayoutInfos(
      GetAcceleratorLayoutInfosCallback callback) override;
  void PreventProcessingAccelerators(
      bool prevent_processing_accelerators,
      PreventProcessingAcceleratorsCallback callback) override;
  void AddAccelerator(mojom::AcceleratorSource source,
                      uint32_t action_id,
                      const ui::Accelerator& accelerator,
                      AddAcceleratorCallback callback) override;
  void RemoveAccelerator(mojom::AcceleratorSource source,
                         uint32_t action_id,
                         const ui::Accelerator& accelerator,
                         RemoveAcceleratorCallback callback) override;
  void ReplaceAccelerator(mojom::AcceleratorSource source,
                          uint32_t action_id,
                          const ui::Accelerator& old_accelerator,
                          const ui::Accelerator& new_accelerator,
                          ReplaceAcceleratorCallback callback) override;
  void RestoreDefault(mojom::AcceleratorSource source,
                      uint32_t action_id,
                      RestoreDefaultCallback callback) override;
  void RestoreAllDefaults(RestoreAllDefaultsCallback callback) override;

  // ui::InputDeviceEventObserver:
  void OnInputDeviceConfigurationChanged(uint8_t input_device_types) override;

  // InputMethodManager::Observer:
  void InputMethodChanged(input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

  // InputDeviceSettingsController::Observer:
  void OnKeyboardConnected(const mojom::Keyboard& keyboard) override;
  void OnKeyboardDisconnected(const mojom::Keyboard& keyboard) override;
  void OnKeyboardSettingsUpdated(const mojom::Keyboard& keyboard) override;

  AcceleratorConfigurationMap GetAcceleratorConfig();
  std::vector<mojom::AcceleratorLayoutInfoPtr> GetAcceleratorLayoutInfos()
      const;

  void BindInterface(
      mojo::PendingReceiver<
          shortcut_customization::mojom::AcceleratorConfigurationProvider>
          receiver);

  void InitializeNonConfigurableAccelerators(NonConfigurableActionsMap mapping);

  const NonConfigurableActionsMap& GetNonConfigurableAcceleratorsForTesting() {
    return non_configurable_actions_mapping_;
  }

  mojom::AcceleratorInfoPtr CreateTextAcceleratorInfo(
      const NonConfigurableAcceleratorDetails& details) const;

  mojom::TextAcceleratorPropertiesPtr CreateTextAcceleratorProperties(
      const NonConfigurableAcceleratorDetails& details) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(AcceleratorConfigurationProviderTest,
                           SetLayoutDetailsMapForTesting);
  friend class AcceleratorConfigurationProviderTest;
  using NonConfigAcceleratorActionMap = ui::AcceleratorMap<AcceleratorActionId>;

  // Accelerator that is queued to be added. This should only be set if the user
  // has attempted to add an accelerator that conflicts with a overridable
  // existing accelerator.
  struct PendingAccelerator {
    PendingAccelerator(const ui::Accelerator& accelerator,
                       mojom::AcceleratorSource source,
                       AcceleratorActionId action)
        : accelerator(accelerator), source(source), action(action) {}

    ui::Accelerator accelerator;
    mojom::AcceleratorSource source;
    AcceleratorActionId action;
  };

  void OnAcceleratorsUpdated(mojom::AcceleratorSource source,
                             const ActionIdToAcceleratorsMap& mapping);

  void UpdateKeyboards();

  AcceleratorConfigurationMap CreateConfigurationMap();

  void PopulateAshAcceleratorConfig(
      AcceleratorConfigurationMap& accelerator_config_output);

  void PopulateAmbientAcceleratorConfig(
      AcceleratorConfigurationMap& accelerator_config_output);

  void NotifyAcceleratorsUpdated();

  void CreateAndAppendAliasedAccelerators(
      const ui::Accelerator& accelerator,
      bool locked,
      mojom::AcceleratorType type,
      mojom::AcceleratorState state,
      std::vector<mojom::AcceleratorInfoPtr>& output);

  // Returns a non-null value if there was an error with pre-processing the
  // accelerator to be added.
  absl::optional<shortcut_customization::mojom::AcceleratorResultDataPtr>
  PreprocessAddAccelerator(mojom::AcceleratorSource source,
                           AcceleratorActionId action_id,
                           const ui::Accelerator& accelerator);

  void SetLayoutDetailsMapForTesting(
      const std::vector<AcceleratorLayoutDetails>& layouts);

  // Set only for testing purposes, this will ignore the default layouts.
  bool ignore_layouts_for_testing_ = false;

  std::vector<mojom::AcceleratorLayoutInfoPtr> layout_infos_;
  // A lookup map that provides a way to lookup a shortcut's layout details.
  // This is initialized only once along with `layout_infos_`.
  base::flat_map<std::string, AcceleratorLayoutDetails>
      accelerator_layout_lookup_;

  std::unique_ptr<BooleanPrefMember> send_function_keys_pref_;

  AcceleratorConfigurationMap cached_configuration_;

  AcceleratorSourceMap accelerators_mapping_;

  // Stores all connected keyboards.
  std::vector<ui::KeyboardDevice> connected_keyboards_;

  NonConfigurableActionsMap non_configurable_actions_mapping_;

  AcceleratorAliasConverter accelerator_alias_converter_;

  mojo::Receiver<
      shortcut_customization::mojom::AcceleratorConfigurationProvider>
      receiver_{this};

  raw_ptr<AshAcceleratorConfiguration, ExperimentalAsh>
      ash_accelerator_configuration_;

  // One accelerator action ID can potentially have multiple accelerators
  // associated with it.
  ActionIdToAcceleratorsMap id_to_non_configurable_accelerators_;

  // A map from accelerators to AcceleratorActions, used as a reverse lookup for
  // standard non-configurable accelerators.
  NonConfigAcceleratorActionMap non_configurable_accelerator_to_id_;

  std::unique_ptr<PendingAccelerator> pending_accelerator_;

  mojo::Remote<shortcut_customization::mojom::AcceleratorsUpdatedObserver>
      accelerators_updated_mojo_observer_;
  base::ObserverList<AcceleratorsUpdatedObserver>
      accelerators_updated_observers_;

  base::WeakPtrFactory<AcceleratorConfigurationProvider> weak_ptr_factory_{
      this};
};

std::u16string GetKeyDisplay(ui::KeyboardCode key_code);

}  // namespace ash::shortcut_ui

#endif  // ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_ACCELERATOR_CONFIGURATION_PROVIDER_H_
