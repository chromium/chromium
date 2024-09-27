// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_dlc_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path_factory.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_engagement_metrics_service.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_files.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_metrics_util.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_controller.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_item_controller.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

// This file contains VLOG logging to aid debugging tast tests.
#define LOG_FUNCTION_CALL() \
  VLOG(2) << "PluginVmManagerImpl::" << __func__ << " called"

namespace plugin_vm {

namespace {

PluginVmLaunchResult ConvertToLaunchResult(int result_code) {
  switch (result_code) {
    case PRL_ERR_SUCCESS:
      return PluginVmLaunchResult::kSuccess;
    case PRL_ERR_LICENSE_NOT_VALID:
    case PRL_ERR_LICENSE_WRONG_VERSION:
    case PRL_ERR_LICENSE_WRONG_PLATFORM:
    case PRL_ERR_LICENSE_BETA_KEY_RELEASE_PRODUCT:
    case PRL_ERR_LICENSE_RELEASE_KEY_BETA_PRODUCT:
    case PRL_ERR_JLIC_WRONG_HWID:
    case PRL_ERR_JLIC_LICENSE_DISABLED:
      return PluginVmLaunchResult::kInvalidLicense;
    case PRL_ERR_LICENSE_EXPIRED:
    case PRL_ERR_LICENSE_SUBSCR_EXPIRED:
      return PluginVmLaunchResult::kExpiredLicense;
    case PRL_ERR_JLIC_WEB_PORTAL_ACCESS_REQUIRED:
      return PluginVmLaunchResult::kNetworkError;
    case PRL_ERR_NOT_ENOUGH_DISK_SPACE_TO_START_VM:
      return PluginVmLaunchResult::kInsufficientDiskSpace;
    default:
      return PluginVmLaunchResult::kError;
  }
}

// Checks if the VM is in a state in which we can't immediately start it.
bool VmIsStopping(vm_tools::plugin_dispatcher::VmState state) {
  return state == vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDING ||
         state == vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPING ||
         state == vm_tools::plugin_dispatcher::VmState::VM_STATE_RESETTING ||
         state == vm_tools::plugin_dispatcher::VmState::VM_STATE_PAUSING;
}

bool VmIsStopped(vm_tools::plugin_dispatcher::VmState state) {
  return state == vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED ||
         state == vm_tools::plugin_dispatcher::VmState::VM_STATE_PAUSED ||
         state == vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDED;
}

void ShowStartVmFailedDialog(PluginVmLaunchResult result) {
  LOG(ERROR) << "Failed to start VM with launch result "
             << static_cast<int>(result);
  std::u16string app_name = l10n_util::GetStringUTF16(IDS_PLUGIN_VM_APP_NAME);
  std::u16string title;
  int message_id;
  switch (result) {
    default:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
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
    case PluginVmLaunchResult::kInsufficientDiskSpace:
      title = l10n_util::GetStringFUTF16(IDS_PLUGIN_VM_START_VM_ERROR_TITLE,
                                         app_name);
      message_id = IDS_PLUGIN_VM_START_VM_INSUFFICIENT_DISK_SPACE_ERROR_MESSAGE;
      break;
  }

  chrome::ShowWarningMessageBox(nullptr, std::move(title),
                                l10n_util::GetStringUTF16(message_id));
}

}  // namespace

PluginVmManagerImpl::PluginVmManagerImpl(Profile* profile)
    : profile_(profile),
      owner_id_(ash::ProfileHelper::GetUserIdHashFromProfile(profile)) {
  ash::VmPluginDispatcherClient::Get()->AddObserver(this);
  availability_subscription_ =
      std::make_unique<PluginVmAvailabilitySubscription>(
          profile_,
          base::BindRepeating(&PluginVmManagerImpl::OnAvailabilityChanged,
                              weak_ptr_factory_.GetWeakPtr()));

  if (PluginVmFeatures::Get()->IsEnabled(profile_)) {
    OnAvailabilityChanged(true, true);
  }
}

PluginVmManagerImpl::~PluginVmManagerImpl() {
  ash::VmPluginDispatcherClient::Get()->RemoveObserver(this);
}

void PluginVmManagerImpl::OnPrimaryUserSessionStarted() {
  vm_tools::plugin_dispatcher::ListVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name_uuid(kPluginVmName);

  // Probe the dispatcher.
  ash::VmPluginDispatcherClient::Get()->ListVms(
      std::move(request),
      base::BindOnce(
          [](std::optional<vm_tools::plugin_dispatcher::ListVmResponse> reply) {
            // If the dispatcher is already running here, Chrome probably
            // crashed. Restart it so it can bind to the new wayland socket.
            // TODO(b/149180115): Fix this properly.
            if (reply.has_value()) {
              LOG(ERROR) << "New session has dispatcher unexpected already "
                            "running. Perhaps Chrome crashed?";
              ash::DebugDaemonClient::Get()->StopPluginVmDispatcher(
                  base::BindOnce([](bool success) {
                    if (!success) {
                      LOG(ERROR) << "Failed to stop the dispatcher";
                    }
                  }));
            }
          }));
}

void PluginVmManagerImpl::LaunchPluginVm(LaunchPluginVmCallback callback) {
  LOG_FUNCTION_CALL();
  const bool launch_in_progress = !launch_vm_callbacks_.empty();
  launch_vm_callbacks_.push_back(std::move(callback));
  // If a launch is already in progress we don't need to do any more here.
  if (launch_in_progress) {
    VLOG(1) << "Launch already in progress";
    return;
  }

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
    ChromeShelfController::instance()
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
    ash::VmStartingObserver* observer) {
  vm_starting_observers_.AddObserver(observer);
}
void PluginVmManagerImpl::RemoveVmStartingObserver(
    ash::VmStartingObserver* observer) {
  vm_starting_observers_.RemoveObserver(observer);
}

void PluginVmManagerImpl::StopPluginVm(const std::string& name, bool force) {
  LOG_FUNCTION_CALL() << " with name = " << name << ", force = " << force;
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
  ash::VmPluginDispatcherClient::Get()->StopVm(std::move(request),
                                               base::DoNothing());
}

void PluginVmManagerImpl::RelaunchPluginVm() {
  LOG_FUNCTION_CALL();
  if (relaunch_in_progress_) {
    VLOG(1) << "Relaunch already in progress";
    pending_relaunch_vm_ = true;
    return;
  }

  relaunch_in_progress_ = true;

  vm_tools::plugin_dispatcher::SuspendVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name_uuid(kPluginVmName);

  // TODO(dtor): This may not work if the vm is STARTING|CONTINUING|RESUMING.
  ash::VmPluginDispatcherClient::Get()->SuspendVm(
      std::move(request),
      base::BindOnce(&PluginVmManagerImpl::OnSuspendVmForRelaunch,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmManagerImpl::OnSuspendVmForRelaunch(
    std::optional<vm_tools::plugin_dispatcher::SuspendVmResponse> reply) {
  LOG_FUNCTION_CALL();
  if (reply &&
      reply->error() == vm_tools::plugin_dispatcher::VmErrorCode::VM_SUCCESS) {
    LaunchPluginVm(base::BindOnce(&PluginVmManagerImpl::OnRelaunchVmComplete,
                                  weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  LOG(ERROR) << "Failed to suspend Plugin VM for relaunch";
}

void PluginVmManagerImpl::OnRelaunchVmComplete(bool success) {
  LOG_FUNCTION_CALL();
  relaunch_in_progress_ = false;

  if (!success) {
    LOG(ERROR) << "Failed to relaunch Plugin VM";
  } else if (pending_relaunch_vm_) {
    pending_relaunch_vm_ = false;
    RelaunchPluginVm();
  }
}

void PluginVmManagerImpl::UninstallPluginVm() {
  LOG_FUNCTION_CALL();
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
  LOG_FUNCTION_CALL() << ": {" << signal.owner_id() << ", " << signal.vm_name()
                      << ", " << signal.vm_tools_state() << "}";
  if (signal.owner_id() != owner_id_ || signal.vm_name() != kPluginVmName) {
    VLOG(1) << "Unexpected owner_id or vm_name";
    return;
  }

  vm_tools_state_ = signal.vm_tools_state();

  if (vm_tools_state_ ==
          vm_tools::plugin_dispatcher::VmToolsState::VM_TOOLS_STATE_INSTALLED &&
      pending_vm_tools_installed_) {
    pending_vm_tools_installed_ = false;
    LaunchSuccessful();
  }
}

void PluginVmManagerImpl::OnVmStateChanged(
    const vm_tools::plugin_dispatcher::VmStateChangedSignal& signal) {
  LOG_FUNCTION_CALL() << ": {" << signal.owner_id() << ", " << signal.vm_name()
                      << ", " << signal.vm_state() << "}";
  if (signal.owner_id() != owner_id_ || signal.vm_name() != kPluginVmName) {
    VLOG(1) << "Unexpected owner_id or vm_name";
    return;
  }

  vm_state_ = signal.vm_state();

  if (pending_start_vm_ && VmIsStopped(vm_state_)) {
    // We attempted to the launch when the VM was in the middle of stopping.
    VLOG(1) << "VM finished transition to a stopped state.";
    pending_start_vm_ = false;
    StartVm();
  }

  if (pending_vm_tools_installed_ &&
      (VmIsStopping(vm_state_) || VmIsStopped(vm_state_))) {
    // StartVm succeeded but the VM was stopped while waiting for the signal
    // indicating VM tools are installed.
    VLOG(1) << "VM stopped without tools installed.";
    pending_vm_tools_installed_ = false;
    LaunchFailed(PluginVmLaunchResult::kStoppedWaitingForVmTools);
  }

  if (pending_destroy_disk_image_ && !VmIsStopping(vm_state_)) {
    DestroyDiskImage();
  }

  // When the VM_STATE_RUNNING signal is received:
  // 1) Call Concierge::GetVmInfo to get seneschal server handle.
  // 2) Ensure default shared path exists.
  if (vm_state_ == vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING) {
    // If the VM was just created via VMC (instead of the installer), this flag
    // will not yet be set. Setting it here allows us to avoid showing the
    // installer when the user launches from the UI
    profile_->GetPrefs()->SetBoolean(plugin_vm::prefs::kPluginVmImageExists,
                                     true);

    vm_tools::concierge::GetVmInfoRequest concierge_request;
    concierge_request.set_owner_id(owner_id_);
    concierge_request.set_name(kPluginVmName);
    ash::ConciergeClient::Get()->GetVmInfo(
        std::move(concierge_request),
        base::BindOnce(&PluginVmManagerImpl::OnGetVmInfoForSharing,
                       weak_ptr_factory_.GetWeakPtr()));
  } else if (VmIsStopped(vm_state_)) {
    // The previous seneschal handle is no longer valid.
    seneschal_server_handle_ = 0;

    ChromeShelfController::instance()->Close(ash::ShelfID(kPluginVmShelfAppId));
  }

  auto* engagement_metrics_service =
      PluginVmEngagementMetricsService::Factory::GetForProfile(profile_);
  // This is null in unit tests.
  if (engagement_metrics_service) {
    engagement_metrics_service->SetBackgroundActive(
        vm_state_ == vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING);
  }
}

void PluginVmManagerImpl::StartDispatcher(
    base::OnceCallback<void(bool)> callback) const {
  LOG_FUNCTION_CALL();
  ash::DebugDaemonClient::Get()->StartPluginVmDispatcher(
      owner_id_, g_browser_process->GetApplicationLocale(),
      std::move(callback));
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
  LOG_FUNCTION_CALL();
  in_progress_installation_ =
      std::make_unique<guest_os::GuestOsDlcInstallation>(
          kPitaDlc,
          base::BindOnce(&PluginVmManagerImpl::OnInstallPluginVmDlc,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(success_callback),
                         std::move(error_callback)),
          base::DoNothing());
}

void PluginVmManagerImpl::OnInstallPluginVmDlc(
    base::OnceCallback<void(bool default_vm_exists)> success_callback,
    base::OnceClosure error_callback,
    guest_os::GuestOsDlcInstallation::Result install_result) {
  LOG_FUNCTION_CALL();
  if (install_result.has_value()) {
    StartDispatcher(base::BindOnce(
        &PluginVmManagerImpl::OnStartDispatcher, weak_ptr_factory_.GetWeakPtr(),
        std::move(success_callback), std::move(error_callback)));
  } else {
    LOG(ERROR) << "Couldn't install PluginVM DLC after import: "
               << install_result.error();
    std::move(error_callback).Run();
  }
}

void PluginVmManagerImpl::OnStartDispatcher(
    base::OnceCallback<void(bool)> success_callback,
    base::OnceClosure error_callback,
    bool success) {
  LOG_FUNCTION_CALL();
  if (!success) {
    LOG(ERROR) << "Failed to start Plugin Vm Dispatcher.";
    std::move(error_callback).Run();
    return;
  }

  vm_tools::plugin_dispatcher::ListVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name_uuid(kPluginVmName);

  ash::VmPluginDispatcherClient::Get()->ListVms(
      std::move(request),
      base::BindOnce(&PluginVmManagerImpl::OnListVms,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(success_callback), std::move(error_callback)));
}

void PluginVmManagerImpl::OnListVms(
    base::OnceCallback<void(bool)> success_callback,
    base::OnceClosure error_callback,
    std::optional<vm_tools::plugin_dispatcher::ListVmResponse> reply) {
  LOG_FUNCTION_CALL();
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
  LOG_FUNCTION_CALL();
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
  LOG_FUNCTION_CALL();
  // If the download from Drive got interrupted, ensure that the temporary image
  // and the containing directory get deleted.
  RemoveDriveDownloadDirectoryIfExists();

  vm_is_starting_ = true;

  vm_tools::plugin_dispatcher::StartVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name_uuid(kPluginVmName);

  ash::VmPluginDispatcherClient::Get()->StartVm(
      std::move(request), base::BindOnce(&PluginVmManagerImpl::OnStartVm,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmManagerImpl::OnStartVm(
    std::optional<vm_tools::plugin_dispatcher::StartVmResponse> reply) {
  PluginVmLaunchResult result;
  if (reply) {
    switch (reply->error()) {
      case vm_tools::plugin_dispatcher::VmErrorCode::VM_SUCCESS:
        result = PluginVmLaunchResult::kSuccess;
        break;
      case vm_tools::plugin_dispatcher::VmErrorCode::VM_ERR_NATIVE_RESULT_CODE:
        result = ConvertToLaunchResult(reply->result_code());
        break;
      default:
        result = PluginVmLaunchResult::kError;
        break;
    }
  } else {
    result = PluginVmLaunchResult::kError;
  }
  LOG_FUNCTION_CALL() << " with result = " << static_cast<int>(result);

  vm_is_starting_ = false;

  if (result != PluginVmLaunchResult::kSuccess) {
    ShowStartVmFailedDialog(result);
    LaunchFailed(result);
    return;
  }

  ShowVm();
}

void PluginVmManagerImpl::ShowVm() {
  LOG_FUNCTION_CALL();
  vm_tools::plugin_dispatcher::ShowVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name_uuid(kPluginVmName);

  ash::VmPluginDispatcherClient::Get()->ShowVm(
      std::move(request), base::BindOnce(&PluginVmManagerImpl::OnShowVm,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmManagerImpl::OnShowVm(
    std::optional<vm_tools::plugin_dispatcher::ShowVmResponse> reply) {
  LOG_FUNCTION_CALL();
  if (!reply.has_value() || reply->error()) {
    LOG(ERROR) << "Failed to show VM.";
    LaunchFailed();
    return;
  }

  RecordPluginVmLaunchResultHistogram(PluginVmLaunchResult::kSuccess);

  if (vm_tools_state_ ==
      vm_tools::plugin_dispatcher::VmToolsState::VM_TOOLS_STATE_INSTALLED) {
    LaunchSuccessful();
  } else {
    pending_vm_tools_installed_ = true;
  }
}

void PluginVmManagerImpl::OnGetVmInfoForSharing(
    std::optional<vm_tools::concierge::GetVmInfoResponse> reply) {
  LOG_FUNCTION_CALL();
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to get concierge VM info.";
    return;
  }
  if (!reply->success()) {
    LOG(ERROR) << "VM not started, cannot share paths";
    return;
  }
  seneschal_server_handle_ = reply->vm_info().seneschal_server_handle();

  // Create and share default folder.
  EnsureDefaultSharedDirExists(
      profile_, base::BindOnce(&PluginVmManagerImpl::OnDefaultSharedDirExists,
                               weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmManagerImpl::OnDefaultSharedDirExists(const base::FilePath& dir,
                                                   bool exists) {
  LOG_FUNCTION_CALL();
  if (exists) {
    guest_os::GuestOsSharePathFactory::GetForProfile(profile_)->SharePath(
        kPluginVmName, seneschal_server_handle_, dir,
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
  LOG_FUNCTION_CALL();
  DCHECK(!pending_start_vm_);
  DCHECK(!pending_vm_tools_installed_);

  std::vector<LaunchPluginVmCallback> observers;
  observers.swap(launch_vm_callbacks_);  // Ensure reentrancy.
  for (auto& observer : observers) {
    std::move(observer).Run(true);
  }
}

void PluginVmManagerImpl::LaunchFailed(PluginVmLaunchResult result) {
  LOG_FUNCTION_CALL();
  DCHECK(!pending_start_vm_);
  DCHECK(!pending_vm_tools_installed_);

  if (result == PluginVmLaunchResult::kVmMissing) {
    profile_->GetPrefs()->SetBoolean(plugin_vm::prefs::kPluginVmImageExists,
                                     false);
    plugin_vm::ShowPluginVmInstallerView(profile_);
  }

  RecordPluginVmLaunchResultHistogram(result);

  ChromeShelfController::instance()->GetShelfSpinnerController()->CloseSpinner(
      kPluginVmShelfAppId);

  std::vector<LaunchPluginVmCallback> observers;
  observers.swap(launch_vm_callbacks_);  // Ensure reentrancy.
  for (auto& observer : observers) {
    std::move(observer).Run(false);
  }
}

void PluginVmManagerImpl::OnListVmsForUninstall(bool default_vm_exists) {
  LOG_FUNCTION_CALL();
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
  LOG_FUNCTION_CALL();
  vm_tools::plugin_dispatcher::StopVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name_uuid(kPluginVmName);
  request.set_stop_mode(
      vm_tools::plugin_dispatcher::VmStopMode::VM_STOP_MODE_SHUTDOWN);

  ash::VmPluginDispatcherClient::Get()->StopVm(
      std::move(request),
      base::BindOnce(&PluginVmManagerImpl::OnStopVmForUninstall,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmManagerImpl::OnStopVmForUninstall(
    std::optional<vm_tools::plugin_dispatcher::StopVmResponse> reply) {
  LOG_FUNCTION_CALL();
  if (!reply || reply->error() != vm_tools::plugin_dispatcher::VM_SUCCESS) {
    LOG(ERROR) << "Failed to stop VM.";
    UninstallFailed(
        PluginVmUninstallerNotification::FailedReason::kStopVmFailed);
    return;
  }

  DestroyDiskImage();
}

void PluginVmManagerImpl::DestroyDiskImage() {
  LOG_FUNCTION_CALL();
  pending_destroy_disk_image_ = false;

  vm_tools::concierge::DestroyDiskImageRequest request;
  request.set_cryptohome_id(owner_id_);
  request.set_vm_name(kPluginVmName);

  ash::ConciergeClient::Get()->DestroyDiskImage(
      std::move(request),
      base::BindOnce(&PluginVmManagerImpl::OnDestroyDiskImage,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmManagerImpl::OnDestroyDiskImage(
    std::optional<vm_tools::concierge::DestroyDiskImageResponse> response) {
  LOG_FUNCTION_CALL();
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
  VLOG(1) << "UninstallPluginVm failed.";
  DCHECK(uninstaller_notification_);
  uninstaller_notification_->SetFailed(reason);
  uninstaller_notification_.reset();
}

void PluginVmManagerImpl::OnAvailabilityChanged(bool is_allowed,
                                                bool is_configured) {
  bool is_enabled = is_allowed && is_configured;
  auto* share_path = guest_os::GuestOsSharePathFactory::GetForProfile(profile_);
  guest_os::GuestId id{guest_os::VmType::PLUGIN_VM, kPluginVmName, ""};
  if (is_enabled) {
    share_path->RegisterGuest(id);
  } else {
    share_path->UnregisterGuest(id);
  }
}
}  // namespace plugin_vm

#undef LOG_FUNCTION_CALL
