// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/debug_interface_registerer_ash.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/public/cpp/accelerator_actions.h"
#include "ash/shell.h"
#include "base/notreached.h"
#include "chromeos/crosapi/mojom/debug_interface.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

DebugInterfaceRegistererAsh::DebugInterfaceRegistererAsh() {
  // This class is instantiated after `ash::Shell` but destroyed before
  // `ash::Shell` and thus does not need to unregister itself from
  // `AcceleratorControllerImpl`.
  // In detail the ownership chain goes like:
  // CrosapiManager -> CrosapiAsh -> DebugInterfaceRegistererAsh.
  // Shell -> AcceleratorControllerImpl.
  // The ctor and dtor order is:
  // Shell ctor, CrosapiManager ctor, Shell dtor, CrosapiManager dtor
  //
  // Some tests do not initialize `Shell` so check if its initialized.
  if (ash::Shell::HasInstance()) {
    ash::Shell::Get()->accelerator_controller()->SetDebugDelegate(this);
  }
}
DebugInterfaceRegistererAsh::~DebugInterfaceRegistererAsh() {
  // Required because some tests have a different dtor order from the real
  // implementation. Specifically in real implementation,
  // `AcceleratorControllerImpl` is destructed before `DebugInterfaceAsh` and
  // thus no need to specifically unset the delegate. However in some tests, the
  // dtor order is reversed and thus without this explicit unsetting, it causes
  // a dangling ptr. crbug.com/1518715.
  if (ash::Shell::HasInstance()) {
    ash::Shell::Get()->accelerator_controller()->SetDebugDelegate(nullptr);
  }
}

void DebugInterfaceRegistererAsh::BindReceiver(
    mojo::PendingReceiver<mojom::DebugInterfaceRegisterer> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void DebugInterfaceRegistererAsh::RegisterDebugInterface(
    mojo::PendingRemote<mojom::DebugInterface> interface) {
  interfaces_.Add(std::move(interface));
}

void DebugInterfaceRegistererAsh::PrintLayerHierarchy() {
  for (const auto& interface : interfaces_) {
    interface->PrintUiHierarchy(mojom::PrintTarget::kLayer);
  }
}

void DebugInterfaceRegistererAsh::PrintWindowHierarchy() {
  for (const auto& interface : interfaces_) {
    interface->PrintUiHierarchy(mojom::PrintTarget::kWindow);
  }
}

void DebugInterfaceRegistererAsh::PrintViewHierarchy() {
  for (const auto& interface : interfaces_) {
    interface->PrintUiHierarchy(mojom::PrintTarget::kView);
  }
}

}  // namespace crosapi
