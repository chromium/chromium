// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_service.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "bruschetta_terminal_provider.h"
#include "chrome/browser/ash/bruschetta/bruschetta_features.h"
#include "chrome/browser/ash/bruschetta/bruschetta_launcher.h"
#include "chrome/browser/ash/bruschetta/bruschetta_mount_provider.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service_factory.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/guest_os/public/types.h"

namespace bruschetta {

BruschettaService::BruschettaService(Profile* profile) : profile_(profile) {
  // Don't set up anything if the bruschetta flag isn't enabled.
  if (!BruschettaFeatures::Get()->IsEnabled())
    return;

  bool registered_guests = false;
  // Register all bruschetta instances that have already been installed.
  for (const auto& guest_id :
       guest_os::GetContainers(profile, guest_os::VmType::BRUSCHETTA)) {
    Register(guest_id);
    registered_guests = true;
  }

  // Migrate VMs installed during the alpha. These will have been set up by hand
  // using vmc so chrome doesn't know about this, but we know what the VM name
  // should be, so register it here is nothing has been registered from prefs
  // and the migration flag is turned on.
  if (!registered_guests && base::FeatureList::IsEnabled(
                                chromeos::features::kBruschettaAlphaMigrate)) {
    Register(GetBruschettaId());
  }
}

BruschettaService::~BruschettaService() = default;

BruschettaService* BruschettaService::GetForProfile(Profile* profile) {
  return BruschettaServiceFactory::GetForProfile(profile);
}

void BruschettaService::Register(const guest_os::GuestId& guest_id) {
  launchers_.insert({guest_id.vm_name, std::make_unique<BruschettaLauncher>(
                                           guest_id.vm_name, profile_)});
  guest_os::GuestOsService::GetForProfile(profile_)
      ->MountProviderRegistry()
      ->Register(std::make_unique<BruschettaMountProvider>(profile_, guest_id));
  guest_os::GuestOsService::GetForProfile(profile_)
      ->TerminalProviderRegistry()
      ->Register(
          std::make_unique<BruschettaTerminalProvider>(profile_, guest_id));
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
