// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/guest_os_sk_forwarder_factory_ash.h"

#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace crosapi {

GuestOsSkForwarderFactoryAsh::GuestOsSkForwarderFactoryAsh()
    : receiver_(this) {}

GuestOsSkForwarderFactoryAsh::~GuestOsSkForwarderFactoryAsh() = default;

void GuestOsSkForwarderFactoryAsh::BindReceiver(
    mojo::PendingReceiver<mojom::GuestOsSkForwarderFactory> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void GuestOsSkForwarderFactoryAsh::BindGuestOsSkForwarder(
    mojo::PendingRemote<mojom::GuestOsSkForwarder> remote) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();

  auto* service = guest_os::GuestOsService::GetForProfile(profile);

  if (service) {
    service->SkForwarder()->BindCrosapiRemote(std::move(remote));
  }
}

}  // namespace crosapi
