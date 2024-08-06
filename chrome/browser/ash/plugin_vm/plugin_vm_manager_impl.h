// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_MANAGER_IMPL_H_

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/guest_os/guest_os_dlc_helper.h"
#include "chrome/browser/ash/guest_os/vm_starting_observer.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_metrics_util.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_uninstaller_notification.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "chromeos/ash/components/dbus/vm_plugin_dispatcher/vm_plugin_dispatcher.pb.h"
#include "chromeos/ash/components/dbus/vm_plugin_dispatcher/vm_plugin_dispatcher_client.h"

class Profile;

namespace plugin_vm {

// Native Parallels error codes.
constexpr int PRL_ERR_SUCCESS = 0;
constexpr int PRL_ERR_DISP_SHUTDOWN_IN_PROCESS = 0x80000404;
constexpr int PRL_ERR_LICENSE_NOT_VALID = 0x80011000;
constexpr int PRL_ERR_LICENSE_EXPIRED = 0x80011001;
constexpr int PRL_ERR_LICENSE_WRONG_VERSION = 0x80011002;
constexpr int PRL_ERR_LICENSE_WRONG_PLATFORM = 0x80011004;
constexpr int PRL_ERR_LICENSE_BETA_KEY_RELEASE_PRODUCT = 0x80011011;
constexpr int PRL_ERR_LICENSE_RELEASE_KEY_BETA_PRODUCT = 0x80011013;
constexpr int PRL_ERR_LICENSE_SUBSCR_EXPIRED = 0x80011074;
constexpr int PRL_ERR_JLIC_WRONG_HWID = 0x80057005;
constexpr int PRL_ERR_JLIC_LICENSE_DISABLED = 0x80057010;
constexpr int PRL_ERR_JLIC_WEB_PORTAL_ACCESS_REQUIRED = 0x80057012;
constexpr int PRL_ERR_NOT_ENOUGH_DISK_SPACE_TO_START_VM = 0x80000456;

// The PluginVmManagerImpl is responsible for connecting to the D-Bus services
// to manage the Plugin Vm.

class PluginVmManagerImpl : public PluginVmManager,
                            public ash::VmPluginDispatcherClient::Observer {
 public:
  using LaunchPluginVmCallback = base::OnceCallback<void(bool success)>;

  explicit PluginVmManagerImpl(Profile* profile);

  PluginVmManagerImpl(const PluginVmManagerImpl&) = delete;
  PluginVmManagerImpl& operator=(const PluginVmManagerImpl&) = delete;

  ~PluginVmManagerImpl() override;

  void OnPrimaryUserSessionStarted() override;

  // TODO(juwa): Don't allow launch/stop/uninstall to run simultaneously.
  // |callback| is called either when VM tools are ready or if an error occurs.
  void LaunchPluginVm(LaunchPluginVmCallback callback) override;
  void RelaunchPluginVm() override;
  void StopPluginVm(const std::string& name, bool force) override;
  void UninstallPluginVm() override;

  uint64_t seneschal_server_handle() const override;

  // ash::VmPluginDispatcherClient::Observer:
  void OnVmToolsStateChanged(
      const vm_tools::plugin_dispatcher::VmToolsStateChangedSignal& signal)
      override;
  void OnVmStateChanged(
      const vm_tools::plugin_dispatcher::VmStateChangedSignal& signal) override;

  void StartDispatcher(
      base::OnceCallback<void(bool success)> callback) const override;

  vm_tools::plugin_dispatcher::VmState vm_state() const override;

  bool IsRelaunchNeededForNewPermissions() const override;

  void AddVmStartingObserver(ash::VmStartingObserver* observer) override;
  void RemoveVmStartingObserver(ash::VmStartingObserver* observer) override;

  PluginVmUninstallerNotification* uninstaller_notification_for_testing()
      const {
    return uninstaller_notification_.get();
  }

 private:
  void InstallDlcAndUpdateVmState(
      base::OnceCallback<void(bool default_vm_exists)> success_callback,
      base::OnceClosure error_callback);
  void OnInstallPluginVmDlc(
      base::OnceCallback<void(bool default_vm_exists)> success_callback,
      base::OnceClosure error_callback,
      guest_os::GuestOsDlcInstallation::Result install_result);
  void OnStartDispatcher(
      base::OnceCallback<void(bool default_vm_exists)> success_callback,
      base::OnceClosure error_callback,
      bool success);
  void OnListVms(
      base::OnceCallback<void(bool default_vm_exists)> success_callback,
      base::OnceClosure error_callback,
      std::optional<vm_tools::plugin_dispatcher::ListVmResponse> reply);

  // The flow to launch a Plugin Vm. We'll probably want to add additional
  // abstraction around starting the services in the future but this is
  // sufficient for now.
  void OnListVmsForLaunch(bool default_vm_exists);
  void StartVm();
  void OnStartVm(
      std::optional<vm_tools::plugin_dispatcher::StartVmResponse> reply);
  void ShowVm();
  void OnShowVm(
      std::optional<vm_tools::plugin_dispatcher::ShowVmResponse> reply);
  void OnGetVmInfoForSharing(
      std::optional<vm_tools::concierge::GetVmInfoResponse> reply);
  void OnDefaultSharedDirExists(const base::FilePath& dir, bool exists);
  void UninstallSucceeded();

  // Called when LaunchPluginVm() is successful.
  void LaunchSuccessful();
  // Called when LaunchPluginVm() is unsuccessful.
  void LaunchFailed(PluginVmLaunchResult result = PluginVmLaunchResult::kError);

  // The flow to relaunch Plugin Vm.
  void OnSuspendVmForRelaunch(
      std::optional<vm_tools::plugin_dispatcher::SuspendVmResponse> reply);
  void OnRelaunchVmComplete(bool success);

  // The flow to uninstall Plugin Vm.
  void OnListVmsForUninstall(bool default_vm_exists);
  void StopVmForUninstall();
  void OnStopVmForUninstall(
      std::optional<vm_tools::plugin_dispatcher::StopVmResponse> reply);
  void DestroyDiskImage();
  void OnDestroyDiskImage(
      std::optional<vm_tools::concierge::DestroyDiskImageResponse> response);

  // Called when UninstallPluginVm() is unsuccessful.
  void UninstallFailed(
      PluginVmUninstallerNotification::FailedReason reason =
          PluginVmUninstallerNotification::FailedReason::kUnknown);

  // Called when Plugin VM changes availability e.g. installed, uninstalled,
  // policy changes.
  void OnAvailabilityChanged(bool is_allowed, bool is_configured);

  raw_ptr<Profile> profile_;
  std::string owner_id_;
  uint64_t seneschal_server_handle_ = 0;

  // State of the default VM's tools, kept up-to-date by signals from the
  // dispatcher.
  vm_tools::plugin_dispatcher::VmToolsState vm_tools_state_ =
      vm_tools::plugin_dispatcher::VmToolsState::VM_TOOLS_STATE_UNKNOWN;
  // State of the default VM, kept up-to-date by signals from the dispatcher.
  vm_tools::plugin_dispatcher::VmState vm_state_ =
      vm_tools::plugin_dispatcher::VmState::VM_STATE_UNKNOWN;

  base::ObserverList<ash::VmStartingObserver> vm_starting_observers_;

  std::unique_ptr<guest_os::GuestOsDlcInstallation> in_progress_installation_;

  // Members used in the launch flow.

  std::vector<LaunchPluginVmCallback> launch_vm_callbacks_;
  // We can't immediately start the VM when it is in states like suspending, so
  // delay until an in progress operation finishes.
  bool pending_start_vm_ = false;
  // |launch_vm_callbacks_| cannot be run before the vm tools are installed, so
  // delay until the tools are installed. This is set only after StartVm() runs
  // successfully.
  bool pending_vm_tools_installed_ = false;

  // Members used in the relaunch flow.

  // Indicates that we are attempting to start the VM. This fact may not yet
  // be reflected in VM state as the dispatcher may not have had a chance
  // to update it, or maybe it even is not yet aware that we issued StartVm
  // request.
  bool vm_is_starting_ = false;
  // Indicates that we are executing VM relaunch.
  bool relaunch_in_progress_ = false;
  // If we receive second or third relaunch request while already in the middle
  // of relaunch, we need to repeat it to ensure that privileges are set up
  // according to the latest settings.
  bool pending_relaunch_vm_ = false;

  // Members used in the uninstall flow

  std::unique_ptr<PluginVmUninstallerNotification> uninstaller_notification_;
  // We can't immediately destroy the VM when it is in states like
  // suspending, so delay until an in progress operation finishes.
  bool pending_destroy_disk_image_ = false;

  // For notifying the GuestOsSharePath.
  std::unique_ptr<PluginVmAvailabilitySubscription> availability_subscription_;

  base::WeakPtrFactory<PluginVmManagerImpl> weak_ptr_factory_{this};
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_MANAGER_IMPL_H_
