// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/multidevice_setup/multidevice_setup_base.h"

namespace chromeos {

namespace multidevice_setup {

// TODO(https://crbug.com/1164001): remove after migrating to ash.
namespace mojom = ::ash::multidevice_setup::mojom;

MultiDeviceSetupBase::MultiDeviceSetupBase() = default;

MultiDeviceSetupBase::~MultiDeviceSetupBase() = default;

void MultiDeviceSetupBase::BindReceiver(
    mojo::PendingReceiver<mojom::MultiDeviceSetup> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MultiDeviceSetupBase::CloseAllReceivers() {
  receivers_.Clear();
}

}  // namespace multidevice_setup

}  // namespace chromeos
