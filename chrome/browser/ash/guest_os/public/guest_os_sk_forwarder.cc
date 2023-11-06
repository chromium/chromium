// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/public/guest_os_sk_forwarder.h"

#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/guest_os/vm_sk_forwarding_native_message_host.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace guest_os {

GuestOsSkForwarder::GuestOsSkForwarder() = default;
GuestOsSkForwarder::~GuestOsSkForwarder() = default;

void GuestOsSkForwarder::BindCrosapiRemote(
    mojo::PendingRemote<crosapi::mojom::GuestOsSkForwarder> remote) {
  remote_.reset();
  remote_.Bind(std::move(remote));
}

void GuestOsSkForwarder::DeliverMessageToSKForwardingExtension(
    Profile* profile,
    const std::string& json_message,
    crosapi::mojom::GuestOsSkForwarder::ForwardRequestCallback callback) {
  // Signal errors or non-response with an empty string.
  callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), "");

  if (crosapi::browser_util::IsLacrosEnabled()) {
    if (remote_.is_bound() && remote_.is_connected()) {
      remote_->ForwardRequest(json_message, std::move(callback));
    }
  } else {
    // TODO(b/306296365) Once we require lacros, remove this branch and the ash
    // copy of VmSKForwardingNativeMessageHost
    ash::guest_os::VmSKForwardingNativeMessageHost::
        DeliverMessageToSKForwardingExtension(profile, json_message,
                                              std::move(callback));
  }
}

}  // namespace guest_os
