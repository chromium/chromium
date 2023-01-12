// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_ACCELERATOR_CONFIGURATION_PROVIDER_H_
#define ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_ACCELERATOR_CONFIGURATION_PROVIDER_H_

#include <map>

#include "ash/accelerators/accelerator_alias_converter.h"
#include "ash/public/cpp/accelerator_configuration.h"
#include "ash/public/mojom/accelerator_keys.mojom.h"
#include "ash/webui/shortcut_customization_ui/backend/accelerator_layout_table.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/chromeos/events/keyboard_capability.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace ash {
// Gets the parts of the string that don't contain replacements.
// Ex: "Press and " -> ["Press ", " and "]
std::vector<std::u16string> SplitStringOnOffsets(
    const std::u16string& input,
    const std::vector<size_t>& offsets);

// Creates text accelerator parts needed to properly display kText accelerators
// in the UI. Uses the list of offsets which must be sorted and contains the
// start points of our replacements to place the |plain_text_parts| and
// |replacement_parts| in the correct order.
std::vector<mojom::TextAcceleratorPartPtr> GenerateTextAcceleratorParts(
    const std::vector<std::u16string>& plain_text_parts,
    const std::vector<TextAcceleratorPart>& replacement_parts,
    const std::vector<size_t>& offsets,
    size_t str_size);

namespace shortcut_ui {

class AcceleratorConfigurationProvider
    : public shortcut_customization::mojom::AcceleratorConfigurationProvider,
      public ui::InputDeviceEventObserver,
      public input_method::InputMethodManager::Observer,
      public ui::KeyboardCapability::Observer {
 public:
  using ActionIdToAcceleratorsInfoMap =
      base::flat_map<AcceleratorActionId,
                     std::vector<mojom::AcceleratorInfoPtr>>;
  using AcceleratorConfigurationMap =
      base::flat_map<mojom::AcceleratorSource, ActionIdToAcceleratorsInfoMap>;
  using AcceleratorSourceMap = base::flat_map<
      mojom::AcceleratorSource,
      std::map<AcceleratorActionId, std::vector<ui::Accelerator>>>;

  AcceleratorConfigurationProvider();
  AcceleratorConfigurationProvider(const AcceleratorConfigurationProvider&) =
      delete;
  AcceleratorConfigurationProvider& operator=(
      const AcceleratorConfigurationProvider&) = delete;
  ~AcceleratorConfigurationProvider() override;

  // shortcut_customization::mojom::AcceleratorConfigurationProvider:
  void IsMutable(ash::mojom::AcceleratorSource source,
                 IsMutableCallback callback) override;
  void GetAccelerators(GetAcceleratorsCallback callback) override;
  void AddObserver(mojo::PendingRemote<
                   shortcut_customization::mojom::AcceleratorsUpdatedObserver>
                       observer) override;
  void GetAcceleratorLayoutInfos(
      GetAcceleratorLayoutInfosCallback callback) override;

  // ui::InputDeviceEventObserver:
  void OnInputDeviceConfigurationChanged(uint8_t input_device_types) override;

  // InputMethodManager::Observer:
  void InputMethodChanged(input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

  // ui::KeyboardCapability::Observer:
  void OnTopRowKeysAreFKeysChanged() override;

  void BindInterface(
      mojo::PendingReceiver<
          shortcut_customization::mojom::AcceleratorConfigurationProvider>
          receiver);

  void InitializeNonConfigurableAccelerators(NonConfigurableActionsMap);

  const NonConfigurableActionsMap& GetNonConfigurableAcceleratorsForTesting() {
    return non_configurable_actions_mapping_;
  }

  mojom::AcceleratorInfoPtr CreateTextAcceleratorInfo(
      const NonConfigurableAcceleratorDetails& details);

  mojom::TextAcceleratorPropertiesPtr CreateTextAcceleratorProperties(
      const NonConfigurableAcceleratorDetails& details);

 private:
  friend class AcceleratorConfigurationProviderTest;

  void OnAcceleratorsUpdated(mojom::AcceleratorSource source,
                             const ActionIdToAcceleratorsMap& mapping);

  void UpdateKeyboards();

  AcceleratorConfigurationMap CreateConfigurationMap();

  void NotifyAcceleratorsUpdated();

  std::vector<mojom::AcceleratorInfoPtr> CreateAcceleratorInfos(
      const std::vector<ui::Accelerator>& accelerators) const;

  std::vector<mojom::AcceleratorLayoutInfoPtr> layout_infos_;

  std::map<AcceleratorActionId, std::vector<mojom::AcceleratorInfoPtr>>
      id_to_accelerator_info_;

  AcceleratorSourceMap accelerators_mapping_;

  // Stores all connected keyboards.
  std::vector<ui::InputDevice> connected_keyboards_;

  NonConfigurableActionsMap non_configurable_actions_mapping_;

  AcceleratorAliasConverter accelerator_alias_converter_;

  mojo::Receiver<
      shortcut_customization::mojom::AcceleratorConfigurationProvider>
      receiver_{this};

  AcceleratorConfiguration* ash_accelerator_configuration_;

  mojo::Remote<shortcut_customization::mojom::AcceleratorsUpdatedObserver>
      accelerators_updated_observers_;

  base::WeakPtrFactory<AcceleratorConfigurationProvider> weak_ptr_factory_{
      this};
};

std::u16string GetKeyDisplay(ui::KeyboardCode key_code);

}  // namespace shortcut_ui
}  // namespace ash

#endif  // ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_ACCELERATOR_CONFIGURATION_PROVIDER_H_
