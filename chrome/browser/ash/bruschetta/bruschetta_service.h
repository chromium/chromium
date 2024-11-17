// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_SERVICE_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_SERVICE_H_

#include <string_view>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_remover.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider_registry.h"
#include "chrome/browser/ash/guest_os/public/guest_os_terminal_provider_registry.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace bruschetta {

class BruschettaLauncher;

// A service to hold the separate modules that provide Bruschetta
// (third-party/generic VM) support within Chrome (files app integration, app
// service integration, etc).
class BruschettaService : public KeyedService,
                          public ash::ConciergeClient::VmObserver {
 public:
  explicit BruschettaService(Profile* profile);
  ~BruschettaService() override;

  // Register an existing bruschetta instance with the terminal app.
  void RegisterWithTerminal(const guest_os::GuestId& guest_id);

  // Register a new bruschetta instance in prefs. `config_id` controls which
  // enterprise policy manages this instance.
  void RegisterInPrefs(const guest_os::GuestId& guest_id,
                       const std::string& config_id);

  // Register `vm_name` as running with the specified enterprise policy. This is
  // a no-op if called for a VM we already believe to be running. In particular,
  // in that case the VM is considered to keep its old policy, not the new one
  // passed in.
  void RegisterVmLaunch(std::string vm_name, RunningVmPolicy policy);

  // Returns a handle to the launcher for the vm specified by `vm_name`. Will
  // return a null pointer if the name isn't recognised.
  base::WeakPtr<BruschettaLauncher> GetLauncher(std::string vm_name);

  // ConciergeClient::VmObserver:
  void OnVmStarted(const vm_tools::concierge::VmStartedSignal& signal) override;
  void OnVmStopped(const vm_tools::concierge::VmStoppedSignal& signal) override;

  void SetLauncherForTesting(std::string vm_name,
                             std::unique_ptr<BruschettaLauncher> launcher);

  const base::flat_map<std::string, RunningVmPolicy>& GetRunningVmsForTesting();

  // Uninstalls the Bruschetta vm identified by `guest_id`. No-op if already
  // completely uninstalled, can run after a failed uninstall to finish cleaning
  // up.
  void RemoveVm(const guest_os::GuestId& guest_id,
                base::OnceCallback<void(bool)> callback);

  // Checks if the vm identified by `vm_name` is in the running list.
  bool IsVmRunning(std::string_view vm_name);

  // Stops all running VMs.
  void StopRunningVms();

 private:
  struct VmRegistration {
    std::unique_ptr<BruschettaLauncher> launcher;
    guest_os::GuestOsMountProviderRegistry::Id mount_id;
    // We don't track the terminal registration because that should remain in
    // place even if the VM is blocked from launching.

    VmRegistration(std::unique_ptr<BruschettaLauncher> launcher,
                   guest_os::GuestOsMountProviderRegistry::Id mount_id);
    VmRegistration(VmRegistration&&);
    VmRegistration& operator=(VmRegistration&&);
    VmRegistration(const VmRegistration&) = delete;
    VmRegistration& operator=(const VmRegistration&) = delete;
    ~VmRegistration();
  };

  void OnPolicyChanged();
  void AllowLaunch(guest_os::GuestId guest_id);
  void BlockLaunch(guest_os::GuestId guest_id);
  void StopVm(std::string vm_name);

  void StopVmIfRequiredByPolicy(std::string vm_name,
                                std::string config_id,
                                const base::Value::Dict* config);

  void OnRemoveVm(base::OnceCallback<void(bool)> callback,
                  guest_os::GuestId guest_id,
                  guest_os::GuestOsRemover::Result result);
  void OnUninstallToolsDlc(base::OnceCallback<void(bool)> callback,
                           guest_os::GuestId guest_id,
                           std::string_view result);
  void OnUninstallAllDlcs(base::OnceCallback<void(bool)> callback,
                          guest_os::GuestId guest_id,
                          std::string_view tools_result,
                          std::string_view firmware_result);

  base::flat_map<std::string, VmRegistration> runnable_vms_;
  base::flat_map<std::string, RunningVmPolicy> running_vms_;

  // Terminal providers are special, since even non-runnable VMs should still
  // show up in the terminal, so we track them separately instead of as part of
  // runnable_vms_.
  base::flat_map<std::string, guest_os::GuestOsTerminalProviderRegistry::Id>
      terminal_providers_;

  PrefChangeRegistrar pref_observer_;
  base::CallbackListSubscription cros_settings_observer_;

  const raw_ptr<Profile> profile_;

  // Must be last
  base::WeakPtrFactory<BruschettaService> weak_ptr_factory_{this};
};

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_SERVICE_H_
