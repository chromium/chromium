// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/bind_helpers.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/chromeos/guest_os/guest_os_share_path.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_engagement_metrics_service.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_files.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_controller.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_item_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

namespace plugin_vm {

namespace {

constexpr char kInvalidLicenseNotificationId[] = "plugin-vm-invalid-license";
constexpr char kInvalidLicenseNotifierId[] = "plugin-vm-invalid-license";

class PluginVmManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static PluginVmManager* GetForProfile(Profile* profile) {
    return static_cast<PluginVmManager*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static PluginVmManagerFactory* GetInstance() {
    static base::NoDestructor<PluginVmManagerFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<PluginVmManagerFactory>;

  PluginVmManagerFactory()
      : BrowserContextKeyedServiceFactory(
            "PluginVmManager",
            BrowserContextDependencyManager::GetInstance()) {}

  ~PluginVmManagerFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    Profile* profile = Profile::FromBrowserContext(context);
    return new PluginVmManager(profile);
  }
};

// Checks if the VM is in a state in which we can't immediately start it.
bool VmIsStopping(vm_tools::plugin_dispatcher::VmState state) {
  return state == vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDING ||
         state == vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPING ||
         state == vm_tools::plugin_dispatcher::VmState::VM_STATE_RESETTING ||
         state == vm_tools::plugin_dispatcher::VmState::VM_STATE_PAUSING;
}

void ShowInvalidLicenseNotification(Profile* profile) {
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kInvalidLicenseNotificationId,
          l10n_util::GetStringUTF16(
              IDS_PLUGIN_VM_INVALID_LICENSE_NOTIFICATION_TITLE),
          l10n_util::GetStringUTF16(
              IDS_PLUGIN_VM_INVALID_LICENSE_NOTIFICATION_MESSAGE),
          l10n_util::GetStringUTF16(IDS_PLUGIN_VM_APP_NAME), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kInvalidLicenseNotifierId),
          {}, new message_center::NotificationDelegate(),
          kNotificationPluginVmIcon,
          message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);
  notification->SetSystemPriority();
  NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

}  // namespace

PluginVmManager* PluginVmManager::GetForProfile(Profile* profile) {
  return PluginVmManagerFactory::GetForProfile(profile);
}

PluginVmManager::PluginVmManager(Profile* profile)
    : profile_(profile),
      owner_id_(chromeos::ProfileHelper::GetUserIdHashFromProfile(profile)) {
  chromeos::DBusThreadManager::Get()
      ->GetVmPluginDispatcherClient()
      ->AddObserver(this);
}

PluginVmManager::~PluginVmManager() {
  chromeos::DBusThreadManager::Get()
      ->GetVmPluginDispatcherClient()
      ->RemoveObserver(this);
}

void PluginVmManager::LaunchPluginVm() {
  if (!IsPluginVmAllowedForProfile(profile_)) {
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
            kPluginVmAppId,
            std::make_unique<ShelfSpinnerItemController>(kPluginVmAppId));
  }

  // Launching Plugin Vm goes through the following steps:
  // 1) Start the Plugin Vm Dispatcher (no-op if already running)
  // 2) Call ListVms to get the state of the VM
  // 3) Start the VM if necessary
  // 4) Show the UI.
  chromeos::DBusThreadManager::Get()
      ->GetDebugDaemonClient()
      ->StartPluginVmDispatcher(
          owner_id_, base::BindOnce(&PluginVmManager::OnStartPluginVmDispatcher,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmManager::AddVmStartingObserver(
    chromeos::VmStartingObserver* observer) {
  vm_starting_observers_.AddObserver(observer);
}
void PluginVmManager::RemoveVmStartingObserver(
    chromeos::VmStartingObserver* observer) {
  vm_starting_observers_.RemoveObserver(observer);
}

void PluginVmManager::StopPluginVm(const std::string& name) {
  vm_tools::plugin_dispatcher::StopVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name_uuid(name);

  chromeos::DBusThreadManager::Get()->GetVmPluginDispatcherClient()->StopVm(
      std::move(request), base::DoNothing());
}

void PluginVmManager::OnVmStateChanged(
    const vm_tools::plugin_dispatcher::VmStateChangedSignal& signal) {
  if (signal.owner_id() != owner_id_ || signal.vm_name() != kPluginVmName)
    return;

  vm_state_ = signal.vm_state();

  if (pending_start_vm_ && !VmIsStopping(vm_state_))
    StartVm();

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
        base::BindOnce(&PluginVmManager::OnGetVmInfoForSharing,
                       weak_ptr_factory_.GetWeakPtr()));
  } else if (vm_state_ ==
                 vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED ||
             vm_state_ ==
                 vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDED) {
    // The previous seneschal handle is no longer valid.
    seneschal_server_handle_ = 0;

    ChromeLauncherController::instance()->Close(ash::ShelfID(kPluginVmAppId));
  }

  auto* engagement_metrics_service =
      PluginVmEngagementMetricsService::Factory::GetForProfile(profile_);
  // This is null in unit tests.
  if (engagement_metrics_service) {
    engagement_metrics_service->SetBackgroundActive(
        vm_state_ == vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING);
  }
}

void PluginVmManager::OnStartPluginVmDispatcher(bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to start Plugin Vm Dispatcher.";
    LaunchFailed();
    return;
  }

  vm_tools::plugin_dispatcher::ListVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name_uuid(kPluginVmName);

  chromeos::DBusThreadManager::Get()->GetVmPluginDispatcherClient()->ListVms(
      std::move(request), base::BindOnce(&PluginVmManager::OnListVms,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmManager::OnListVms(
    base::Optional<vm_tools::plugin_dispatcher::ListVmResponse> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to list VMs.";
    LaunchFailed();
    return;
  }
  if (reply->error() || reply->vm_info_size() != 1) {
    // Currently the error() field is set when the requested VM doesn't exist,
    // but having an empty vm_info list should also be a valid response.
    LOG(WARNING) << "Default VM is missing, it may have been manually removed.";
    profile_->GetPrefs()->SetBoolean(plugin_vm::prefs::kPluginVmImageExists,
                                     false);
    plugin_vm::ShowPluginVmLauncherView(profile_);
    LaunchFailed(PluginVmLaunchResult::kVmMissing);
    return;
  }

  // This is kept up to date in OnVmStateChanged, but the state will not yet be
  // set if we just started the dispatcher.
  vm_state_ = reply->vm_info(0).state();

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

void PluginVmManager::StartVm() {
  pending_start_vm_ = false;

  vm_tools::plugin_dispatcher::StartVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name_uuid(kPluginVmName);

  chromeos::DBusThreadManager::Get()->GetVmPluginDispatcherClient()->StartVm(
      std::move(request), base::BindOnce(&PluginVmManager::OnStartVm,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmManager::OnStartVm(
    base::Optional<vm_tools::plugin_dispatcher::StartVmResponse> reply) {
  if (reply &&
      reply->error() ==
          vm_tools::plugin_dispatcher::VmErrorCode::VM_ERR_LIC_NOT_VALID) {
    VLOG(1) << "Failed to start VM due to invalid license.";
    ShowInvalidLicenseNotification(profile_);
    LaunchFailed(PluginVmLaunchResult::kInvalidLicense);
    return;
  }

  if (!reply || reply->error()) {
    LOG(ERROR) << "Failed to start VM.";
    LaunchFailed();
    return;
  }

  ShowVm();
}

void PluginVmManager::ShowVm() {
  vm_tools::plugin_dispatcher::ShowVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name_uuid(kPluginVmName);

  chromeos::DBusThreadManager::Get()->GetVmPluginDispatcherClient()->ShowVm(
      std::move(request), base::BindOnce(&PluginVmManager::OnShowVm,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmManager::OnShowVm(
    base::Optional<vm_tools::plugin_dispatcher::ShowVmResponse> reply) {
  if (!reply.has_value() || reply->error()) {
    LOG(ERROR) << "Failed to show VM.";
    LaunchFailed();
    return;
  }

  VLOG(1) << "ShowVm completed successfully.";
  RecordPluginVmLaunchResultHistogram(PluginVmLaunchResult::kSuccess);
}

void PluginVmManager::OnGetVmInfoForSharing(
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
      profile_, base::BindOnce(&PluginVmManager::OnDefaultSharedDirExists,
                               weak_ptr_factory_.GetWeakPtr()));
  guest_os::GuestOsSharePath::GetForProfile(profile_)->SharePersistedPaths(
      kPluginVmName, base::DoNothing());
}

void PluginVmManager::OnDefaultSharedDirExists(const base::FilePath& dir,
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

void PluginVmManager::LaunchFailed(PluginVmLaunchResult result) {
  RecordPluginVmLaunchResultHistogram(result);

  ChromeLauncherController::instance()
      ->GetShelfSpinnerController()
      ->CloseSpinner(kPluginVmAppId);
}

}  // namespace plugin_vm
