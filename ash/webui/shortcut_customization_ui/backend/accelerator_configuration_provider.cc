// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/accelerator_configuration_provider.h"

#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/shell.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {
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
  // TODO(jimmyxgong): Remove `ash_accelerator_mapping_` and instead fire the
  // Mojo event with the mapping.
  ash_accelerator_mapping_ = mapping;
}

}  // namespace shortcut_ui
}  // namespace ash
