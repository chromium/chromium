// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chrome_browser_main_chromeos.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "ash/keyboard/ui/resources/keyboard_resource_util.h"
#include "ash/public/ash_interfaces.h"
#include "ash/public/cpp/event_rewriter_controller.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/linux_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_event_rewriter_delegate.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_mode_idle_app_name_notification.h"
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/chromeos/arc/session/arc_service_launcher.h"
#include "chrome/browser/chromeos/boot_times_recorder.h"
#include "chrome/browser/chromeos/crosapi/browser_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_unsupported_action_notifier.h"
#include "chrome/browser/chromeos/crostini/crosvm_metrics.h"
#include "chrome/browser/chromeos/dbus/chrome_features_service_provider.h"
#include "chrome/browser/chromeos/dbus/component_updater_service_provider.h"
#include "chrome/browser/chromeos/dbus/cryptohome_key_delegate_service_provider.h"
#include "chrome/browser/chromeos/dbus/dbus_helper.h"
#include "chrome/browser/chromeos/dbus/drive_file_stream_service_provider.h"
#include "chrome/browser/chromeos/dbus/kiosk_info_service_provider.h"
#include "chrome/browser/chromeos/dbus/libvda_service_provider.h"
#include "chrome/browser/chromeos/dbus/lock_to_single_user_service_provider.h"
#include "chrome/browser/chromeos/dbus/machine_learning_decision_service_provider.h"
#include "chrome/browser/chromeos/dbus/memory_pressure_service_provider.h"
#include "chrome/browser/chromeos/dbus/metrics_event_service_provider.h"
#include "chrome/browser/chromeos/dbus/mojo_connection_service_provider.h"
#include "chrome/browser/chromeos/dbus/plugin_vm_service_provider.h"
#include "chrome/browser/chromeos/dbus/printers_service_provider.h"
#include "chrome/browser/chromeos/dbus/proxy_resolution_service_provider.h"
#include "chrome/browser/chromeos/dbus/screen_lock_service_provider.h"
#include "chrome/browser/chromeos/dbus/smb_fs_service_provider.h"
#include "chrome/browser/chromeos/dbus/virtual_file_request_service_provider.h"
#include "chrome/browser/chromeos/dbus/vm/vm_permission_service_provider.h"
#include "chrome/browser/chromeos/dbus/vm_applications_service_provider.h"
#include "chrome/browser/chromeos/display/quirks_manager_delegate_impl.h"
#include "chrome/browser/chromeos/events/event_rewriter_delegate_impl.h"
#include "chrome/browser/chromeos/extensions/default_app_order.h"
#include "chrome/browser/chromeos/extensions/login_screen/login_screen_ui/ui_handler.h"
#include "chrome/browser/chromeos/external_metrics.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/lock_screen_apps/state_controller.h"
#include "chrome/browser/chromeos/logging.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_mode_resources_remover.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/login_screen_extensions_lifetime_manager.h"
#include "chrome/browser/chromeos/login/login_screen_extensions_storage_cleaner.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/session/chrome_session_manager.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/net/network_health/network_health_service.h"
#include "chrome/browser/chromeos/net/network_portal_detector_impl.h"
#include "chrome/browser/chromeos/net/network_pref_state_observer.h"
#include "chrome/browser/chromeos/net/network_throttling_observer.h"
#include "chrome/browser/chromeos/network_change_manager_client.h"
#include "chrome/browser/chromeos/note_taking_helper.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/lock_to_single_user_manager.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/controller.h"
#include "chrome/browser/chromeos/power/freezer_cgroup_process_manager.h"
#include "chrome/browser/chromeos/power/idle_action_warning_observer.h"
#include "chrome/browser/chromeos/power/ml/adaptive_screen_brightness_manager.h"
#include "chrome/browser/chromeos/power/power_data_collector.h"
#include "chrome/browser/chromeos/power/power_metrics_reporter.h"
#include "chrome/browser/chromeos/power/process_data_collector.h"
#include "chrome/browser/chromeos/power/renderer_freezer.h"
#include "chrome/browser/chromeos/power/smart_charging/smart_charging_manager.h"
#include "chrome/browser/chromeos/printing/bulk_printers_calculator_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/scheduler_configuration_manager.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/shutdown_policy_forwarder.h"
#include "chrome/browser/chromeos/startup_settings_cache.h"
#include "chrome/browser/chromeos/system/breakpad_consent_watcher.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/chromeos/system/user_removal_manager.h"
#include "chrome/browser/chromeos/system_token_cert_db_initializer.h"
#include "chrome/browser/chromeos/ui/gnubby_notification.h"
#include "chrome/browser/chromeos/ui/low_disk_notification.h"
#include "chrome/browser/chromeos/usb/cros_usb_detector.h"
#include "chrome/browser/chromeos/wilco_dtc_supportd/wilco_dtc_supportd_manager.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/ui/ash/assistant/assistant_client_impl.h"
#include "chrome/browser/ui/ash/assistant/assistant_state_client.h"
#include "chrome/browser/ui/ash/image_downloader_impl.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_manager.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chromeos/audio/audio_devices_pref_handler_impl.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/components/drivefs/fake_drivefs_launcher_client.h"
#include "chromeos/components/power/dark_resume_controller.h"
#include "chromeos/components/sensors/sensor_hal_dispatcher.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/constants/cryptohome_key_delegate_constants.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "chromeos/login/auth/login_event_recorder.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/login/session/session_termination_manager.h"
#include "chromeos/network/fast_transition_observer.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/portal_detector/network_portal_detector_stub.h"
#include "chromeos/services/cfm/public/buildflags/buildflags.h"  // PLATFORM_CFM
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/system/statistics_provider.h"
#include "chromeos/tpm/install_attributes.h"
#include "chromeos/tpm/tpm_token_loader.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_util.h"
#include "components/arc/enterprise/arc_data_snapshotd_manager.h"
#include "components/device_event_log/device_event_log.h"
#include "components/language/core/browser/pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/ownership/owner_key_util.h"
#include "components/prefs/pref_service.h"
#include "components/quirks/quirks_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/media_capture_devices.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_change_notifier_posix.h"
#include "printing/backend/print_backend.h"
#include "rlz/buildflags/buildflags.h"
#include "services/audio/public/cpp/sounds/sounds_manager.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "ui/base/pointer/pointer_device.h"
#include "ui/base/ui_base_features.h"
#include "ui/chromeos/events/pref_names.h"
#include "ui/events/event_utils.h"

#if BUILDFLAG(PLATFORM_CFM)
#include "chrome/browser/chromeos/cfm/cfm_chrome_services.h"
#endif

#if BUILDFLAG(ENABLE_RLZ)
#include "components/rlz/rlz_tracker.h"
#endif

namespace chromeos {

namespace {

void ChromeOSVersionCallback(const std::string& version) {
  base::SetLinuxDistro(std::string("CrOS ") + version);
}

bool ShouldAutoLaunchKioskApp(const base::CommandLine& command_line) {
  KioskAppManager* app_manager = KioskAppManager::Get();
  WebKioskAppManager* web_app_manager = WebKioskAppManager::Get();
  ArcKioskAppManager* arc_app_manager = ArcKioskAppManager::Get();
  return command_line.HasSwitch(switches::kLoginManager) &&
         (app_manager->IsAutoLaunchEnabled() ||
          web_app_manager->GetAutoLaunchAccountId().is_valid() ||
          arc_app_manager->GetAutoLaunchAccountId().is_valid()) &&
         KioskAppLaunchError::Get() == KioskAppLaunchError::NONE;
}

// Creates an instance of the NetworkPortalDetector implementation or a stub.
void InitializeNetworkPortalDetector() {
  if (network_portal_detector::SetForTesting())
    return;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kTestType)) {
    network_portal_detector::SetNetworkPortalDetector(
        new NetworkPortalDetectorStub());
  } else {
    network_portal_detector::SetNetworkPortalDetector(
        new NetworkPortalDetectorImpl());
  }
}

}  // namespace

namespace internal {

// Wrapper class for initializing D-Bus services and shutting them down.
class DBusServices {
 public:
  explicit DBusServices(const content::MainFunctionParams& parameters) {
    PowerPolicyController::Initialize(PowerManagerClient::Get());

    dbus::Bus* system_bus = DBusThreadManager::Get()->IsUsingFakes()
                                ? nullptr
                                : DBusThreadManager::Get()->GetSystemBus();

    // See also PostBrowserStart() where machine_learning_decision_service_ is
    // initialized.

    proxy_resolution_service_ = CrosDBusService::Create(
        system_bus, kNetworkProxyServiceName,
        dbus::ObjectPath(kNetworkProxyServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<ProxyResolutionServiceProvider>()));

    kiosk_info_service_ =
        CrosDBusService::Create(system_bus, kKioskAppServiceName,
                                dbus::ObjectPath(kKioskAppServicePath),
                                CrosDBusService::CreateServiceProviderList(
                                    std::make_unique<KioskInfoService>()));

    metrics_event_service_ = CrosDBusService::Create(
        system_bus, kMetricsEventServiceName,
        dbus::ObjectPath(kMetricsEventServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<MetricsEventServiceProvider>()));

    plugin_vm_service_ = CrosDBusService::Create(
        system_bus, kPluginVmServiceName,
        dbus::ObjectPath(kPluginVmServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<PluginVmServiceProvider>()));

    screen_lock_service_ = CrosDBusService::Create(
        system_bus, kScreenLockServiceName,
        dbus::ObjectPath(kScreenLockServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<ScreenLockServiceProvider>()));

    virtual_file_request_service_ = CrosDBusService::Create(
        system_bus, kVirtualFileRequestServiceName,
        dbus::ObjectPath(kVirtualFileRequestServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<VirtualFileRequestServiceProvider>()));

    component_updater_service_ = CrosDBusService::Create(
        system_bus, kComponentUpdaterServiceName,
        dbus::ObjectPath(kComponentUpdaterServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<ComponentUpdaterServiceProvider>(
                g_browser_process->platform_part()
                    ->cros_component_manager()
                    .get())));

    chrome_features_service_ = CrosDBusService::Create(
        system_bus, kChromeFeaturesServiceName,
        dbus::ObjectPath(kChromeFeaturesServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<ChromeFeaturesServiceProvider>()));

    printers_service_ = CrosDBusService::Create(
        system_bus, kPrintersServiceName,
        dbus::ObjectPath(kPrintersServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<PrintersServiceProvider>()));

    vm_applications_service_ = CrosDBusService::Create(
        system_bus, vm_tools::apps::kVmApplicationsServiceName,
        dbus::ObjectPath(vm_tools::apps::kVmApplicationsServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<VmApplicationsServiceProvider>()));

    vm_permission_service_ = CrosDBusService::Create(
        system_bus, kVmPermissionServiceName,
        dbus::ObjectPath(kVmPermissionServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<VmPermissionServiceProvider>()));

    drive_file_stream_service_ = CrosDBusService::Create(
        system_bus, drivefs::kDriveFileStreamServiceName,
        dbus::ObjectPath(drivefs::kDriveFileStreamServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<DriveFileStreamServiceProvider>()));

    cryptohome_key_delegate_service_ = CrosDBusService::Create(
        system_bus, cryptohome::kCryptohomeKeyDelegateServiceName,
        dbus::ObjectPath(cryptohome::kCryptohomeKeyDelegateServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<CryptohomeKeyDelegateServiceProvider>()));

    smb_fs_service_ =
        CrosDBusService::Create(system_bus, smbfs::kSmbFsServiceName,
                                dbus::ObjectPath(smbfs::kSmbFsServicePath),
                                CrosDBusService::CreateServiceProviderList(
                                    std::make_unique<SmbFsServiceProvider>()));
    lock_to_single_user_service_ = CrosDBusService::Create(
        system_bus, lock_to_single_user::kLockToSingleUserServiceName,
        dbus::ObjectPath(lock_to_single_user::kLockToSingleUserServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<LockToSingleUserServiceProvider>()));

    memory_pressure_service_ = CrosDBusService::Create(
        system_bus, memory_pressure::kMemoryPressureServiceName,
        dbus::ObjectPath(memory_pressure::kMemoryPressureServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<MemoryPressureServiceProvider>()));

    mojo_connection_service_ = CrosDBusService::Create(
        system_bus,
        ::mojo_connection_service::kMojoConnectionServiceServiceName,
        dbus::ObjectPath(
            ::mojo_connection_service::kMojoConnectionServiceServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<MojoConnectionServiceProvider>()));

    if (arc::IsArcVmEnabled()) {
      libvda_service_ = CrosDBusService::Create(
          system_bus, libvda::kLibvdaServiceName,
          dbus::ObjectPath(libvda::kLibvdaServicePath),
          CrosDBusService::CreateServiceProviderList(
              std::make_unique<LibvdaServiceProvider>()));
    }

    // Initialize PowerDataCollector after DBusThreadManager is initialized.
    PowerDataCollector::Initialize();
    ProcessDataCollector::Initialize();

    LoginState::Initialize();
    TPMTokenLoader::Initialize();
    NetworkCertLoader::Initialize();

    disks::DiskMountManager::Initialize();
    cryptohome::AsyncMethodCaller::Initialize();
    cryptohome::HomedirMethods::Initialize();

    NetworkHandler::Initialize();

    sensors::SensorHalDispatcher::Initialize();

    DeviceSettingsService::Get()->SetSessionManager(
        SessionManagerClient::Get(),
        OwnerSettingsServiceChromeOSFactory::GetInstance()->GetOwnerKeyUtil());
  }

  void CreateMachineLearningDecisionProvider() {
    dbus::Bus* system_bus = DBusThreadManager::Get()->IsUsingFakes()
                                ? nullptr
                                : DBusThreadManager::Get()->GetSystemBus();
    // TODO(alanlxl): update Ml here to MachineLearning after powerd is
    // uprevved.
    machine_learning_decision_service_ = CrosDBusService::Create(
        system_bus, kMlDecisionServiceName,
        dbus::ObjectPath(kMlDecisionServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<MachineLearningDecisionServiceProvider>()));
  }

  ~DBusServices() {
    sensors::SensorHalDispatcher::Shutdown();
    NetworkHandler::Shutdown();
    cryptohome::AsyncMethodCaller::Shutdown();
    disks::DiskMountManager::Shutdown();
    LoginState::Shutdown();
    NetworkCertLoader::Shutdown();
    TPMTokenLoader::Shutdown();
    proxy_resolution_service_.reset();
    kiosk_info_service_.reset();
    metrics_event_service_.reset();
    plugin_vm_service_.reset();
    printers_service_.reset();
    virtual_file_request_service_.reset();
    component_updater_service_.reset();
    chrome_features_service_.reset();
    vm_applications_service_.reset();
    vm_permission_service_.reset();
    drive_file_stream_service_.reset();
    cryptohome_key_delegate_service_.reset();
    lock_to_single_user_service_.reset();
    memory_pressure_service_.reset();
    mojo_connection_service_.reset();
    ProcessDataCollector::Shutdown();
    PowerDataCollector::Shutdown();
    PowerPolicyController::Shutdown();
    device::BluetoothAdapterFactory::Shutdown();
  }

  void PreAshShutdown() {
    // Services depending on ash should be released here.
    machine_learning_decision_service_.reset();
  }

 private:
  std::unique_ptr<CrosDBusService> proxy_resolution_service_;
  std::unique_ptr<CrosDBusService> kiosk_info_service_;
  std::unique_ptr<CrosDBusService> metrics_event_service_;
  std::unique_ptr<CrosDBusService> plugin_vm_service_;
  std::unique_ptr<CrosDBusService> printers_service_;
  std::unique_ptr<CrosDBusService> screen_lock_service_;
  std::unique_ptr<CrosDBusService> virtual_file_request_service_;
  std::unique_ptr<CrosDBusService> component_updater_service_;
  std::unique_ptr<CrosDBusService> chrome_features_service_;
  std::unique_ptr<CrosDBusService> vm_applications_service_;
  std::unique_ptr<CrosDBusService> vm_permission_service_;
  std::unique_ptr<CrosDBusService> drive_file_stream_service_;
  std::unique_ptr<CrosDBusService> cryptohome_key_delegate_service_;
  std::unique_ptr<CrosDBusService> libvda_service_;
  std::unique_ptr<CrosDBusService> machine_learning_decision_service_;
  std::unique_ptr<CrosDBusService> smb_fs_service_;
  std::unique_ptr<CrosDBusService> lock_to_single_user_service_;
  std::unique_ptr<CrosDBusService> memory_pressure_service_;
  std::unique_ptr<CrosDBusService> mojo_connection_service_;

  DISALLOW_COPY_AND_ASSIGN(DBusServices);
};

}  // namespace internal

// ChromeBrowserMainPartsChromeos ----------------------------------------------

ChromeBrowserMainPartsChromeos::ChromeBrowserMainPartsChromeos(
    const content::MainFunctionParams& parameters,
    StartupData* startup_data)
    : ChromeBrowserMainPartsLinux(parameters, startup_data) {}

ChromeBrowserMainPartsChromeos::~ChromeBrowserMainPartsChromeos() {
  // To be precise, logout (browser shutdown) is not yet done, but the
  // remaining work is negligible, hence we say LogoutDone here.
  BootTimesRecorder::Get()->AddLogoutTimeMarker("LogoutDone", false);
  BootTimesRecorder::Get()->WriteLogoutTimes();
}

// content::BrowserMainParts and ChromeBrowserMainExtraParts overrides ---------

int ChromeBrowserMainPartsChromeos::PreEarlyInitialization() {
  base::CommandLine* singleton_command_line =
      base::CommandLine::ForCurrentProcess();

  if (parsed_command_line().HasSwitch(switches::kGuestSession)) {
    // Disable sync and extensions if we're in "browse without sign-in" mode.
    singleton_command_line->AppendSwitch(::switches::kDisableSync);
    singleton_command_line->AppendSwitch(::switches::kDisableExtensions);
    browser_defaults::bookmarks_enabled = false;
  }

  // If we're not running on real Chrome OS hardware (or under VM), and are not
  // showing the login manager or attempting a command line login, login with a
  // stub user.
  if (!base::SysInfo::IsRunningOnChromeOS() &&
      !parsed_command_line().HasSwitch(switches::kLoginManager) &&
      !parsed_command_line().HasSwitch(switches::kLoginUser) &&
      !parsed_command_line().HasSwitch(switches::kGuestSession)) {
    singleton_command_line->AppendSwitchASCII(
        switches::kLoginUser,
        cryptohome::Identification(user_manager::StubAccountId()).id());
    if (!parsed_command_line().HasSwitch(switches::kLoginProfile)) {
      singleton_command_line->AppendSwitchASCII(switches::kLoginProfile,
                                                chrome::kTestUserProfileDir);
    }
    LOG(WARNING) << "Running as stub user with profile dir: "
                 << singleton_command_line
                        ->GetSwitchValuePath(switches::kLoginProfile)
                        .value();
  }

  // DBus is initialized in ChromeMainDelegate::PostEarlyInitialization().
  CHECK(DBusThreadManager::IsInitialized());

#if !defined(USE_REAL_DBUS_CLIENTS)
  if (!base::SysInfo::IsRunningOnChromeOS() &&
      parsed_command_line().HasSwitch(
          switches::kFakeDriveFsLauncherChrootPath) &&
      parsed_command_line().HasSwitch(
          switches::kFakeDriveFsLauncherSocketPath)) {
    drivefs::FakeDriveFsLauncherClient::Init(
        parsed_command_line().GetSwitchValuePath(
            switches::kFakeDriveFsLauncherChrootPath),
        parsed_command_line().GetSwitchValuePath(
            switches::kFakeDriveFsLauncherSocketPath));
  }
#endif  // !defined(USE_REAL_DBUS_CLIENTS)

  return ChromeBrowserMainPartsLinux::PreEarlyInitialization();
}

void ChromeBrowserMainPartsChromeos::PreMainMessageLoopStart() {
  // Initialize session manager in early stage in case others want to listen
  // to session state change right after browser is started.
  g_browser_process->platform_part()->InitializeSessionManager();

  ChromeBrowserMainPartsLinux::PreMainMessageLoopStart();
}

void ChromeBrowserMainPartsChromeos::PostMainMessageLoopStart() {
  // device_event_log must be initialized after the message loop.
  device_event_log::Initialize(0 /* default max entries */);

  // This has to be initialized before DBusServices
  // (ComponentUpdaterServiceProvider).
  g_browser_process->platform_part()->InitializeCrosComponentManager();

  dbus_services_.reset(new internal::DBusServices(parameters()));

  // Need to be done after LoginState has been initialized in DBusServices().
  memory::MemoryKillsMonitor::Initialize();

  ChromeBrowserMainPartsLinux::PostMainMessageLoopStart();
}

// Threads are initialized between MainMessageLoopStart and MainMessageLoopRun.
// about_flags settings are applied in ChromeBrowserMainParts::PreCreateThreads.
void ChromeBrowserMainPartsChromeos::PreMainMessageLoopRun() {
  network_change_manager_client_.reset(new NetworkChangeManagerClient(
      static_cast<net::NetworkChangeNotifierPosix*>(
          content::GetNetworkChangeNotifier())));

  // Set the crypto thread after the IO thread has been created/started.
  TPMTokenLoader::Get()->SetCryptoTaskRunner(
      content::GetIOThreadTaskRunner({}));

  // Initialize NSS database for system token.
  system_token_certdb_initializer_ =
      std::make_unique<SystemTokenCertDBInitializer>();

  mojo::PendingRemote<media_session::mojom::MediaControllerManager>
      media_controller_manager;
  content::GetMediaSessionService().BindMediaControllerManager(
      media_controller_manager.InitWithNewPipeAndPassReceiver());
  CrasAudioHandler::Initialize(
      std::move(media_controller_manager),
      new AudioDevicesPrefHandlerImpl(g_browser_process->local_state()));

  content::MediaCaptureDevices::GetInstance()->AddVideoCaptureObserver(
      CrasAudioHandler::Get());

  quirks::QuirksManager::Initialize(
      std::unique_ptr<quirks::QuirksManager::Delegate>(
          new quirks::QuirksManagerDelegateImpl()),
      g_browser_process->local_state(),
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory());

  // Start loading machine statistics here. StatisticsProvider::Shutdown()
  // will ensure that loading is aborted on early exit.
  bool load_oem_statistics = !StartupUtils::IsOobeCompleted();
  system::StatisticsProvider::GetInstance()->StartLoadingMachineStatistics(
      load_oem_statistics);

  base::FilePath downloads_directory;
  CHECK(base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS,
                               &downloads_directory));

  DeviceOAuth2TokenServiceFactory::Initialize(
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory(),
      g_browser_process->local_state());

  fast_transition_observer_.reset(
      new FastTransitionObserver(g_browser_process->local_state()));
  network_throttling_observer_.reset(
      new NetworkThrottlingObserver(g_browser_process->local_state()));

  discover_manager_ = std::make_unique<DiscoverManager>();

  g_browser_process->platform_part()->InitializeSchedulerConfigurationManager();
  arc_service_launcher_ = std::make_unique<arc::ArcServiceLauncher>(
      g_browser_process->platform_part()->scheduler_configuration_manager());

  session_termination_manager_ =
      std::make_unique<chromeos::SessionTerminationManager>();

  // This should be in PreProfileInit but it needs to be created before the
  // policy connector is started.
  bulk_printers_calculator_factory_ =
      std::make_unique<BulkPrintersCalculatorFactory>();

  // StatsReportingController is created in
  // ChromeBrowserMainParts::PreCreateThreads, so this must come afterwards.
  chromeos::StatsReportingController* stats_controller =
      chromeos::StatsReportingController::Get();
  // |stats_controller| can be nullptr if ChromeBrowserMainParts's
  // browser_process_->GetApplicationLocale() returns empty. We're trying to
  // show an error message in that case, so don't just crash. (See
  // ChromeBrowserMainParts::PreCreateThreadsImpl()).
  if (stats_controller != nullptr) {
    breakpad_consent_watcher_ =
        system::BreakpadConsentWatcher::Initialize(stats_controller);
  }

#if BUILDFLAG(PLATFORM_CFM)
  chromeos::cfm::InitializeCfmServices();
#endif  // BUILDFLAG(PLATFORM_CFM)

  ChromeBrowserMainPartsLinux::PreMainMessageLoopRun();
}

void ChromeBrowserMainPartsChromeos::PreProfileInit() {
  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- immediately before Profile creation().

  // PreProfileInit() is not always called if no browser process is started
  // (e.g. during some browser tests). Set a boolean so that we do not try to
  // destroy singletons that are initialized here.
  pre_profile_init_called_ = true;

  // Now that the file thread exists we can record our stats.
  BootTimesRecorder::Get()->RecordChromeMainStats();
  LoginEventRecorder::Get()->SetDelegate(BootTimesRecorder::Get());

  // Trigger prefetching of ownership status.
  DeviceSettingsService::Get()->Load();

  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- just before CreateProfile().

  g_browser_process->platform_part()->InitializeChromeUserManager();

  arc_data_snapshotd_manager_ =
      std::make_unique<arc::data_snapshotd::ArcDataSnapshotdManager>(
          g_browser_process->local_state());
  if (base::FeatureList::IsEnabled(::features::kWilcoDtc))
    wilco_dtc_supportd_manager_ = std::make_unique<WilcoDtcSupportdManager>();

  ScreenLocker::InitClass();

  // This forces the ProfileManager to be created and register for the
  // notification it needs to track the logged in user.
  g_browser_process->profile_manager();

  // Must come after User Manager is inited.
  lock_to_single_user_manager_ =
      std::make_unique<policy::LockToSingleUserManager>();

  // AccessibilityManager and SystemKeyEventListener use InputMethodManager.
  input_method::Initialize();

  // keyboard::KeyboardController initializes ChromeKeyboardUI which depends
  // on ChromeKeyboardControllerClient.
  chrome_keyboard_controller_client_ = ChromeKeyboardControllerClient::Create();

  // ProfileHelper has to be initialized after UserManager instance is created.
  ProfileHelper::Get()->Initialize();

  // If kLoginUser is passed this indicates that user has already
  // logged in and we should behave accordingly.
  bool immediate_login = parsed_command_line().HasSwitch(switches::kLoginUser);
  if (immediate_login) {
    // Redirects Chrome logging to the user data dir.
    logging::RedirectChromeLogging(parsed_command_line());

    // Load the default app order synchronously for restarting case.
    app_order_loader_.reset(
        new default_app_order::ExternalLoader(false /* async */));
  }

  if (!app_order_loader_) {
    app_order_loader_.reset(
        new default_app_order::ExternalLoader(true /* async */));
  }

  audio::SoundsManager::Create(content::GetAudioServiceStreamFactoryBinder());

  // |arc_service_launcher_| must be initialized before NoteTakingHelper.
  NoteTakingHelper::Initialize();

  AccessibilityManager::Initialize();

  // Initialize magnification manager before ash tray is created. And this
  // must be placed after UserManager initialization.
  MagnificationManager::Initialize();

  // Has to be initialized before |assistant_client_|;
  image_downloader_ = std::make_unique<ImageDownloaderImpl>();

  // Requires UserManager.
  assistant_state_client_ = std::make_unique<AssistantStateClient>();

  // Assistant has to be initialized before
  // ChromeBrowserMainExtraPartsAsh::session_controller_client_ to avoid race of
  // SessionChanged event and assistant_client initialization. It must come
  // after AssistantStateClient.
  assistant_client_ = std::make_unique<AssistantClientImpl>();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&version_loader::GetVersion, version_loader::VERSION_FULL),
      base::BindOnce(&ChromeOSVersionCallback));

  arc_kiosk_app_manager_.reset(new ArcKioskAppManager());
  web_kiosk_app_manager_.reset(new WebKioskAppManager());

  // Make sure that wallpaper boot transition and other delays in OOBE
  // are disabled for tests and kiosk app launch by default.
  // Individual tests may enable them if they want.
  if (parsed_command_line().HasSwitch(::switches::kTestType) ||
      ShouldAutoLaunchKioskApp(parsed_command_line())) {
    WizardController::SetZeroDelays();
  }

  // On Chrome OS, Chrome does not exit when all browser windows are closed.
  // UnregisterKeepAlive is called from chrome::HandleAppExitingForPlatform.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kDisableZeroBrowsersOpenForTests)) {
    g_browser_process->platform_part()->RegisterKeepAlive();
  }

  // NOTE: Calls ChromeBrowserMainParts::PreProfileInit() which calls
  // ChromeBrowserMainExtraPartsAsh::PreProfileInit() which initializes
  // ash::Shell.
  ChromeBrowserMainPartsLinux::PreProfileInit();

  // ash::Shell must be initialized before we can ask Ash to bind a
  // CrosDisplayConfigController.
  mojo::PendingRemote<ash::mojom::CrosDisplayConfigController> display_config;
  ash::BindCrosDisplayConfigController(
      display_config.InitWithNewPipeAndPassReceiver());
  arc_service_launcher_->Initialize(std::move(display_config));

  // Needs to be initialized after ash::Shell.
  chrome_keyboard_controller_client_->Init(ash::KeyboardController::Get());

  // Initialize the keyboard before any session state changes (i.e. before
  // loading the default profile).
  keyboard::InitializeKeyboardResources();

  lock_screen_apps_state_controller_ =
      std::make_unique<lock_screen_apps::StateController>();
  lock_screen_apps_state_controller_->Initialize();

  // Always construct BrowserManager, even if the lacros flag is disabled, so
  // it can do cleanup work if needed. Initialized in PreProfileInit because the
  // profile-keyed service AppService can call into it.
  browser_manager_ = std::make_unique<crosapi::BrowserManager>(
      g_browser_process->platform_part()->cros_component_manager());

  if (immediate_login) {
    const std::string cryptohome_id =
        parsed_command_line().GetSwitchValueASCII(switches::kLoginUser);
    const AccountId account_id(
        cryptohome::Identification::FromString(cryptohome_id).GetAccountId());

    user_manager::UserManager* user_manager = user_manager::UserManager::Get();

    if (policy::IsDeviceLocalAccountUser(account_id.GetUserEmail(), nullptr) &&
        !user_manager->IsKnownUser(account_id)) {
      // When a device-local account is removed, its policy is deleted from disk
      // immediately. If a session using this account happens to be in progress,
      // the session is allowed to continue with policy served from an in-memory
      // cache. If Chrome crashes later in the session, the policy becomes
      // completely unavailable. Exit the session in that case, rather than
      // allowing it to continue without policy.
      chrome::AttemptUserExit();
      return;
    }

    // In case of multi-profiles --login-profile will contain user_id_hash.
    std::string user_id_hash =
        parsed_command_line().GetSwitchValueASCII(switches::kLoginProfile);
    session_manager::SessionManager::Get()->CreateSessionForRestart(
        account_id, user_id_hash);

    // If restarting demo session, mark demo session as started before primary
    // profile starts initialization so browser context keyed services created
    // with the browser context (for example ExtensionService) can use
    // DemoSession::started().
    DemoSession::StartIfInDemoMode();

    VLOG(1) << "Relaunching browser for user: " << account_id.Serialize()
            << " with hash: " << user_id_hash;
  }
}

class GuestLanguageSetCallbackData {
 public:
  explicit GuestLanguageSetCallbackData(Profile* profile) : profile(profile) {}

  // Must match SwitchLanguageCallback type.
  static void Callback(
      const std::unique_ptr<GuestLanguageSetCallbackData>& self,
      const locale_util::LanguageSwitchResult& result);

  Profile* profile;
};

// static
void GuestLanguageSetCallbackData::Callback(
    const std::unique_ptr<GuestLanguageSetCallbackData>& self,
    const locale_util::LanguageSwitchResult& result) {
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  scoped_refptr<input_method::InputMethodManager::State> ime_state =
      manager->GetActiveIMEState();
  // For guest mode, we should always use the first login input methods.
  // This is to keep consistency with UserSessionManager::SetFirstLoginPrefs().
  // See crbug.com/530808.
  std::vector<std::string> input_methods;
  manager->GetInputMethodUtil()->GetFirstLoginInputMethodIds(
      result.loaded_locale, ime_state->GetCurrentInputMethod(), &input_methods);
  ime_state->ReplaceEnabledInputMethods(input_methods);

  // Active layout must be hardware "login layout".
  // The previous one must be "locale default layout".
  // First, enable all hardware input methods.
  input_methods = manager->GetInputMethodUtil()->GetHardwareInputMethodIds();
  for (size_t i = 0; i < input_methods.size(); ++i)
    ime_state->EnableInputMethod(input_methods[i]);

  // Second, enable locale based input methods.
  const std::string locale_default_input_method =
      manager->GetInputMethodUtil()->GetLanguageDefaultInputMethodId(
          result.loaded_locale);
  if (!locale_default_input_method.empty()) {
    PrefService* user_prefs = self->profile->GetPrefs();
    user_prefs->SetString(prefs::kLanguagePreviousInputMethod,
                          locale_default_input_method);
    ime_state->EnableInputMethod(locale_default_input_method);
  }

  // Finally, activate the first login input method.
  const std::vector<std::string>& login_input_methods =
      manager->GetInputMethodUtil()->GetHardwareLoginInputMethodIds();
  ime_state->ChangeInputMethod(login_input_methods[0],
                               false /* show_message */);
}

void SetGuestLocale(Profile* const profile) {
  std::unique_ptr<GuestLanguageSetCallbackData> data(
      new GuestLanguageSetCallbackData(profile));
  locale_util::SwitchLanguageCallback callback(base::Bind(
      &GuestLanguageSetCallbackData::Callback, base::Passed(std::move(data))));
  const user_manager::User* const user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  UserSessionManager::GetInstance()->RespectLocalePreference(profile, user,
                                                             callback);
}

void ChromeBrowserMainPartsChromeos::PostProfileInit() {
  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- just after CreateProfile().

  if (ProfileHelper::IsSigninProfile(profile())) {
    // Flush signin profile if it is just created (new device or after recovery)
    // to ensure it is correctly persisted.
    if (profile()->IsNewProfile())
      ProfileHelper::Get()->FlushProfile(profile());
  } else {
    // Force loading of signin profile if it was not loaded before. It is
    // possible when we are restoring session or skipping login screen for some
    // other reason.
    ProfileHelper::GetSigninProfile();
  }

  BootTimesRecorder::Get()->OnChromeProcessStart();

  // Initialize the network portal detector for Chrome OS. The network
  // portal detector starts to listen for notifications from
  // NetworkStateHandler and initiates captive portal detection for
  // active networks. Should be called before call to initialize
  // ChromeSessionManager because it depends on NetworkPortalDetector.
  InitializeNetworkPortalDetector();
  {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    bool is_official_build = true;
#else
    bool is_official_build = false;
#endif
    // Enable portal detector if EULA was previously accepted or if
    // this is an unofficial build.
    if (!is_official_build || StartupUtils::IsEulaAccepted())
      network_portal_detector::GetInstance()->Enable(true);
  }

  // Initialize an observer to update NetworkHandler's pref based services.
  network_pref_state_observer_ = std::make_unique<NetworkPrefStateObserver>();

  // Initialize the NetworkHealth aggregator.
  network_health::NetworkHealthService::GetInstance();

  // Create the service connection to CrosHealthd platform service instance.
  auto* cros_healthd = cros_healthd::ServiceConnection::GetInstance();

  // Pass a callback to the CrosHealthd service connection that binds a pending
  // remote to service.
  cros_healthd->SetBindNetworkHealthServiceCallback(base::BindRepeating([] {
    return network_health::NetworkHealthService::GetInstance()
        ->GetHealthRemoteAndBindReceiver();
  }));

  // Pass a callback to the CrosHealthd service connection that binds a pending
  // remote to the interface.
  cros_healthd->SetBindNetworkDiagnosticsRoutinesCallback(
      base::BindRepeating([] {
        return network_health::NetworkHealthService::GetInstance()
            ->GetDiagnosticsRemoteAndBindReceiver();
      }));

  // Initialize input methods.
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  UserSessionManager* session_manager = UserSessionManager::GetInstance();
  DCHECK(manager);
  DCHECK(session_manager);

  manager->SetState(session_manager->GetDefaultIMEState(profile()));

  bool is_running_test = parameters().ui_task != nullptr;
  g_browser_process->platform_part()->session_manager()->Initialize(
      parsed_command_line(), profile(), is_running_test);

  // Guest user profile is never initialized with locale settings,
  // so we need special handling for Guest session.
  if (user_manager::UserManager::Get()->IsLoggedInAsGuest())
    SetGuestLocale(profile());

  renderer_freezer_ = std::make_unique<RendererFreezer>(
      std::make_unique<FreezerCgroupProcessManager>());

  power_metrics_reporter_ = std::make_unique<PowerMetricsReporter>(
      PowerManagerClient::Get(), g_browser_process->local_state());

  g_browser_process->platform_part()->InitializeAutomaticRebootManager();
  user_removal_manager::RemoveUsersIfNeeded();

  // This observer cannot be created earlier because it requires the shell to be
  // available.
  idle_action_warning_observer_ = std::make_unique<IdleActionWarningObserver>();

  // Start watching for low disk space events to notify the user if it is not a
  // guest profile.
  if (!user_manager::UserManager::Get()->IsLoggedInAsGuest())
    low_disk_notification_ = std::make_unique<LowDiskNotification>();

  gnubby_notification_ = std::make_unique<GnubbyNotification>();
  demo_mode_resources_remover_ = DemoModeResourcesRemover::CreateIfNeeded(
      g_browser_process->local_state());
  // Start measuring crosvm processes resource usage.
  crosvm_metrics_ = std::make_unique<crostini::CrosvmMetrics>();
  crosvm_metrics_->Start();

  login_screen_extensions_lifetime_manager_ =
      std::make_unique<LoginScreenExtensionsLifetimeManager>();
  login_screen_extensions_storage_cleaner_ =
      std::make_unique<LoginScreenExtensionsStorageCleaner>();

  ChromeBrowserMainPartsLinux::PostProfileInit();
}

void ChromeBrowserMainPartsChromeos::PreBrowserStart() {
  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- just before MetricsService::LogNeedForCleanShutdown().

  // Start the external metrics service, which collects metrics from Chrome OS
  // and passes them to the browser process.
  external_metrics_ = new ExternalMetrics;
  external_metrics_->Start();

  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- immediately after ChildProcess::WaitForDebugger().

  if (ui::ShouldDefaultToNaturalScroll()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kNaturalScrollDefault);
    system::InputDeviceSettings::Get()->SetTapToClick(true);
  }

  ChromeBrowserMainPartsLinux::PreBrowserStart();
}

void ChromeBrowserMainPartsChromeos::PostBrowserStart() {
  // Construct a delegate to connect the accessibility component extensions and
  // AccessibilityEventRewriter.
  accessibility_event_rewriter_delegate_ =
      std::make_unique<AccessibilityEventRewriterDelegate>();

  event_rewriter_delegate_ = std::make_unique<EventRewriterDelegateImpl>(
      ash::Shell::Get()->activation_client());

  // Set up the EventRewriterController after ash itself has finished
  // initialization.
  auto* event_rewriter_controller = ash::EventRewriterController::Get();
  event_rewriter_controller->Initialize(
      event_rewriter_delegate_.get(),
      accessibility_event_rewriter_delegate_.get());

  // Enable the KeyboardDrivenEventRewriter if the OEM manifest flag is on.
  if (system::InputDeviceSettings::Get()->ForceKeyboardDrivenUINavigation())
    event_rewriter_controller->SetKeyboardDrivenEventRewriterEnabled(true);

  // In classic ash must occur after ash::Shell is initialized. Triggers a
  // fetch of the initial CrosSettings DeviceRebootOnShutdown policy.
  shutdown_policy_forwarder_ = std::make_unique<ShutdownPolicyForwarder>();

  smart_charging_manager_ = power::SmartChargingManager::CreateInstance();

  if (base::FeatureList::IsEnabled(
          ::features::kAdaptiveScreenBrightnessLogging)) {
    adaptive_screen_brightness_manager_ =
        power::ml::AdaptiveScreenBrightnessManager::CreateInstance();
  }

  if (base::FeatureList::IsEnabled(::features::kUserActivityEventLogging)) {
    // MachineLearningDecisionServiceProvider needs to be created after
    // UserActivityController which depends on UserActivityDetector, not
    // available until PostBrowserStart.
    dbus_services_->CreateMachineLearningDecisionProvider();
  }

  auto_screen_brightness_controller_ =
      std::make_unique<power::auto_screen_brightness::Controller>();

  // Enable Chrome OS USB detection.
  cros_usb_detector_ = std::make_unique<CrosUsbDetector>();
  cros_usb_detector_->ConnectToDeviceManager();

  crostini_unsupported_action_notifier_ =
      std::make_unique<crostini::CrostiniUnsupportedActionNotifier>();

  mojo::PendingRemote<device::mojom::WakeLockProvider> wake_lock_provider;
  content::GetDeviceService().BindWakeLockProvider(
      wake_lock_provider.InitWithNewPipeAndPassReceiver());
  dark_resume_controller_ = std::make_unique<system::DarkResumeController>(
      std::move(wake_lock_provider));

  ChromeBrowserMainPartsLinux::PostBrowserStart();
}

// Shut down services before the browser process, etc are destroyed.
// NOTE: This may get called without PreProfileInit() (or other
// PreMainMessageLoopRun sub-stages) getting called, so be careful with
// shutdown calls and test |pre_profile_init_called_| if necessary. See
// crbug.com/702403 for details.
void ChromeBrowserMainPartsChromeos::PostMainMessageLoopRun() {
  crostini_unsupported_action_notifier_.reset();

  BootTimesRecorder::Get()->AddLogoutTimeMarker("UIMessageLoopEnded", true);

  browser_manager_.reset();

  if (lock_screen_apps_state_controller_)
    lock_screen_apps_state_controller_->Shutdown();

  // This must be shut down before |arc_service_launcher_|.
  if (pre_profile_init_called_)
    NoteTakingHelper::Shutdown();

  arc_data_snapshotd_manager_.reset();

  arc_service_launcher_->Shutdown();

  // Assistant has to shut down before voice interaction controller client to
  // correctly remove the observer.
  assistant_client_.reset();

  assistant_state_client_.reset();

  // Unregister CrosSettings observers before CrosSettings is destroyed.
  shutdown_policy_forwarder_.reset();

  // Destroy the application name notifier for Kiosk mode.
  if (pre_profile_init_called_)
    KioskModeIdleAppNameNotification::Shutdown();

  // Tell DeviceSettingsService to stop talking to session_manager. Do not
  // shutdown DeviceSettingsService yet, it might still be accessed by
  // BrowserPolicyConnector (owned by g_browser_process).
  DeviceSettingsService::Get()->UnsetSessionManager();

  // Destroy the CrosUsb detector so it stops trying to reconnect to the
  // UsbDeviceManager
  cros_usb_detector_.reset();

  // We should remove observers attached to D-Bus clients before
  // DBusThreadManager is shut down.
  network_pref_state_observer_.reset();
  power_metrics_reporter_.reset();
  renderer_freezer_.reset();
  fast_transition_observer_.reset();
  network_throttling_observer_.reset();
  if (pre_profile_init_called_)
    ScreenLocker::ShutDownClass();
  low_disk_notification_.reset();
  demo_mode_resources_remover_.reset();
  smart_charging_manager_.reset();
  adaptive_screen_brightness_manager_.reset();
  auto_screen_brightness_controller_.reset();
  dark_resume_controller_.reset();
  lock_to_single_user_manager_.reset();
  wilco_dtc_supportd_manager_.reset();
  gnubby_notification_.reset();
  login_screen_extensions_lifetime_manager_.reset();
  login_screen_extensions_storage_cleaner_.reset();

  // Detach D-Bus clients before DBusThreadManager is shut down.
  idle_action_warning_observer_.reset();

  if (login_screen_extension_ui::UiHandler::Get(false /*can_create*/))
    login_screen_extension_ui::UiHandler::Shutdown();

  if (pre_profile_init_called_) {
    MagnificationManager::Shutdown();
    audio::SoundsManager::Shutdown();
  }
  system::StatisticsProvider::GetInstance()->Shutdown();

  DemoSession::ShutDownIfInitialized();

  // Inform |NetworkCertLoader| that it should not notify observers anymore.
  // TODO(https://crbug.com/894867): Remove this when the root cause of the
  // crash is found.
  if (NetworkCertLoader::IsInitialized())
    NetworkCertLoader::Get()->set_is_shutting_down();

  // Tear down BulkPrintersCalculators while we still have threads.
  bulk_printers_calculator_factory_.reset();

  CHECK(g_browser_process);
  CHECK(g_browser_process->platform_part());

  // Let the UserManager unregister itself as an observer of the CrosSettings
  // singleton before it is destroyed. This also ensures that the UserManager
  // has no URLRequest pending (see http://crbug.com/276659).
  if (g_browser_process->platform_part()->user_manager())
    g_browser_process->platform_part()->user_manager()->Shutdown();

  // Let the DeviceDisablingManager unregister itself as an observer of the
  // CrosSettings singleton before it is destroyed.
  g_browser_process->platform_part()->ShutdownDeviceDisablingManager();

  // Let the AutomaticRebootManager unregister itself as an observer of several
  // subsystems.
  g_browser_process->platform_part()->ShutdownAutomaticRebootManager();

  // Clean up dependency on CrosSettings and stop pending data fetches.
  KioskAppManager::Shutdown();

  // Make sure that there is no pending URLRequests.
  if (pre_profile_init_called_)
    UserSessionManager::GetInstance()->Shutdown();

  // Give BrowserPolicyConnectorChromeOS a chance to unregister any observers
  // on services that are going to be deleted later but before its Shutdown()
  // is called.
  g_browser_process->platform_part()
      ->browser_policy_connector_chromeos()
      ->PreShutdown();

  // Shutdown the virtual keyboard UI before destroying ash::Shell or the
  // primary profile.
  if (chrome_keyboard_controller_client_)
    chrome_keyboard_controller_client_->Shutdown();

  // Must occur before BrowserProcessImpl::StartTearDown() destroys the
  // ProfileManager.
  if (pre_profile_init_called_) {
    Profile* primary_user = ProfileManager::GetPrimaryUserProfile();
    if (primary_user) {
      // See startup_settings_cache::ReadAppLocale() comment for why we do this.
      startup_settings_cache::WriteAppLocale(
          primary_user->GetPrefs()->GetString(
              language::prefs::kApplicationLocale));
    }
  }

  // Cleans up dbus services depending on ash.
  dbus_services_->PreAshShutdown();

  // NOTE: Closes ash and destroys ash::Shell.
  ChromeBrowserMainPartsLinux::PostMainMessageLoopRun();

  // Destroy classes that may have ash observers or dependencies.
  arc_kiosk_app_manager_.reset();
  web_kiosk_app_manager_.reset();
  chrome_keyboard_controller_client_.reset();

  // All ARC related modules should have been shut down by this point, so
  // destroy ARC.
  // Specifically, this should be done after Profile destruction run in
  // ChromeBrowserMainPartsLinux::PostMainMessageLoopRun().
  arc_service_launcher_.reset();
  // |arc_service_launcher_| uses SchedulerConfigurationManager.
  g_browser_process->platform_part()->ShutdownSchedulerConfigurationManager();

  if (pre_profile_init_called_) {
    AccessibilityManager::Shutdown();
    input_method::Shutdown();
  }

  // Stops all in-flight OAuth2 token fetchers before the IO thread stops.
  DeviceOAuth2TokenServiceFactory::Shutdown();

  content::MediaCaptureDevices::GetInstance()->RemoveAllVideoCaptureObservers();

  // Shutdown after PostMainMessageLoopRun() which should destroy all observers.
  CrasAudioHandler::Shutdown();

  quirks::QuirksManager::Shutdown();

  // Called after ChromeBrowserMainPartsLinux::PostMainMessageLoopRun() (which
  // calls chrome::CloseAsh()) because some parts of WebUI depend on
  // NetworkPortalDetector.
  if (pre_profile_init_called_)
    network_portal_detector::Shutdown();

  g_browser_process->platform_part()->ShutdownSessionManager();
  // Ash needs to be closed before UserManager is destroyed.
  g_browser_process->platform_part()->DestroyChromeUserManager();
}

void ChromeBrowserMainPartsChromeos::PostDestroyThreads() {
  // Destroy crosvm_metrics_ after threads are stopped so that no weak_ptr is
  // held by any task.
  crosvm_metrics_.reset();

  network_change_manager_client_.reset();
  session_termination_manager_.reset();

  // The cert database initializer must be shut down before DBus services are
  // destroyed.
  system_token_certdb_initializer_->ShutDown();

  // Destroy DBus services immediately after threads are stopped.
  dbus_services_.reset();

  // This has to be destroyed after DBusServices
  // (ComponentUpdaterServiceProvider).
  g_browser_process->platform_part()->ShutdownCrosComponentManager();

  ShutdownDBus();

  // Reset SystemTokenCertDBInitializer after DBus services because it should
  // outlive NetworkCertLoader.
  system_token_certdb_initializer_.reset();

  ChromeBrowserMainPartsLinux::PostDestroyThreads();

  // Shutdown these services after g_browser_process.
  InstallAttributes::Shutdown();
  DeviceSettingsService::Shutdown();
}

}  //  namespace chromeos
