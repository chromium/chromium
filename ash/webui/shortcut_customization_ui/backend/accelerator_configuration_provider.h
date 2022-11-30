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
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {
namespace shortcut_ui {

class AcceleratorConfigurationProvider
    : shortcut_customization::mojom::AcceleratorConfigurationProvider {
 public:
  using AcceleratorConfigurationMap =
      base::flat_map<mojom::AcceleratorSource,
                     base::flat_map<AcceleratorActionId,
                                    std::vector<mojom::AcceleratorInfoPtr>>>;

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

  void BindInterface(
      mojo::PendingReceiver<
          shortcut_customization::mojom::AcceleratorConfigurationProvider>
          receiver);

 private:
  friend class AcceleratorConfigurationProviderTest;

  void OnAshAcceleratorsUpdated(
      mojom::AcceleratorSource source,
      const std::map<AcceleratorActionId, std::vector<AcceleratorInfo>>&
          mapping);

  std::map<AcceleratorActionId, std::vector<AcceleratorInfo>>
      ash_accelerator_mapping_;

  mojo::Receiver<
      shortcut_customization::mojom::AcceleratorConfigurationProvider>
      receiver_{this};

  AcceleratorConfiguration* ash_accelerator_configuration_;
  base::WeakPtrFactory<AcceleratorConfigurationProvider> weak_ptr_factory_{
      this};
};

}  // namespace shortcut_ui
}  // namespace ash

#endif  // ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_ACCELERATOR_CONFIGURATION_PROVIDER_H_
