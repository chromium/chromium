// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/guest_os/guest_os_share_path.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_engagement_metrics_service.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_files.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_metrics_util.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_controller.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_item_controller.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace plugin_vm {

namespace {

// Checks if the VM is in a state in which we can't immediately start it.
bool VmIsStopping(vm_tools::plugin_dispatcher::VmState state) {
  return state == vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDING ||
         state == vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPING ||
         state == vm_tools::plugin_dispatcher::VmState::VM_STATE_RESETTING ||
         state == vm_tools::plugin_dispatcher::VmState::VM_STATE_PAUSING;
}

void ShowStartVmFailedDialog(PluginVmLaunchResult result) {
  LOG(ERROR) << "Failed to start VM with launch result "
             << static_cast<int>(result);
  base::string16 app_name = l10n_util::GetStringUTF16(IDS_PLUGIN_VM_APP_NAME);
  base::string16 title;
  int message_id;
  switch (result) {
    default:
      NOTREACHED();
      FALLTHROUGH;
    case PluginVmLaunchResult::kError:
      title = l10n_util::GetStringFUTF16(IDS_PLUGIN_VM_START_VM_ERROR_TITLE,
                                         app_name);
      message_id = IDS_PLUGIN_VM_START_VM_ERROR_MESSAGE;
      break;
    case PluginVmLaunchResult::kInvalidLicense:
      title = l10n_util::GetStringFUTF16(IDS_PLUGIN_VM_INVALID_LICENSE_TITLE,
                                         app_name);
      message_id = IDS_PLUGIN_VM_INVALID_LICENSE_MESSAGE;
      break;
    case PluginVmLaunchResult::kExpiredLicense:
      title = l10n_util::GetStringFUTF16(IDS_PLUGIN_VM_EXPIRED_LICENSE_TITLE,
                                         app_name);
      message_id = IDS_PLUGIN_VM_INVALID_LICENSE_MESSAGE;
      break;
    case PluginVmLaunchResult::kNetworkError:
      title = l10n_util::GetStringFUTF16(IDS_PLUGIN_VM_START_VM_ERROR_TITLE,
                                         app_name);
      message_id = IDS_PLUGIN_VM_NETWORK_ERROR_MESSAGE;
      break;
  }

  chrome::ShowWarningMessageBox(nullptr, std::move(title),
                                l10n_util::GetStringUTF16(message_id));
}

}  // namespace

PluginVmManagerImpl::PluginVmManagerImpl(Profile* profile)
    : profile_(profile),
      owner_id_(chromeos::ProfileHelper::GetUserIdHashFromProfile(profile)) {
  chromeos::DBusThreadManager::Get()
      ->GetVmPluginDispatcherClient()
      ->AddObserver(this);
}

PluginVmManagerImpl::~PluginVmManagerImpl() {
  chromeos::DBusThreadManager::Get()
      ->GetVmPluginDispatcherClient()
      ->RemoveObserver(this);
}

void PluginVmManagerImpl::OnPrimaryUserProfilePrepared() {
  vm_tools::plugin_dispatcher::ListVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name_uuid(kPluginVmName);

  // Probe the dispatcher.
  chromeos::DBusThreadManager::Get()->GetVmPluginDispatcherClient()->ListVms(
      std::move(request),
      base::BindOnce(
          [](base::Optional<vm_tools::plugin_dispatcher::ListVmResponse>
                 reply) {
            // If the dispatcher is already running here, Chrome probably
            // crashed. Restart it so it can bind to the new wayland socket.
            // TODO(b/149180115): Fix this properly.
            if (reply.has_value()) {
              LOG(ERROR) << "New session has dispatcher unexpected already "
                            "running. Perhaps Chrome crashed?";
              chromeos::DBusThreadManager::Get()
                  ->GetDebugDaemonClient()
                  ->StopPluginVmDispatcher(base::BindOnce([](bool success) {
                    if (!success) {
                      LOG(ERROR) << "Failed to stop the dispatcher";
                    }
                  }));
            }
          }));
}

void PluginVmManagerImpl::LaunchPluginVm(LaunchPluginVmCallback callback) {
  const bool launch_in_progress = !launch_vm_callbacks_.empty();
  launch_vm_callbacks_.push_back(std::move(callback));
  // If a launch is already in progress we don't need to do any more here.
  if (launch_in_progress)
    return;

  if (!PluginVmFeatures::Get()->IsAllowed(profile_)) {
    LOG(ERROR) << "Attempted to launch PluginVm when it is not allowed";
    LaunchFailed();
    return;
  }

  for (auto& observer : vm_starting_observers_) {
    observer.OnVmStarting();
  }

  // Show a spinner for the first launch (state UNKNOWN) or if we will have to
  // wait before starting the VM.
  if (vm_state_ == vm_tools::plugin_dispatcher::VmState::VM_STATE_UNKNOWN ||
      VmIsStopping(vm_state_)) {
    ChromeLauncherController::instance()
        ->GetShelfSpinnerController()
        ->AddSpinnerToShelf(
            kPluginVmShelfAppId,
            std::make_unique<ShelfSpinnerItemController>(kPluginVmShelfAppId));
  }

  // Launching Plugin Vm goes through the following steps:
  // 1) Ensure the PluginVM DLC is installed.
  // 2) Start the Plugin Vm Dispatcher. (no-op if already running)
  // 3) Call ListVms to get the state of the VM.
  // 4) Start the VM if necessary.
  // 5) Show the UI.
  InstallDlcAndUpdateVmState(
      base::BindOnce(&PluginVmManagerImpl::OnListVmsForLaunch,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&PluginVmManagerImpl::LaunchFailed,
                     weak_ptr_factory_.GetWeakPtr(),
                     PluginVmLaunchResult::kError));
}

void PluginVmManagerImpl::AddVmStartingObserver(
    chromeos::VmStartingObserver* observer) {
  vm_starting_observers_.AddObserver(observer);
}
void PluginVmManagerImpl::RemoveVmStartingObserver(
    chromeos::VmStartingObserver* observer) {
  vm_starting_observers_.RemoveObserver(observer);
}

void PluginVmManagerImpl::StopPluginVm(const std::string& name, bool force) {
  vm_tools::plugin_dispatcher::StopVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name_uuid(name);

  if (force) {
    request.set_stop_mode(
        vm_tools::plugin_dispatcher::VmStopMode::VM_STOP_MODE_KILL);
  } else {
    request.set_stop_mode(
        vm_tools::plugin_dispatcher::VmStopMode::VM_STOP_MODE_SHUTDOWN);
  }

  // TODO(juwa): This may not work if the vm is STARTING|CONTINUING|RESUMING.
  chromeos::DBusThreadManager::Get()->GetVmPluginDispatcherClient()->StopVm(
      std::move(request), base::DoNothing());
}

void PluginVmManagerImpl::RelaunchPluginVm() {
  if (relaunch_in_progress_) {
    pending_relaunch_vm_ = true;
    return;
  }

  relaunch_in_progress_ = true;

  vm_tools::plugin_dispatcher::SuspendVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name_uuid(kPluginVmName);

  // TODO(dtor): This may not work if the vm is STARTING|CONTINUING|RESUMING.
  chromeos::DBusThreadManager::Get()->GetVmPluginDispatcherClient()->SuspendVm(
      std::move(request),
      base::BindOnce(&PluginVmManagerImpl::OnSuspendVmForRelaunch,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmManagerImpl::OnSuspendVmForRelaunch(
    base::Optional<vm_tools::plugin_dispatcher::SuspendVmResponse> reply) {
  if (reply &&
      reply->error() == vm_tools::plugin_dispatcher::VmErrorCode::VM_SUCCESS) {
    LaunchPluginVm(base::BindOnce(&PluginVmManagerImpl::OnRelaunchVmComplete,
                                  weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  LOG(ERROR) << "Failed to suspend Plugin VM for relaunch";
}

void PluginVmManagerImpl::OnRelaunchVmComplete(bool success) {
  relaunch_in_progress_ = false;

  if (!success) {
    LOG(ERROR) << "Failed to relaunch Plugin VM";
  } else if (pending_relaunch_vm_) {
    pending_relaunch_vm_ = false;
    RelaunchPluginVm();
  }
}

void PluginVmManagerImpl::UninstallPluginVm() {
  if (uninstaller_notification_) {
    uninstaller_notification_->ForceRedisplay();
    return;
  }

  uninstaller_notification_ =
      std::make_unique<PluginVmUninstallerNotification>(profile_);
  // Uninstalling Plugin Vm goes through the following steps:
  // 1) Ensure DLC is installed (otherwise we will not be able to start the
  //    dispatcher). Potentially, we can check and skip to 5) if it is not
  //    installed, but it is probably easier to just always go through the same
  //    flow.
  // 2) Start the Plugin Vm Dispatcher (no-op if already running)
  // 3) Call ListVms to get the state of the VM
  // 4) Stop the VM if necessary
  // 5) Uninstall the VM
  // It does not stop the dispatcher, as it will be stopped upon next shutdown
  InstallDlcAndUpdateVmState(
      base::BindOnce(&PluginVmManagerImpl::OnListVmsForUninstall,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&PluginVmManagerImpl::UninstallFailed,
                     weak_ptr_factory_.GetWeakPtr(),
                     PluginVmUninstallerNotification::FailedReason::kUnknown));
}

uint64_t PluginVmManagerImpl::seneschal_server_handle() const {
  return seneschal_server_handle_;
}

void PluginVmManagerImpl::OnVmToolsStateChanged(
    const vm_tools::plugin_dispatcher::VmToolsStateChangedSignal& signal) {
  if (signal.owner_id() != owner_id_ || signal.vm_name() != kPluginVmName) {
    return;
  }

  vm_tools_state_ = signal.vm_tools_state();

  if (vm_tools_state_ ==
          vm_tools::plugin_dispatcher::VmToolsState::VM_TOOLS_STATE_INSTALLED &&
      pending_vm_tools_installed_) {
    LaunchSuccessful();
  }
}

void PluginVmManagerImpl::OnVmStateChanged(
    const vm_tools::plugin_dispatcher::VmStateChangedSignal& signal) {
  if (signal.owner_id() != owner_id_ || signal.vm_name() != kPluginVmName)
    return;

  vm_state_ = signal.vm_state();

  if (pending_start_vm_ && !VmIsStopping(vm_state_))
    StartVm();
  if (pending_destroy_disk_image_ && !VmIsStopping(vm_state_))
    DestroyDiskImage();

  // When the VM_STATE_RUNNING signal is received:
  // 1) Call Concierge::GetVmInfo to get seneschal server handle.
  // 2) Ensure default shared path exists.
  // 3) Share paths with PluginVm
  if (vm_state_ == vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING) {
    vm_tools::concierge::GetVmInfoRequest concierge_request;
    concierge_request.set_owner_id(owner_id_);
    concierge_request.set_name(kPluginVmName);
    chromeos::DBusThreadManager::Get()->GetConciergeClient()->GetVmInfo(
        std::move(concierge_request),
        base::BindOnce(&PluginVmManagerImpl::OnGetVmInfoForSharing,
                       weak_ptr_factory_.GetWeakPtr()));
  } else if (vm_state_ ==
                 vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED ||
             vm_state_ ==
                 vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDED) {
    // The previous seneschal handle is no longer valid.
    seneschal_server_handle_ = 0;

    ChromeLauncherController::instance()->Close(
        ash::ShelfID(kPluginVmShelfAppId));
  }

  auto* engagement_metrics_service =
      PluginVmEngagementMetricsService::Factory::GetForProfile(profile_);
  // This is null in unit tests.
  if (engagement_metrics_service) {
    engagement_metrics_service->SetBackgroundActive(
        vm_state_ == vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING);
  }
}

void PluginVmManagerImpl::UpdateVmState(
    base::OnceCallback<void(bool)> success_callback,
    base::OnceClosure error_callback) {
  chromeos::DBusThreadManager::Get()
      ->GetDebugDaemonClient()
      ->StartPluginVmDispatcher(
          owner_id_, g_browser_process->GetApplicationLocale(),
          base::BindOnce(&PluginVmManagerImpl::OnStartDispatcher,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(success_callback),
                         std::move(error_callback)));
}

vm_tools::plugin_dispatcher::VmState PluginVmManagerImpl::vm_state() const {
  return vm_state_;
}

bool PluginVmManagerImpl::IsRelaunchNeededForNewPermissions() const {
  return vm_is_starting_ ||
         vm_state_ == vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING;
}

void PluginVmManagerImpl::InstallDlcAndUpdateVmState(
    base::OnceCallback<void(bool default_vm_exists)> success_callback,
    base::OnceClosure error_callback) {
  chromeos::DlcserviceClient::Get()->Install(
      "pita",
      base::BindOnce(&PluginVmManagerImpl::OnInstallPluginVmDlc,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(success_callback), std::move(error_callback)),
      base::DoNothing());
}

void PluginVmManagerImpl::OnInstallPluginVmDlc(
    base::OnceCallback<void(bool default_vm_exists)> success_callback,
    base::OnceClosure error_callback,
    const chromeos::DlcserviceClient::InstallResult& install_result) {
  if (install_result.error == dlcservice::kErrorNone) {
    UpdateVmState(std::move(success_callback), std::move(error_callback));
  } else {
    // TODO(kimjae): Unify the dlcservice error handler with
    // PluginVmInstaller.
    LOG(ERROR) << "Couldn't install PluginVM DLC after import: "
               << install_result.error;
    std::move(error_callback).Run();
  }
}

void PluginVmManagerImpl::OnStartDispatcher(
    base::OnceCallback<void(bool)> success_callback,
    base::OnceClosure error_callback,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to start Plugin Vm Dispatcher.";
    std::move(error_callback).Run();
    return;
  }

  vm_tools::plugin_dispatcher::ListVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name_uuid(kPluginVmName);

  chromeos::DBusThreadManager::Get()->GetVmPluginDispatcherClient()->ListVms(
      std::move(request),
      base::BindOnce(&PluginVmManagerImpl::OnListVms,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(success_callback), std::move(error_callback)));
}

void PluginVmManagerImpl::OnListVms(
    base::OnceCallback<void(bool)> success_callback,
    base::OnceClosure error_callback,
    base::Optional<vm_tools::plugin_dispatcher::ListVmResponse> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to list VMs.";
    std::move(error_callback).Run();
    return;
  }
  if (reply->vm_info_size() > 1) {
    LOG(ERROR) << "ListVms returned multiple results";
    std::move(error_callback).Run();
    return;
  }

  // Currently the error() field is set when the requested VM doesn't exist, but
  // having an empty vm_info list should also be a valid response.
  if (reply->error() || reply->vm_info_size() == 0) {
    vm_state_ = vm_tools::plugin_dispatcher::VmState::VM_STATE_UNKNOWN;
    std::move(success_callback).Run(false);
  } else {
    vm_state_ = reply->vm_info(0).state();
    std::move(success_callback).Run(true);
  }
}

void PluginVmManagerImpl::OnListVmsForLaunch(bool default_vm_exists) {
  if (!default_vm_exists) {
    LOG(WARNING) << "Default VM is missing, it may have been manually removed.";
    LaunchFailed(PluginVmLaunchResult::kVmMissing);
    return;
  }

  switch (vm_state_) {
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDING:
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_RESETTING:
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPING:
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_PAUSING:
      pending_start_vm_ = true;
      break;
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_STARTING:
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING:
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_CONTINUING:
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_RESUMING:
      ShowVm();
      break;
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED:
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_PAUSED:
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDED:
      StartVm();
      break;
    default:
      LOG(ERROR) << "Didn't start VM as it is in unexpected state "
                 << vm_state_;
      LaunchFailed();
      break;
  }
}

void PluginVmManagerImpl::StartVm() {
  // If the download from Drive got interrupted, ensure that the temporary image
  // and the containing directory get deleted.
  RemoveDriveDownloadDirectoryIfExists();

  pending_start_vm_ = false;
  vm_is_starting_ = true;

  vm_tools::plugin_dispatcher::StartVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name_uuid(kPluginVmName);

  chromeos::DBusThreadManager::Get()->GetVmPluginDispatcherClient()->StartVm(
      std::move(request), base::BindOnce(&PluginVmManagerImpl::OnStartVm,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmManagerImpl::OnStartVm(
    base::Optional<vm_tools::plugin_dispatcher::StartVmResponse> reply) {
  PluginVmLaunchResult result;
  if (reply) {
    switch (reply->error()) {
      case vm_tools::plugin_dispatcher::VmErrorCode::VM_SUCCESS:
        result = PluginVmLaunchResult::kSuccess;
        break;
      case vm_tools::plugin_dispatcher::VmErrorCode::VM_ERR_LIC_NOT_VALID:
        result = PluginVmLaunchResult::kInvalidLicense;
        break;
      case vm_tools::plugin_dispatcher::VmErrorCode::VM_ERR_LIC_EXPIRED:
        result = PluginVmLaunchResult::kExpiredLicense;
        break;
      case vm_tools::plugin_dispatcher::VmErrorCode::
          VM_ERR_LIC_WEB_PORTAL_UNAVAILABLE:
        result = PluginVmLaunchResult::kNetworkError;
        break;
      default:
        result = PluginVmLaunchResult::kError;
        break;
    }
  } else {
    result = PluginVmLaunchResult::kError;
  }

  vm_is_starting_ = false;

  if (result != PluginVmLaunchResult::kSuccess) {
    ShowStartVmFailedDialog(result);
    LaunchFailed(result);
    return;
  }

  ShowVm();
}

void PluginVmManagerImpl::ShowVm() {
  vm_tools::plugin_dispatcher::ShowVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name_uuid(kPluginVmName);

  chromeos::DBusThreadManager::Get()->GetVmPluginDispatcherClient()->ShowVm(
      std::move(request), base::BindOnce(&PluginVmManagerImpl::OnShowVm,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmManagerImpl::OnShowVm(
    base::Optional<vm_tools::plugin_dispatcher::ShowVmResponse> reply) {
  if (!reply.has_value() || reply->error()) {
    LOG(ERROR) << "Failed to show VM.";
    LaunchFailed();
    return;
  }

  VLOG(1) << "ShowVm completed successfully.";
  RecordPluginVmLaunchResultHistogram(PluginVmLaunchResult::kSuccess);

  if (vm_tools_state_ ==
      vm_tools::plugin_dispatcher::VmToolsState::VM_TOOLS_STATE_INSTALLED) {
    LaunchSuccessful();
  } else {
    pending_vm_tools_installed_ = true;
  }
}

void PluginVmManagerImpl::OnGetVmInfoForSharing(
    base::Optional<vm_tools::concierge::GetVmInfoResponse> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to get concierge VM info.";
    return;
  }
  if (!reply->success()) {
    LOG(ERROR) << "VM not started, cannot share paths";
    return;
  }
  seneschal_server_handle_ = reply->vm_info().seneschal_server_handle();

  // Create and share default folder, and other persisted shares.
  EnsureDefaultSharedDirExists(
      profile_, base::BindOnce(&PluginVmManagerImpl::OnDefaultSharedDirExists,
                               weak_ptr_factory_.GetWeakPtr()));
  guest_os::GuestOsSharePath::GetForProfile(profile_)->SharePersistedPaths(
      kPluginVmName, base::DoNothing());
}

void PluginVmManagerImpl::OnDefaultSharedDirExists(const base::FilePath& dir,
                                                   bool exists) {
  if (exists) {
    guest_os::GuestOsSharePath::GetForProfile(profile_)->SharePath(
        kPluginVmName, dir, false,
        base::BindOnce([](const base::FilePath& dir, bool success,
                          const std::string& failure_reason) {
          if (!success) {
            LOG(ERROR) << "Error sharing PluginVm default dir " << dir.value()
                       << ": " << failure_reason;
          }
        }));
  }
}

void PluginVmManagerImpl::LaunchSuccessful() {
  pending_start_vm_ = false;
  pending_vm_tools_installed_ = false;

  std::vector<LaunchPluginVmCallback> observers;
  observers.swap(launch_vm_callbacks_);  // Ensure reentrancy.
  for (auto& observer : observers) {
    std::move(observer).Run(true);
  }
}

void PluginVmManagerImpl::LaunchFailed(PluginVmLaunchResult result) {
  if (result == PluginVmLaunchResult::kVmMissing) {
    profile_->GetPrefs()->SetBoolean(plugin_vm::prefs::kPluginVmImageExists,
                                     false);
    plugin_vm::ShowPluginVmInstallerView(profile_);
  }

  RecordPluginVmLaunchResultHistogram(result);

  ChromeLauncherController::instance()
      ->GetShelfSpinnerController()
      ->CloseSpinner(kPluginVmShelfAppId);

  pending_start_vm_ = false;
  pending_vm_tools_installed_ = false;

  std::vector<LaunchPluginVmCallback> observers;
  observers.swap(launch_vm_callbacks_);  // Ensure reentrancy.
  for (auto& observer : observers) {
    std::move(observer).Run(false);
  }
}

void PluginVmManagerImpl::OnListVmsForUninstall(bool default_vm_exists) {
  if (!default_vm_exists) {
    LOG(WARNING) << "Default VM is missing, it may have been manually removed.";
    UninstallSucceeded();
    return;
  }

  switch (vm_state_) {
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDING:
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_RESETTING:
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPING:
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_PAUSING:
      pending_destroy_disk_image_ = true;
      break;
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_STARTING:
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING:
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_CONTINUING:
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_RESUMING:
      // TODO(juwa): This may not work if the vm is
      // STARTING|CONTINUING|RESUMING.
      StopVmForUninstall();
      break;
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED:
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_PAUSED:
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDED:
      DestroyDiskImage();
      break;
    default:
      LOG(ERROR) << "Didn't uninstall VM as it is in unexpected state "
                 << vm_state_;
      UninstallFailed();
      break;
  }
}

void PluginVmManagerImpl::StopVmForUninstall() {
  vm_tools::plugin_dispatcher::StopVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name_uuid(kPluginVmName);
  request.set_stop_mode(
      vm_tools::plugin_dispatcher::VmStopMode::VM_STOP_MODE_SHUTDOWN);

  chromeos::DBusThreadManager::Get()->GetVmPluginDispatcherClient()->StopVm(
      std::move(request),
      base::BindOnce(&PluginVmManagerImpl::OnStopVmForUninstall,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmManagerImpl::OnStopVmForUninstall(
    base::Optional<vm_tools::plugin_dispatcher::StopVmResponse> reply) {
  if (!reply || reply->error() != vm_tools::plugin_dispatcher::VM_SUCCESS) {
    LOG(ERROR) << "Failed to stop VM.";
    UninstallFailed(
        PluginVmUninstallerNotification::FailedReason::kStopVmFailed);
    return;
  }

  DestroyDiskImage();
}

void PluginVmManagerImpl::DestroyDiskImage() {
  pending_destroy_disk_image_ = false;

  vm_tools::concierge::DestroyDiskImageRequest request;
  request.set_cryptohome_id(owner_id_);
  request.set_disk_path(kPluginVmName);

  chromeos::DBusThreadManager::Get()->GetConciergeClient()->DestroyDiskImage(
      std::move(request),
      base::BindOnce(&PluginVmManagerImpl::OnDestroyDiskImage,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmManagerImpl::OnDestroyDiskImage(
    base::Optional<vm_tools::concierge::DestroyDiskImageResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to uninstall Plugin Vm. Received empty "
                  "DestroyDiskImageResponse.";
    UninstallFailed();
    return;
  }
  bool success =
      response->status() == vm_tools::concierge::DISK_STATUS_DESTROYED ||
      response->status() == vm_tools::concierge::DISK_STATUS_DOES_NOT_EXIST;
  if (!success) {
    LOG(ERROR) << "Failed to uninstall Plugin Vm. Received unsuccessful "
                  "DestroyDiskImageResponse."
               << response->status();
    UninstallFailed();
    return;
  }

  vm_state_ = vm_tools::plugin_dispatcher::VmState::VM_STATE_UNKNOWN;

  UninstallSucceeded();
}

void PluginVmManagerImpl::UninstallSucceeded() {
  VLOG(1) << "UninstallPluginVm completed successfully.";
  profile_->GetPrefs()->SetBoolean(plugin_vm::prefs::kPluginVmImageExists,
                                   false);
  // TODO(juwa): Potentially need to cleanup DLC here too.

  DCHECK(uninstaller_notification_);
  uninstaller_notification_->SetCompleted();
  uninstaller_notification_.reset();
}

void PluginVmManagerImpl::UninstallFailed(
    PluginVmUninstallerNotification::FailedReason reason) {
  DCHECK(uninstaller_notification_);
  uninstaller_notification_->SetFailed(reason);
  uninstaller_notification_.reset();
}

}  // namespace plugin_vm
