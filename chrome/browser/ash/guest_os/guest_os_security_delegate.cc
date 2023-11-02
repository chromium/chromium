// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_security_delegate.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "components/exo/server/wayland_server_controller.h"

namespace guest_os {

GuestOsSecurityDelegate::GuestOsSecurityDelegate() : weak_factory_(this) {}

GuestOsSecurityDelegate::~GuestOsSecurityDelegate() = default;

// static
void GuestOsSecurityDelegate::BuildServer(
    std::unique_ptr<GuestOsSecurityDelegate> security_delegate,
    BuildCallback callback) {
  // Ownership of the security_delegate is transferred to exo in the following
  // request. Exo ensures the security_delegate live until we call
  // DeleteServer(), so we will retain a copy of its pointer for future use.
  base::WeakPtr<GuestOsSecurityDelegate> cap_ptr =
      security_delegate->weak_factory_.GetWeakPtr();
  exo::WaylandServerController::Get()->CreateServer(
      std::move(security_delegate),
      base::BindOnce(std::move(callback), cap_ptr));
}

// static
void GuestOsSecurityDelegate::MaybeRemoveServer(
    base::WeakPtr<GuestOsSecurityDelegate> security_delegate,
    const base::FilePath& path) {
  if (!security_delegate)
    return;
  exo::WaylandServerController* controller =
      exo::WaylandServerController::Get();
  if (!controller)
    return;
  controller->DeleteServer(path);
}

}  // namespace guest_os
