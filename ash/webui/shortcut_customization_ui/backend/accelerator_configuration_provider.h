// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_ACCELERATOR_CONFIGURATION_PROVIDER_H_
#define ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_ACCELERATOR_CONFIGURATION_PROVIDER_H_

#include <map>

#include "ash/accelerators/accelerator_layout_table.h"
#include "ash/public/cpp/accelerator_configuration.h"
#include "ash/public/mojom/accelerator_keys.mojom.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace ash {
namespace shortcut_ui {

class AcceleratorConfigurationProvider
    : public shortcut_customization::mojom::AcceleratorConfigurationProvider,
      public ui::InputDeviceEventObserver,
      public input_method::InputMethodManager::Observer {
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

  void BindInterface(
      mojo::PendingReceiver<
          shortcut_customization::mojom::AcceleratorConfigurationProvider>
          receiver);

  void InitializeNonConfigurableAccelerators(
      NonConfigurableActionsTextDetailsMap);

  mojom::AcceleratorInfoPtr CreateTextAcceleratorInfo(
      const AcceleratorTextDetails& details);
  mojom::TextAcceleratorPropertiesPtr CreateTextAcceleratorParts();

 private:
  friend class AcceleratorConfigurationProviderTest;

  void OnAcceleratorsUpdated(mojom::AcceleratorSource source,
                             const ActionIdToAcceleratorsMap& mapping);

  mojom::AcceleratorType GetAcceleratorType(ui::Accelerator accelerator) const;

  void UpdateKeyboards();

  AcceleratorConfigurationMap CreateConfigurationMap();

  void NotifyAcceleratorsUpdated();

  // Create accelerator info using accelerator and extra properties.
  mojom::AcceleratorInfoPtr CreateDefaultAcceleratorInfo(
      const ui::Accelerator& accelerator,
      bool locked,
      mojom::AcceleratorType type,
      mojom::AcceleratorState state) const;

  // Create base accelerator info using accelerator.
  mojom::AcceleratorInfoPtr CreateBaseAcceleratorInfo(
      const ui::Accelerator& accelerator) const;

  // Create alias accelerator info for top row key if applicable.
  mojom::AcceleratorInfoPtr CreateRemappedTopRowAcceleratorInfo(
      const ui::Accelerator& accelerator) const;

  // Create alias accelerator info for six pack key if applicable.
  mojom::AcceleratorInfoPtr CreateRemappedSixPackAcceleratorInfo(
      const ui::Accelerator& accelerator) const;

  // Create alias accelerator infos when the accelerator contains a top row key
  // or six pack key. For |top_row_key|, replace the base accelerator with
  // top-row remapped accelerator, For |six_pack_key|, show both the base
  // accelerator and the six-pack remapped accelerator. Therefore, return a
  // vector here since it may display two accelerator infos for six pack
  // remapping case.
  std::vector<mojom::AcceleratorInfoPtr> CreateAcceleratorInfoVariants(
      const ui::Accelerator& accelerator) const;

  std::vector<mojom::AcceleratorLayoutInfoPtr> layout_infos_;

  std::map<AcceleratorActionId, std::vector<mojom::AcceleratorInfoPtr>>
      id_to_accelerator_info_;

  AcceleratorSourceMap accelerators_mapping_;

  // Stores all connected keyboards.
  std::vector<ui::InputDevice> connected_keyboards_;

  NonConfigurableActionsTextDetailsMap non_configurable_actions_mapping_;

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
