// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_MANAGER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_metrics_util.h"
#include "chrome/browser/chromeos/vm_starting_observer.h"
#include "chromeos/dbus/concierge/service.pb.h"
#include "chromeos/dbus/vm_plugin_dispatcher/vm_plugin_dispatcher.pb.h"
#include "chromeos/dbus/vm_plugin_dispatcher_client.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace plugin_vm {

// The PluginVmManager is responsible for connecting to the D-Bus services to
// manage the Plugin Vm.

class PluginVmManager : public KeyedService,
                        public chromeos::VmPluginDispatcherClient::Observer {
 public:
  static PluginVmManager* GetForProfile(Profile* profile);

  explicit PluginVmManager(Profile* profile);
  ~PluginVmManager() override;

  void LaunchPluginVm();
  void StopPluginVm(const std::string& name);

  // Seneschal server handle to use for path sharing.
  uint64_t seneschal_server_handle() { return seneschal_server_handle_; }

  // chromeos::VmPluginDispatcherClient::Observer:
  void OnVmStateChanged(
      const vm_tools::plugin_dispatcher::VmStateChangedSignal& signal) override;

  vm_tools::plugin_dispatcher::VmState vm_state() const { return vm_state_; }

  // Add/remove vm starting observers.
  void AddVmStartingObserver(chromeos::VmStartingObserver* observer);
  void RemoveVmStartingObserver(chromeos::VmStartingObserver* observer);

 private:
  // The flow to launch a Plugin Vm. We'll probably want to add additional
  // abstraction around starting the services in the future but this is
  // sufficient for now.
  void OnStartPluginVmDispatcher(bool success);
  void OnListVms(
      base::Optional<vm_tools::plugin_dispatcher::ListVmResponse> reply);
  void StartVm();
  void OnStartVm(
      base::Optional<vm_tools::plugin_dispatcher::StartVmResponse> reply);
  void ShowVm();
  void OnShowVm(
      base::Optional<vm_tools::plugin_dispatcher::ShowVmResponse> reply);
  void OnGetVmInfoForSharing(
      base::Optional<vm_tools::concierge::GetVmInfoResponse> reply);
  void OnDefaultSharedDirExists(const base::FilePath& dir, bool exists);

  // Called when LaunchPluginVm() is unsuccessful.
  void LaunchFailed(PluginVmLaunchResult result = PluginVmLaunchResult::kError);

  Profile* profile_;
  std::string owner_id_;
  uint64_t seneschal_server_handle_ = 0;

  // State of the default VM, kept up-to-date by signals from the dispatcher.
  vm_tools::plugin_dispatcher::VmState vm_state_ =
      vm_tools::plugin_dispatcher::VmState::VM_STATE_UNKNOWN;

  // We can't immediately start the VM when it is in states like suspending, so
  // delay until an in progress operation finishes.
  bool pending_start_vm_ = false;

  base::ObserverList<chromeos::VmStartingObserver> vm_starting_observers_;

  base::WeakPtrFactory<PluginVmManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PluginVmManager);
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_MANAGER_H_
