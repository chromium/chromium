// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_security_delegate.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "components/exo/server/wayland_server_controller.h"
#include "components/exo/server/wayland_server_handle.h"
#include "third_party/cros_system_api/constants/vm_tools.h"

namespace guest_os {

GuestOsSecurityDelegate::GuestOsSecurityDelegate(std::string vm_name)
    : vm_name_(std::move(vm_name)), weak_factory_(this) {}

GuestOsSecurityDelegate::~GuestOsSecurityDelegate() = default;

// static
void GuestOsSecurityDelegate::MakeServerWithFd(
    std::unique_ptr<GuestOsSecurityDelegate> security_delegate,
    base::ScopedFD fd,
    base::OnceCallback<void(base::WeakPtr<GuestOsSecurityDelegate>,
                            std::unique_ptr<exo::WaylandServerHandle>)>
        callback) {
  // Ownership of the security_delegate is transferred to exo in the following
  // request. Exo ensures the security_delegate lives until we call
  // CloseSocket(), so we will retain a copy of its pointer for future use.
  base::WeakPtr<GuestOsSecurityDelegate> cap_ptr =
      security_delegate->weak_factory_.GetWeakPtr();
  exo::WaylandServerController::Get()->ListenOnSocket(
      std::move(security_delegate), std::move(fd),
      base::BindOnce(std::move(callback), cap_ptr));
}

std::string GuestOsSecurityDelegate::GetVmName(ui::EndpointType target) const {
  return vm_name_;
}

}  // namespace guest_os
