// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/guest_os/vm_sk_forwarding_service.h"

#include "chrome/browser/lacros/guest_os/vm_sk_forwarding_native_message_host.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/lacros/lacros_service.h"

namespace guest_os {

VmSkForwardingService::VmSkForwardingService() : receiver_(this) {
  auto* lacros_service = chromeos::LacrosService::Get();
  DCHECK(lacros_service);

  if (lacros_service
          ->IsAvailable<crosapi::mojom::GuestOsSkForwarderFactory>()) {
    lacros_service->GetRemote<crosapi::mojom::GuestOsSkForwarderFactory>()
        ->BindGuestOsSkForwarder(receiver_.BindNewPipeAndPassRemote());
  }
}

VmSkForwardingService::~VmSkForwardingService() = default;

void VmSkForwardingService::ForwardRequest(const std::string& message,
                                           ForwardRequestCallback callback) {
  auto* profile = ProfileManager::GetPrimaryUserProfile();

  VmSKForwardingNativeMessageHost::DeliverMessageToSKForwardingExtension(
      profile, message, std::move(callback));
}

}  // namespace guest_os
