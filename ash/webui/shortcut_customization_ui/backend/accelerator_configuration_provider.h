// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_ACCELERATOR_CONFIGURATION_PROVIDER_H_
#define ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_ACCELERATOR_CONFIGURATION_PROVIDER_H_

#include "ash/public/cpp/accelerator_configuration.h"
#include "ash/public/mojom/accelerator_keys.mojom.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace ash {
namespace shortcut_ui {

class AcceleratorConfigurationProvider
    : public shortcut_customization::mojom::AcceleratorConfigurationProvider,
      public ui::InputDeviceEventObserver {
 public:
  using AcceleratorConfigurationMap =
      base::flat_map<mojom::AcceleratorSource,
                     base::flat_map<AcceleratorActionId,
                                    std::vector<mojom::AcceleratorInfoPtr>>>;
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

  void BindInterface(
      mojo::PendingReceiver<
          shortcut_customization::mojom::AcceleratorConfigurationProvider>
          receiver);

 private:
  friend class AcceleratorConfigurationProviderTest;

  void OnAcceleratorsUpdated(mojom::AcceleratorSource source,
                             const ActionIdToAcceleratorsMap& mapping);

  mojom::AcceleratorType GetAcceleratorType(ui::Accelerator accelerator);

  void UpdateKeyboards();

  AcceleratorConfigurationMap CreateConfigurationMap();

  void NotifyAcceleratorsUpdated();

  std::vector<mojom::AcceleratorLayoutInfoPtr> layout_infos_;

  std::map<AcceleratorActionId, std::vector<mojom::AcceleratorInfoPtr>>
      id_to_accelerator_info_;

  AcceleratorSourceMap accelerators_mapping_;

  // Stores all connected keyboards.
  std::vector<ui::InputDevice> connected_keyboards_;

  mojo::Receiver<
      shortcut_customization::mojom::AcceleratorConfigurationProvider>
      receiver_{this};

  AcceleratorConfiguration* ash_accelerator_configuration_;

  mojo::Remote<shortcut_customization::mojom::AcceleratorsUpdatedObserver>
      accelerators_updated_observers_;

  base::WeakPtrFactory<AcceleratorConfigurationProvider> weak_ptr_factory_{
      this};
};

}  // namespace shortcut_ui
}  // namespace ash

#endif  // ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_ACCELERATOR_CONFIGURATION_PROVIDER_H_
