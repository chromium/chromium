// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_service.h"

#include <memory>
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/bruschetta/bruschetta_launcher.h"
#include "chrome/browser/ash/bruschetta/bruschetta_mount_provider.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service_factory.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/guest_os/public/types.h"

namespace bruschetta {

BruschettaService::BruschettaService(Profile* profile) {
  // TODO(b/233289313): Once we have an installer we need to do this
  // dynamically, but in the alpha people have VMs they created via vmc called
  // "bru" so hardcode this to get them working while we work on the full
  // installer. Similarly, Bruschetta doesn't have a container but it runs a
  // garcon that identifies itself as in a penguin container, so use that name.
  guest_os::GuestId alpha_id{guest_os::VmType::BRUSCHETTA, "bru", "penguin"};
  launchers_.insert(
      {alpha_id.vm_name, std::make_unique<BruschettaLauncher>("bru", profile)});
  guest_os::GuestOsService::GetForProfile(profile)
      ->MountProviderRegistry()
      ->Register(std::make_unique<BruschettaMountProvider>(profile, alpha_id));
}

BruschettaService::~BruschettaService() = default;

BruschettaService* BruschettaService::GetForProfile(Profile* profile) {
  return BruschettaServiceFactory::GetForProfile(profile);
}

base::WeakPtr<BruschettaLauncher> BruschettaService::GetLauncher(
    std::string vm_name) {
  auto it = launchers_.find(vm_name);
  if (it == launchers_.end()) {
    return nullptr;
  }
  return it->second->GetWeakPtr();
}

void BruschettaService::SetLauncherForTesting(
    std::string vm_name,
    std::unique_ptr<BruschettaLauncher> launcher) {
  launchers_.insert({vm_name, std::move(launcher)});
}

}  // namespace bruschetta
