// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_service.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "bruschetta_terminal_provider.h"
#include "chrome/browser/ash/bruschetta/bruschetta_features.h"
#include "chrome/browser/ash/bruschetta/bruschetta_launcher.h"
#include "chrome/browser/ash/bruschetta/bruschetta_mount_provider.h"
#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service_factory.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "components/prefs/pref_service.h"

namespace bruschetta {

BruschettaService::VmRegistration::VmRegistration(
    std::unique_ptr<BruschettaLauncher> launcher,
    guest_os::GuestOsMountProviderRegistry::Id mount_id)
    : launcher(std::move(launcher)), mount_id(mount_id) {}

BruschettaService::VmRegistration::VmRegistration(VmRegistration&&) = default;
BruschettaService::VmRegistration& BruschettaService::VmRegistration::operator=(
    BruschettaService::VmRegistration&&) = default;
BruschettaService::VmRegistration::~VmRegistration() = default;

BruschettaService::BruschettaService(Profile* profile) : profile_(profile) {
  // Don't set up anything if the bruschetta flag isn't enabled.
  if (!BruschettaFeatures::Get()->IsEnabled())
    return;

  pref_observer_.Init(profile_->GetPrefs());
  pref_observer_.Add(
      prefs::kBruschettaVMConfiguration,
      base::BindRepeating(&BruschettaService::OnPolicyChanged,
                          // Safety: `pref_observer_` owns this callback and is
                          // destroyed before `this`.
                          base::Unretained(this)));

  bool registered_guests = false;
  // Register all bruschetta instances that have already been installed.
  for (auto& guest_id :
       guest_os::GetContainers(profile, guest_os::VmType::BRUSCHETTA)) {
    RegisterWithTerminal(std::move(guest_id));
    registered_guests = true;
  }

  // Migrate VMs installed during the alpha. These will have been set up by hand
  // using vmc so chrome doesn't know about this, but we know what the VM name
  // should be, so register it here is nothing has been registered from prefs
  // and the migration flag is turned on. Note that we do not call
  // `RegisterInPrefs` because these VMs are currently outside of enterprise
  // policy.
  if (!registered_guests &&
      base::FeatureList::IsEnabled(ash::features::kBruschettaAlphaMigrate)) {
    auto guest_id = GetBruschettaAlphaId();
    guest_os::AddContainerToPrefs(profile_, guest_id, {});
    RegisterWithTerminal(std::move(guest_id));
  }

  OnPolicyChanged();
}

BruschettaService::~BruschettaService() = default;

BruschettaService* BruschettaService::GetForProfile(Profile* profile) {
  return BruschettaServiceFactory::GetForProfile(profile);
}

void BruschettaService::OnPolicyChanged() {
  for (auto& guest_id :
       guest_os::GetContainers(profile_, guest_os::VmType::BRUSCHETTA)) {
    const base::Value* config_id_value = GetContainerPrefValue(
        profile_, guest_id, guest_os::prefs::kBruschettaConfigId);
    if (!config_id_value) {
      // Alpha VM, ignore policy
      AllowLaunch(std::move(guest_id));
      continue;
    }

    const std::string& config_id = config_id_value->GetString();
    absl::optional<const base::Value::Dict*> config =
        GetRunnableConfig(profile_, config_id);
    if (!config.has_value()) {
      // config is either unset or explicitly blocked from running.
      BlockLaunch(std::move(guest_id));
      continue;
    }

    AllowLaunch(std::move(guest_id));
  }
}

void BruschettaService::AllowLaunch(guest_os::GuestId guest_id) {
  if (runnable_vms_.contains(guest_id.vm_name)) {
    // Already runnable, do nothing.
    return;
  }

  std::string vm_name = guest_id.vm_name;

  auto launcher =
      std::make_unique<BruschettaLauncher>(guest_id.vm_name, profile_);
  auto mount_id = guest_os::GuestOsService::GetForProfile(profile_)
                      ->MountProviderRegistry()
                      ->Register(std::make_unique<BruschettaMountProvider>(
                          profile_, std::move(guest_id)));

  runnable_vms_.insert(
      {std::move(vm_name), VmRegistration{std::move(launcher), mount_id}});
}

void BruschettaService::BlockLaunch(guest_os::GuestId guest_id) {
  auto it = runnable_vms_.find(guest_id.vm_name);
  if (it == runnable_vms_.end()) {
    // Already blocked, do nothing.
    return;
  }

  guest_os::GuestOsService::GetForProfile(profile_)
      ->MountProviderRegistry()
      ->Unregister(it->second.mount_id);

  runnable_vms_.erase(it);
}

void BruschettaService::RegisterInPrefs(const guest_os::GuestId& guest_id,
                                        const std::string& config_id) {
  base::Value::Dict properties;
  properties.Set(guest_os::prefs::kBruschettaConfigId, config_id);
  guest_os::AddContainerToPrefs(profile_, guest_id, std::move(properties));

  RegisterWithTerminal(guest_id);

  if (GetRunnableConfig(profile_, config_id).has_value()) {
    AllowLaunch(guest_id);
  }
}

void BruschettaService::RegisterWithTerminal(
    const guest_os::GuestId& guest_id) {
  guest_os::GuestOsService::GetForProfile(profile_)
      ->TerminalProviderRegistry()
      ->Register(
          std::make_unique<BruschettaTerminalProvider>(profile_, guest_id));
  guest_os::GuestOsSharePath::GetForProfile(profile_)->RegisterGuest(guest_id);
}

base::WeakPtr<BruschettaLauncher> BruschettaService::GetLauncher(
    std::string vm_name) {
  auto it = runnable_vms_.find(vm_name);
  if (it == runnable_vms_.end()) {
    return nullptr;
  }
  return it->second.launcher->GetWeakPtr();
}

void BruschettaService::SetLauncherForTesting(
    std::string vm_name,
    std::unique_ptr<BruschettaLauncher> launcher) {
  runnable_vms_.insert(
      {std::move(vm_name), VmRegistration{std::move(launcher), -1}});
}

}  // namespace bruschetta
