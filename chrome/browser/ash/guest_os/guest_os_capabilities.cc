// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_capabilities.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "components/exo/server/wayland_server_controller.h"

namespace guest_os {

GuestOsCapabilities::GuestOsCapabilities() : weak_factory_(this) {}

GuestOsCapabilities::~GuestOsCapabilities() = default;

// static
void GuestOsCapabilities::BuildServer(
    std::unique_ptr<GuestOsCapabilities> capabilities,
    BuildCallback callback) {
  // Ownership of the capabilities is transferred to exo in the following
  // request. Exo ensures the capabilities live until we call DeleteServer(),
  // so we will retain a copy of its pointer for future use.
  base::WeakPtr<GuestOsCapabilities> cap_ptr =
      capabilities->weak_factory_.GetWeakPtr();
  exo::WaylandServerController::Get()->CreateServer(
      std::move(capabilities), base::BindOnce(std::move(callback), cap_ptr));
}

// static
void GuestOsCapabilities::MaybeRemoveServer(
    base::WeakPtr<GuestOsCapabilities> capabilities,
    const base::FilePath& path) {
  if (!capabilities)
    return;
  exo::WaylandServerController* controller =
      exo::WaylandServerController::Get();
  if (!controller)
    return;
  controller->DeleteServer(path);
}

}  // namespace guest_os
