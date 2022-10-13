// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/accelerator_configuration_provider.h"

#include <vector>

#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/shell.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "base/containers/flat_map.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {

namespace {

mojom::AcceleratorInfoPtr AcceleratorInfoToMojom(
    const AcceleratorInfo& accelerator) {
  mojom::AcceleratorInfoPtr info_mojom = mojom::AcceleratorInfo::New();
  info_mojom->accelerator = accelerator.accelerator;
  info_mojom->key_display = accelerator.key_display;
  info_mojom->type = accelerator.type;
  info_mojom->state = accelerator.state;
  info_mojom->locked = accelerator.locked;

  return info_mojom;
}

}  // namespace

namespace shortcut_ui {

AcceleratorConfigurationProvider::AcceleratorConfigurationProvider()
    : ash_accelerator_configuration_(
          Shell::Get()->ash_accelerator_configuration()) {
  ash_accelerator_configuration_->AddAcceleratorsUpdatedCallback(
      base::BindRepeating(
          &AcceleratorConfigurationProvider::OnAshAcceleratorsUpdated,
          weak_ptr_factory_.GetWeakPtr()));
}

AcceleratorConfigurationProvider::~AcceleratorConfigurationProvider() = default;

void AcceleratorConfigurationProvider::IsMutable(
    ash::mojom::AcceleratorSource source,
    IsMutableCallback callback) {
  if (source == ash::mojom::AcceleratorSource::kBrowser) {
    // Browser shortcuts are not mutable.
    std::move(callback).Run(/*is_mutable=*/false);
    return;
  }

  // TODO(jimmyxgong): Add more cases for other source types when they're
  // available.
  std::move(callback).Run(/*is_mutable=*/true);
}

void AcceleratorConfigurationProvider::GetAccelerators(
    GetAcceleratorsCallback callback) {
  AcceleratorConfigurationMap accelerator_config;

  base::flat_map<AcceleratorActionId, std::vector<mojom::AcceleratorInfoPtr>>
      accelerators_mojom;
  // TODO(jimmyxgong): Currently only handling Ash case, need to also include
  // other accelerator sources.
  for (const auto& [id, accelerators] : ash_accelerator_mapping_) {
    std::vector<mojom::AcceleratorInfoPtr> infos_mojom;
    infos_mojom.reserve(accelerators.size());
    for (const auto& accelerator : accelerators) {
      infos_mojom.push_back(AcceleratorInfoToMojom(accelerator));
    }
    accelerators_mojom.emplace(id, std::move(infos_mojom));
  }

  accelerator_config.emplace(mojom::AcceleratorSource::kAsh,
                             std::move(accelerators_mojom));

  std::move(callback).Run(std::move(accelerator_config));
}

void AcceleratorConfigurationProvider::BindInterface(
    mojo::PendingReceiver<
        shortcut_customization::mojom::AcceleratorConfigurationProvider>
        receiver) {
  receiver_.Bind(std::move(receiver));
}

void AcceleratorConfigurationProvider::OnAshAcceleratorsUpdated(
    mojom::AcceleratorSource source,
    const std::map<AcceleratorActionId, std::vector<AcceleratorInfo>>&
        mapping) {
  ash_accelerator_mapping_ = mapping;
}

}  // namespace shortcut_ui
}  // namespace ash
