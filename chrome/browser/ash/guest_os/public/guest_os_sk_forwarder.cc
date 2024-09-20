// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/public/guest_os_sk_forwarder.h"

#include "chrome/browser/ash/guest_os/vm_sk_forwarding_native_message_host.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace guest_os {

GuestOsSkForwarder::GuestOsSkForwarder() = default;
GuestOsSkForwarder::~GuestOsSkForwarder() = default;

void GuestOsSkForwarder::DeliverMessageToSKForwardingExtension(
    Profile* profile,
    const std::string& json_message,
    crosapi::mojom::GuestOsSkForwarder::ForwardRequestCallback callback) {
  // Signal errors or non-response with an empty string.
  callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), "");
  ash::guest_os::VmSKForwardingNativeMessageHost::
      DeliverMessageToSKForwardingExtension(profile, json_message,
                                            std::move(callback));
}

}  // namespace guest_os
