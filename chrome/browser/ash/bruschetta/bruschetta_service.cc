// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_service.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "bruschetta_terminal_provider.h"
#include "chrome/browser/ash/bruschetta/bruschetta_launcher.h"
#include "chrome/browser/ash/bruschetta/bruschetta_mount_provider.h"
#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_remover.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path_factory.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service_factory.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/prefs/pref_service.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"

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
  if (auto* concierge = ash::ConciergeClient::Get(); concierge) {
    concierge->AddVmObserver(this);
  }

  pref_observer_.Init(profile_->GetPrefs());
  pref_observer_.Add(
      prefs::kBruschettaVMConfiguration,
      base::BindRepeating(&BruschettaService::OnPolicyChanged,
                          // Safety: `pref_observer_` owns this callback and is
                          // destroyed before `this`.
                          base::Unretained(this)));
  cros_settings_observer_ = ash::CrosSettings::Get()->AddSettingsObserver(
      ash::kVirtualMachinesAllowed,
      base::BindRepeating(&BruschettaService::OnPolicyChanged,
                          // Safety: This callback will be unregistered when
                          // `cros_settings_observer_` is destroyed.
                          base::Unretained(this)));

  bool bruschetta_installed = false;
  // Register all bruschetta instances that have already been installed.
  for (auto& guest_id :
       guest_os::GetContainers(profile, guest_os::VmType::BRUSCHETTA)) {
    RegisterWithTerminal(std::move(guest_id));
    bruschetta_installed = true;
  }

  // Set the pref if Bruschetta is installed.
  if (bruschetta_installed) {
    profile_->GetPrefs()->SetBoolean(bruschetta::prefs::kBruschettaInstalled,
                                     true);
  }

  OnPolicyChanged();
}

BruschettaService::~BruschettaService() {
  // ConciergeClient may be destroyed prior to BruschettaService in tests.
  // Therefore we do this instead of ScopedObservation.
  if (auto* concierge = ash::ConciergeClient::Get(); concierge) {
    concierge->RemoveVmObserver(this);
  }
}

void BruschettaService::OnPolicyChanged() {
  for (auto guest_id :
       guest_os::GetContainers(profile_, guest_os::VmType::BRUSCHETTA)) {
    std::string config_id;
    const base::Value* pref_value = GetContainerPrefValue(
        profile_, guest_id, guest_os::prefs::kBruschettaConfigId);
    if (pref_value) {
      config_id = pref_value->GetString();
    } else {
      LOG(WARNING) << "Missing container prefs for VM " << guest_id.vm_name;
      BlockLaunch(std::move(guest_id));
      continue;
    }

    std::optional<const base::Value::Dict*> config_opt =
        GetRunnableConfig(profile_, config_id);
    if (!config_opt.has_value()) {
      // config is either unset or explicitly blocked from running.
      BlockLaunch(std::move(guest_id));
      continue;
    }
    const auto* config = *config_opt;

    AllowLaunch(guest_id);

    StopVmIfRequiredByPolicy(guest_id.vm_name, std::move(config_id), config);
  }

  // Any change to policy may change the display name of a config, so sync the
  // terminal prefs.
  auto* terminal_registry =
      guest_os::GuestOsServiceFactory::GetForProfile(profile_)
          ->TerminalProviderRegistry();
  for (const auto& it : terminal_providers_) {
    auto id = it.second;
    terminal_registry->SyncPrefs(id);
  }
}

void BruschettaService::StopVmIfRequiredByPolicy(
    std::string vm_name,
    std::string config_id,
    const base::Value::Dict* config) {
  auto it = running_vms_.find(vm_name);
  if (it != running_vms_.end()) {
    auto old_policy = it->second;
    auto new_policy = GetLaunchPolicyForConfig(profile_, config_id).value();

    if (old_policy.vtpm_enabled != new_policy.vtpm_enabled) {
      auto update_action = static_cast<prefs::PolicyUpdateAction>(
          config->FindDict(prefs::kPolicyVTPMKey)
              ->FindInt(prefs::kPolicyVTPMUpdateActionKey)
              .value());

      if (update_action == prefs::PolicyUpdateAction::FORCE_SHUTDOWN_ALWAYS ||
          (update_action ==
               prefs::PolicyUpdateAction::FORCE_SHUTDOWN_IF_MORE_RESTRICTED &&
           new_policy.vtpm_enabled == false)) {
        StopVm(std::move(vm_name));
      }
    }
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
  auto mount_id = guest_os::GuestOsServiceFactory::GetForProfile(profile_)
                      ->MountProviderRegistry()
                      ->Register(std::make_unique<BruschettaMountProvider>(
                          profile_, std::move(guest_id)));

  runnable_vms_.insert(
      {std::move(vm_name), VmRegistration{std::move(launcher), mount_id}});
}

void BruschettaService::BlockLaunch(guest_os::GuestId guest_id) {
  if (running_vms_.contains(guest_id.vm_name)) {
    StopVm(guest_id.vm_name);
  }

  auto it = runnable_vms_.find(guest_id.vm_name);
  if (it == runnable_vms_.end()) {
    // Already blocked, do nothing.
    return;
  }

  guest_os::GuestOsServiceFactory::GetForProfile(profile_)
      ->MountProviderRegistry()
      ->Unregister(it->second.mount_id);

  runnable_vms_.erase(it);
}

void BruschettaService::StopRunningVms() {
  for (const auto& [name, _] : running_vms_) {
    VLOG(1) << "Stopping vm " << name;
    StopVm(std::move(name));
  }
}

void BruschettaService::StopVm(std::string vm_name) {
  auto* client = ash::ConciergeClient::Get();
  DCHECK(client);

  vm_tools::concierge::StopVmRequest request;
  request.set_name(vm_name);
  request.set_owner_id(ash::ProfileHelper::GetUserIdHashFromProfile(profile_));

  client->StopVm(
      request,
      base::BindOnce(
          [](std::string vm_name,
             std::optional<vm_tools::concierge::StopVmResponse> response) {
            // If stopping the VM fails there's not really much we can do about
            // it, but we can log an error.
            if (!response) {
              LOG(ERROR) << "Failed to shutdown " << vm_name << ": no response";
            } else if (!response->success()) {
              LOG(ERROR) << "Failed to shutdown " << vm_name << ": "
                         << response->failure_reason();
            }
            // No need to call RegisterVmStop, the VM stop signal should do it
            // for us.
          },
          std::move(vm_name)));
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
  DCHECK(!terminal_providers_.contains(guest_id.vm_name));
  terminal_providers_[guest_id.vm_name] =
      guest_os::GuestOsServiceFactory::GetForProfile(profile_)
          ->TerminalProviderRegistry()
          ->Register(
              std::make_unique<BruschettaTerminalProvider>(profile_, guest_id));
  guest_os::GuestOsSharePathFactory::GetForProfile(profile_)->RegisterGuest(
      guest_id);
}

void BruschettaService::RegisterVmLaunch(std::string vm_name,
                                         RunningVmPolicy policy) {
  running_vms_.insert(std::make_pair(std::move(vm_name), std::move(policy)));
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

const base::flat_map<std::string, RunningVmPolicy>&
BruschettaService::GetRunningVmsForTesting() {
  return running_vms_;
}

void BruschettaService::OnVmStarted(
    const vm_tools::concierge::VmStartedSignal& signal) {}
void BruschettaService::OnVmStopped(
    const vm_tools::concierge::VmStoppedSignal& signal) {
  running_vms_.erase(signal.name());
}

void BruschettaService::RemoveVm(const guest_os::GuestId& guest_id,
                                 base::OnceCallback<void(bool)> callback) {
  // Owns itself and will delete itself once done.
  auto remover = base::MakeRefCounted<guest_os::GuestOsRemover>(
      profile_, guest_os::VmType::BRUSCHETTA, guest_id.vm_name,
      base::BindOnce(&BruschettaService::OnRemoveVm,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     guest_id));

  remover->RemoveVm();
}

void BruschettaService::OnRemoveVm(base::OnceCallback<void(bool)> callback,
                                   guest_os::GuestId guest_id,
                                   guest_os::GuestOsRemover::Result result) {
  if (result != guest_os::GuestOsRemover::Result::kSuccess) {
    LOG(ERROR) << "Error removing Bruschetta. Error code: "
               << static_cast<int>(result);
    std::move(callback).Run(false);
    return;
  }
  ash::DlcserviceClient::Get()->Uninstall(
      kToolsDlc, base::BindOnce(&BruschettaService::OnUninstallToolsDlc,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(callback), std::move(guest_id)));
}

void BruschettaService::OnUninstallToolsDlc(
    base::OnceCallback<void(bool)> callback,
    guest_os::GuestId guest_id,
    std::string_view result) {
  ash::DlcserviceClient::Get()->Uninstall(
      kUefiDlc,
      base::BindOnce(&BruschettaService::OnUninstallAllDlcs,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(guest_id), result));
}

void BruschettaService::OnUninstallAllDlcs(
    base::OnceCallback<void(bool)> callback,
    guest_os::GuestId guest_id,
    std::string_view tools_result,
    std::string_view firmware_result) {
  if ((tools_result != dlcservice::kErrorNone &&
       tools_result != dlcservice::kErrorInvalidDlc) ||
      (firmware_result != dlcservice::kErrorNone &&
       firmware_result != dlcservice::kErrorInvalidDlc)) {
    LOG(ERROR) << "Error removing bruschetta DLCs";
    LOG(ERROR) << kToolsDlc << ": " << tools_result;
    LOG(ERROR) << kUefiDlc << ": " << firmware_result;
    std::move(callback).Run(false);
    return;
  }

  profile_->GetPrefs()->SetBoolean(bruschetta::prefs::kBruschettaInstalled,
                                   false);

  guest_os::RemoveContainerFromPrefs(profile_, guest_id);
  auto terminal_iter = terminal_providers_.find(guest_id.vm_name);
  if (terminal_iter != terminal_providers_.end()) {
    guest_os::GuestOsServiceFactory::GetForProfile(profile_)
        ->TerminalProviderRegistry()
        ->Unregister(terminal_iter->second);
    terminal_providers_.erase(terminal_iter);
  }
  auto vm = runnable_vms_.find(guest_id.vm_name);
  if (vm != runnable_vms_.end()) {
    guest_os::GuestOsServiceFactory::GetForProfile(profile_)
        ->MountProviderRegistry()
        ->Unregister(vm->second.mount_id);
    runnable_vms_.erase(vm);
  }

  std::move(callback).Run(true);
}

bool BruschettaService::IsVmRunning(std::string_view vm_name) {
  return running_vms_.contains(vm_name);
}

}  // namespace bruschetta
