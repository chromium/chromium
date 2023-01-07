// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_MANAGER_H_
#define CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_MANAGER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/dbus/vm_plugin_dispatcher/vm_plugin_dispatcher.pb.h"
#include "components/keyed_service/core/keyed_service.h"

namespace ash {
class VmStartingObserver;
}

namespace plugin_vm {

class PluginVmManager : public KeyedService {
 public:
  using LaunchPluginVmCallback = base::OnceCallback<void(bool success)>;

  virtual void OnPrimaryUserSessionStarted() = 0;

  // Start and show the VM. The callback is called once the VM is running and
  // we have confirmed VM tools are installed.
  virtual void LaunchPluginVm(LaunchPluginVmCallback callback) = 0;
  virtual void RelaunchPluginVm() = 0;
  virtual void StopPluginVm(const std::string& name, bool force) = 0;
  virtual void UninstallPluginVm() = 0;

  // Seneschal server handle to use for path sharing.
  virtual uint64_t seneschal_server_handle() const = 0;

  virtual void StartDispatcher(
      base::OnceCallback<void(bool success)> callback) const = 0;

  // Add/remove vm starting observers.
  virtual void AddVmStartingObserver(ash::VmStartingObserver* observer) = 0;
  virtual void RemoveVmStartingObserver(ash::VmStartingObserver* observer) = 0;

  virtual vm_tools::plugin_dispatcher::VmState vm_state() const = 0;

  // Indicates whether relaunch (suspend + start) is needed for the new
  // camera/mic permissions to go into effect.
  virtual bool IsRelaunchNeededForNewPermissions() const = 0;
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_MANAGER_H_
