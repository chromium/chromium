// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chrome_browser_main_chromeos.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "ash/events/event_rewriter_controller.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/event_rewriter_controller.mojom.h"
#include "ash/shell.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/linux_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/task/task_traits.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/accessibility/spoken_feedback_event_rewriter_delegate.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_mode_idle_app_name_notification.h"
#include "chrome/browser/chromeos/arc/arc_service_launcher.h"
#include "chrome/browser/chromeos/arc/voice_interaction/voice_interaction_controller_client.h"
#include "chrome/browser/chromeos/boot_times_recorder.h"
#include "chrome/browser/chromeos/dbus/chrome_features_service_provider.h"
#include "chrome/browser/chromeos/dbus/component_updater_service_provider.h"
#include "chrome/browser/chromeos/dbus/dbus_helper.h"
#include "chrome/browser/chromeos/dbus/drive_file_stream_service_provider.h"
#include "chrome/browser/chromeos/dbus/kiosk_info_service_provider.h"
#include "chrome/browser/chromeos/dbus/metrics_event_service_provider.h"
#include "chrome/browser/chromeos/dbus/proxy_resolution_service_provider.h"
#include "chrome/browser/chromeos/dbus/screen_lock_service_provider.h"
#include "chrome/browser/chromeos/dbus/virtual_file_request_service_provider.h"
#include "chrome/browser/chromeos/dbus/vm_applications_service_provider.h"
#include "chrome/browser/chromeos/display/quirks_manager_delegate_impl.h"
#include "chrome/browser/chromeos/events/event_rewriter_delegate_impl.h"
#include "chrome/browser/chromeos/extensions/default_app_order.h"
#include "chrome/browser/chromeos/external_metrics.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/lock_screen_apps/state_controller.h"
#include "chrome/browser/chromeos/logging.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_mode_resources_remover.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/session/chrome_session_manager.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/net/network_portal_detector_impl.h"
#include "chrome/browser/chromeos/net/network_pref_state_observer.h"
#include "chrome/browser/chromeos/net/network_throttling_observer.h"
#include "chrome/browser/chromeos/net/wake_on_wifi_manager.h"
#include "chrome/browser/chromeos/note_taking_helper.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/power/freezer_cgroup_process_manager.h"
#include "chrome/browser/chromeos/power/idle_action_warning_observer.h"
#include "chrome/browser/chromeos/power/ml/adaptive_screen_brightness_manager.h"
#include "chrome/browser/chromeos/power/ml/user_activity_controller.h"
#include "chrome/browser/chromeos/power/power_data_collector.h"
#include "chrome/browser/chromeos/power/power_metrics_reporter.h"
#include "chrome/browser/chromeos/power/process_data_collector.h"
#include "chrome/browser/chromeos/power/renderer_freezer.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/resource_reporter/resource_reporter.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/shutdown_policy_forwarder.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/chromeos/system/user_removal_manager.h"
#include "chrome/browser/chromeos/ui/low_disk_notification.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/ui/ash/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_manager.h"
#include "chrome/browser/upgrade_detector/upgrade_detector_chromeos.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chromeos/accelerometer/accelerometer_reader.h"
#include "chromeos/audio/audio_devices_pref_handler_impl.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/components/drivefs/fake_drivefs_launcher_client.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "chromeos/login/login_state.h"
#include "chromeos/login_event_recorder.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/network_change_notifier_chromeos.h"
#include "chromeos/network/network_change_notifier_factory_chromeos.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/portal_detector/network_portal_detector_stub.h"
#include "chromeos/settings/install_attributes.h"
#include "chromeos/system/statistics_provider.h"
#include "chromeos/tpm/tpm_token_loader.h"
#include "components/account_id/account_id.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/device_event_log/device_event_log.h"
#include "components/metrics/metrics_service.h"
#include "components/ownership/owner_key_util.h"
#include "components/prefs/pref_service.h"
#include "components/quirks/quirks_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_capture_devices.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/service_manager_connection.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_nss_types.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "media/audio/sounds/sounds_manager.h"
#include "net/base/network_change_notifier.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "printing/backend/print_backend.h"
#include "rlz/buildflags/buildflags.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "ui/base/touch/touch_device.h"
#include "ui/base/ui_base_features.h"
#include "ui/chromeos/events/event_rewriter_chromeos.h"
#include "ui/chromeos/events/pref_names.h"
#include "ui/events/event_utils.h"
#include "ui/keyboard/keyboard_resource_util.h"

#if BUILDFLAG(ENABLE_RLZ)
#include "components/rlz/rlz_tracker.h"
#endif

#if BUILDFLAG(ENABLE_CROS_ASSISTANT)
#include "chrome/browser/ui/ash/assistant/assistant_client.h"
#endif

namespace chromeos {

namespace {

void ChromeOSVersionCallback(const std::string& version) {
  base::SetLinuxDistro(std::string("CrOS ") + version);
}

bool ShouldAutoLaunchKioskApp(const base::CommandLine& command_line) {
  KioskAppManager* app_manager = KioskAppManager::Get();
  return command_line.HasSwitch(switches::kLoginManager) &&
         !command_line.HasSwitch(switches::kForceLoginManagerInTests) &&
         app_manager->IsAutoLaunchEnabled() &&
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
        new NetworkPortalDetectorImpl(
            g_browser_process->system_network_context_manager()
                ->GetURLLoaderFactory()));
  }
}

// Called on UI Thread when the system slot has been retrieved.
void GotSystemSlotOnUIThread(
    base::Callback<void(crypto::ScopedPK11Slot)> callback_ui_thread,
    crypto::ScopedPK11Slot system_slot) {
  callback_ui_thread.Run(std::move(system_slot));
}

// Called on IO Thread when the system slot has been retrieved.
void GotSystemSlotOnIOThread(
    base::Callback<void(crypto::ScopedPK11Slot)> callback_ui_thread,
    crypto::ScopedPK11Slot system_slot) {
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&GotSystemSlotOnUIThread, callback_ui_thread,
                     std::move(system_slot)));
}

// Called on IO Thread, initiates retrieval of system slot. |callback_ui_thread|
// will be executed on the UI thread when the system slot has been retrieved.
void GetSystemSlotOnIOThread(
    base::Callback<void(crypto::ScopedPK11Slot)> callback_ui_thread) {
  auto callback =
      base::BindRepeating(&GotSystemSlotOnIOThread, callback_ui_thread);
  crypto::ScopedPK11Slot system_nss_slot =
      crypto::GetSystemNSSKeySlot(callback);
  if (system_nss_slot) {
    callback.Run(std::move(system_nss_slot));
  }
}

// Decides if on start we shall signal to the platform that it can attempt
// owning the TPM.
// For official Chrome builds, send this signal if EULA has been accepted
// already (i.e. the user has started OOBE) to make sure we are not stuck with
// uninitialized TPM after an interrupted OOBE process.
// For Chromium builds, don't send it here. Instead, rely on this signal being
// sent after each successful login.
bool ShallAttemptTpmOwnership() {
#if defined(GOOGLE_CHROME_BUILD)
  return StartupUtils::IsEulaAccepted();
#else
  return false;
#endif
}

}  // namespace

namespace internal {

// Wrapper class for initializing D-Bus services and shutting them down.
class DBusServices {
 public:
  explicit DBusServices(const content::MainFunctionParams& parameters) {
    bluez::BluezDBusManager::Initialize();

    if (!features::IsMultiProcessMash()) {
      // In Mash, power policy is sent to powerd by ash.
      PowerPolicyController::Initialize(
          DBusThreadManager::Get()->GetPowerManagerClient());
    }

    proxy_resolution_service_ = CrosDBusService::Create(
        kNetworkProxyServiceName, dbus::ObjectPath(kNetworkProxyServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<ProxyResolutionServiceProvider>()));

    kiosk_info_service_ = CrosDBusService::Create(
        kKioskAppServiceName, dbus::ObjectPath(kKioskAppServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<KioskInfoService>()));

    metrics_event_service_ = CrosDBusService::Create(
        kMetricsEventServiceName, dbus::ObjectPath(kMetricsEventServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<MetricsEventServiceProvider>()));

    screen_lock_service_ = CrosDBusService::Create(
        kScreenLockServiceName, dbus::ObjectPath(kScreenLockServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<ScreenLockServiceProvider>()));

    virtual_file_request_service_ = CrosDBusService::Create(
        kVirtualFileRequestServiceName,
        dbus::ObjectPath(kVirtualFileRequestServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<VirtualFileRequestServiceProvider>()));

    component_updater_service_ = CrosDBusService::Create(
        kComponentUpdaterServiceName,
        dbus::ObjectPath(kComponentUpdaterServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<ComponentUpdaterServiceProvider>(
                g_browser_process->platform_part()->cros_component_manager())));

    chrome_features_service_ = CrosDBusService::Create(
        kChromeFeaturesServiceName,
        dbus::ObjectPath(kChromeFeaturesServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<ChromeFeaturesServiceProvider>()));

    vm_applications_service_ = CrosDBusService::Create(
        vm_tools::apps::kVmApplicationsServiceName,
        dbus::ObjectPath(vm_tools::apps::kVmApplicationsServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<VmApplicationsServiceProvider>()));

    drive_file_stream_service_ = CrosDBusService::Create(
        drivefs::kDriveFileStreamServiceName,
        dbus::ObjectPath(drivefs::kDriveFileStreamServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<DriveFileStreamServiceProvider>()));

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

    // Initialize the network change notifier for Chrome OS. The network
    // change notifier starts to monitor changes from the power manager and
    // the network manager.
    NetworkChangeNotifierFactoryChromeos::GetInstance()->Initialize();

    // Likewise, initialize the upgrade detector for Chrome OS. The upgrade
    // detector starts to monitor changes from the update engine.
    UpgradeDetectorChromeos::GetInstance()->Init();

    DeviceSettingsService::Get()->SetSessionManager(
        DBusThreadManager::Get()->GetSessionManagerClient(),
        OwnerSettingsServiceChromeOSFactory::GetInstance()->GetOwnerKeyUtil());
  }

  ~DBusServices() {
    NetworkHandler::Shutdown();
    cryptohome::AsyncMethodCaller::Shutdown();
    disks::DiskMountManager::Shutdown();
    LoginState::Shutdown();
    NetworkCertLoader::Shutdown();
    TPMTokenLoader::Shutdown();
    proxy_resolution_service_.reset();
    kiosk_info_service_.reset();
    metrics_event_service_.reset();
    virtual_file_request_service_.reset();
    component_updater_service_.reset();
    chrome_features_service_.reset();
    vm_applications_service_.reset();
    drive_file_stream_service_.reset();
    ProcessDataCollector::Shutdown();
    PowerDataCollector::Shutdown();
    if (!features::IsMultiProcessMash())
      PowerPolicyController::Shutdown();
    device::BluetoothAdapterFactory::Shutdown();
    bluez::BluezDBusManager::Shutdown();
  }

 private:
  std::unique_ptr<CrosDBusService> proxy_resolution_service_;
  std::unique_ptr<CrosDBusService> kiosk_info_service_;
  std::unique_ptr<CrosDBusService> metrics_event_service_;
  std::unique_ptr<CrosDBusService> screen_lock_service_;
  std::unique_ptr<CrosDBusService> virtual_file_request_service_;
  std::unique_ptr<CrosDBusService> component_updater_service_;
  std::unique_ptr<CrosDBusService> chrome_features_service_;
  std::unique_ptr<CrosDBusService> vm_applications_service_;
  std::unique_ptr<CrosDBusService> drive_file_stream_service_;

  DISALLOW_COPY_AND_ASSIGN(DBusServices);
};

// Initializes a global NSSCertDatabase for the system token and starts
// NetworkCertLoader with that database. Note that this is triggered from
// PreMainMessageLoopRun, which is executed after PostMainMessageLoopStart,
// where NetworkCertLoader is initialized. We can thus assume that
// NetworkCertLoader is initialized.
class SystemTokenCertDBInitializer {
 public:
  SystemTokenCertDBInitializer() : weak_ptr_factory_(this) {}
  ~SystemTokenCertDBInitializer() {}

  // Entry point, called on UI thread.
  void Initialize() {
    // Only start loading the system token once cryptohome is available and only
    // if the TPM is ready (available && owned && not being owned).
    DBusThreadManager::Get()
        ->GetCryptohomeClient()
        ->WaitForServiceToBeAvailable(
            base::Bind(&SystemTokenCertDBInitializer::OnCryptohomeAvailable,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Called once the cryptohome service is available.
  void OnCryptohomeAvailable(bool available) {
    if (!available) {
      LOG(ERROR) << "SystemTokenCertDBInitializer: Failed to wait for "
                    "cryptohome to become available.";
      return;
    }

    VLOG(1) << "SystemTokenCertDBInitializer: Cryptohome available.";
    DBusThreadManager::Get()->GetCryptohomeClient()->TpmIsReady(
        base::Bind(&SystemTokenCertDBInitializer::OnGotTpmIsReady,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // This is a callback for the cryptohome TpmIsReady query. Note that this is
  // not a listener which would be called once TPM becomes ready if it was not
  // ready on startup (e.g. after device enrollment), see crbug.com/725500.
  void OnGotTpmIsReady(base::Optional<bool> tpm_is_ready) {
    if (!tpm_is_ready.has_value() || !tpm_is_ready.value()) {
      VLOG(1) << "SystemTokenCertDBInitializer: TPM is not ready - not loading "
                 "system token.";
      if (ShallAttemptTpmOwnership()) {
        // Signal to cryptohome that it can attempt TPM ownership, if it
        // haven't done that yet. The previous signal from EULA dialogue could
        // have been lost if initialization was interrupted.
        // We don't care about the result, and don't block waiting for it.
        LOG(WARNING) << "Request attempting TPM ownership.";
        DBusThreadManager::Get()->GetCryptohomeClient()->TpmCanAttemptOwnership(
            EmptyVoidDBusMethodCallback());
      }

      return;
    }
    VLOG(1)
        << "SystemTokenCertDBInitializer: TPM is ready, loading system token.";
    TPMTokenLoader::Get()->EnsureStarted();
    base::Callback<void(crypto::ScopedPK11Slot)> callback =
        base::BindRepeating(&SystemTokenCertDBInitializer::InitializeDatabase,
                            weak_ptr_factory_.GetWeakPtr());
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(&GetSystemSlotOnIOThread, callback));
  }

  // Initializes the global system token NSSCertDatabase with |system_slot|.
  // Also starts NetworkCertLoader with the system token database.
  void InitializeDatabase(crypto::ScopedPK11Slot system_slot) {
    // Currently, NSSCertDatabase requires a public slot to be set, so we use
    // the system slot there. We also want GetSystemSlot() to return the system
    // slot. As ScopedPK11Slot is actually a unique_ptr which will be moved into
    // the NSSCertDatabase, we need to create a copy, referencing the same slot
    // (using PK11_ReferenceSlot).
    crypto::ScopedPK11Slot system_slot_copy =
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(system_slot.get()));
    auto database = std::make_unique<net::NSSCertDatabaseChromeOS>(
        std::move(system_slot) /* public_slot */,
        crypto::ScopedPK11Slot() /* private_slot */);
    database->SetSystemSlot(std::move(system_slot_copy));

    // TODO(https://crbug.com/844537): Remove this after we've collected logs
    // that show device-wide certificates disappearing.
    database->LogUserCertificates("SystemTokenInitiallyLoaded");

    system_token_cert_database_ = std::move(database);

    VLOG(1) << "SystemTokenCertDBInitializer: Passing system token NSS "
               "database to NetworkCertLoader.";
    NetworkCertLoader::Get()->SetSystemNSSDB(system_token_cert_database_.get());
  }

  // Global NSSCertDatabase which sees the system token.
  std::unique_ptr<net::NSSCertDatabase> system_token_cert_database_;

  base::WeakPtrFactory<SystemTokenCertDBInitializer> weak_ptr_factory_;
};

}  // namespace internal

// ChromeBrowserMainPartsChromeos ----------------------------------------------

ChromeBrowserMainPartsChromeos::ChromeBrowserMainPartsChromeos(
    const content::MainFunctionParams& parameters,
    ChromeFeatureListCreator* chrome_feature_list_creator)
    : ChromeBrowserMainPartsLinux(parameters,
                                  chrome_feature_list_creator),
      is_dbus_initialized_(chrome_feature_list_creator != nullptr) {}

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

  if (!is_dbus_initialized_)
    PreEarlyInitDBus();

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

  return ChromeBrowserMainPartsLinux::PreEarlyInitialization();
}

void ChromeBrowserMainPartsChromeos::PreMainMessageLoopStart() {
  // Initialize session manager in early stage in case others want to listen
  // to session state change right after browser is started.
  g_browser_process->platform_part()->InitializeSessionManager();

  // Replace the default NetworkChangeNotifierFactory with ChromeOS specific
  // implementation. This must be done before BrowserMainLoop calls
  // net::NetworkChangeNotifier::Create() in MainMessageLoopStart().
  net::NetworkChangeNotifier::SetFactory(
      new NetworkChangeNotifierFactoryChromeos());
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
  memory_kills_monitor_ = memory::MemoryKillsMonitor::Initialize();

  ChromeBrowserMainPartsLinux::PostMainMessageLoopStart();
}

// Threads are initialized between MainMessageLoopStart and MainMessageLoopRun.
// about_flags settings are applied in ChromeBrowserMainParts::PreCreateThreads.
void ChromeBrowserMainPartsChromeos::PreMainMessageLoopRun() {
  // Set the crypto thread after the IO thread has been created/started.
  TPMTokenLoader::Get()->SetCryptoTaskRunner(
      base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::IO}));

  // Initialize NSS database for system token.
  system_token_certdb_initializer_ =
      std::make_unique<internal::SystemTokenCertDBInitializer>();
  system_token_certdb_initializer_->Initialize();

  CrasAudioHandler::Initialize(
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

  wake_on_wifi_manager_.reset(new WakeOnWifiManager());
  network_throttling_observer_.reset(
      new NetworkThrottlingObserver(g_browser_process->local_state()));

  arc_service_launcher_ = std::make_unique<arc::ArcServiceLauncher>();
  arc_voice_interaction_controller_client_ =
      std::make_unique<arc::VoiceInteractionControllerClient>();

#if BUILDFLAG(ENABLE_CROS_ASSISTANT)
  // Assistant has to be initialized before session_controller_client to avoid
  // race of SessionChanged event and assistant_client initialization.
  assistant_client_ = std::make_unique<AssistantClient>();
#endif

  chromeos::ResourceReporter::GetInstance()->StartMonitoring(
      task_manager::TaskManagerInterface::GetTaskManager());

  discover_manager_ = std::make_unique<DiscoverManager>();

  ChromeBrowserMainPartsLinux::PreMainMessageLoopRun();
}

void ChromeBrowserMainPartsChromeos::PreProfileInit() {
  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- immediately before Profile creation().

  // Now that the file thread exists we can record our stats.
  BootTimesRecorder::Get()->RecordChromeMainStats();
  LoginEventRecorder::Get()->SetDelegate(BootTimesRecorder::Get());

  // Trigger prefetching of ownership status.
  DeviceSettingsService::Get()->Load();

  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- just before CreateProfile().

  g_browser_process->platform_part()->InitializeChromeUserManager();

  ScreenLocker::InitClass();

  // This forces the ProfileManager to be created and register for the
  // notification it needs to track the logged in user.
  g_browser_process->profile_manager();

  // AccessibilityManager and SystemKeyEventListener use InputMethodManager.
  input_method::Initialize();

  // keyboard::KeyboardController initializes ChromeKeyboardUI which depends
  // on ChromeKeyboardControllerClient.
  chrome_keyboard_controller_client_ =
      std::make_unique<ChromeKeyboardControllerClient>(
          content::ServiceManagerConnection::GetForProcess()->GetConnector());

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

  media::SoundsManager::Create();

  // |arc_service_launcher_| must be initialized before NoteTakingHelper.
  NoteTakingHelper::Initialize();

  AccessibilityManager::Initialize();

  if (!features::IsMultiProcessMash()) {
    // Initialize magnification manager before ash tray is created. And this
    // must be placed after UserManager::SessionStarted();
    // TODO(crbug.com/821551): Mash support.
    MagnificationManager::Initialize();
  }

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::Bind(&version_loader::GetVersion, version_loader::VERSION_FULL),
      base::Bind(&ChromeOSVersionCallback));

  // Make sure that wallpaper boot transition and other delays in OOBE
  // are disabled for tests and kiosk app launch by default.
  // Individual tests may enable them if they want.
  if (parsed_command_line().HasSwitch(::switches::kTestType) ||
      ShouldAutoLaunchKioskApp(parsed_command_line())) {
    WizardController::SetZeroDelays();
  }

  arc_kiosk_app_manager_.reset(new ArcKioskAppManager());

  // On Chrome OS, Chrome does not exit when all browser windows are closed.
  // UnregisterKeepAlive is called from chrome::HandleAppExitingForPlatform.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kDisableZeroBrowsersOpenForTests)) {
    g_browser_process->platform_part()->RegisterKeepAlive();
  }

  // AccelerometerReader is used by ash and content (via DeviceSensor).
  // TODO(mash): Initialize this for Mash or use owned instances in src/ash and
  // src/device. http://crbug.com/525658.
  chromeos::AccelerometerReader::GetInstance()->Initialize(
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));

  // NOTE: Calls ChromeBrowserMainParts::PreProfileInit() which calls
  // ChromeBrowserMainExtraPartsAsh::PreProfileInit() which initializes
  // ash::Shell.
  ChromeBrowserMainPartsLinux::PreProfileInit();

  // Initialize the keyboard before any session state changes (i.e. before
  // loading the default profile).
  keyboard::InitializeKeyboardResources();

  if (lock_screen_apps::StateController::IsEnabled()) {
    lock_screen_apps_state_controller_ =
        std::make_unique<lock_screen_apps::StateController>();
    lock_screen_apps_state_controller_->Initialize();
  }

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

  if (chromeos::ProfileHelper::IsSigninProfile(profile())) {
    // Flush signin profile if it is just created (new device or after recovery)
    // to ensure it is correctly persisted.
    if (profile()->IsNewProfile())
      ProfileHelper::Get()->FlushProfile(profile());
  } else {
    // Force loading of signin profile if it was not loaded before. It is
    // possible when we are restoring session or skipping login screen for some
    // other reason.
    chromeos::ProfileHelper::GetSigninProfile();
  }

  BootTimesRecorder::Get()->OnChromeProcessStart();

  // Initialize the network portal detector for Chrome OS. The network
  // portal detector starts to listen for notifications from
  // NetworkStateHandler and initiates captive portal detection for
  // active networks. Should be called before call to initialize
  // ChromeSessionManager because it depends on NetworkPortalDetector.
  InitializeNetworkPortalDetector();
  {
#if defined(GOOGLE_CHROME_BUILD)
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
      DBusThreadManager::Get()->GetPowerManagerClient(),
      g_browser_process->local_state());

  g_browser_process->platform_part()->InitializeAutomaticRebootManager();
  g_browser_process->platform_part()->InitializeDeviceDisablingManager();
  user_removal_manager::RemoveUsersIfNeeded();

  // This observer cannot be created earlier because it requires the shell to be
  // available.
  idle_action_warning_observer_ = std::make_unique<IdleActionWarningObserver>();

  // Start watching for low disk space events to notify the user if it is not a
  // guest profile.
  if (!user_manager::UserManager::Get()->IsLoggedInAsGuest())
    low_disk_notification_ = std::make_unique<LowDiskNotification>();

  demo_mode_resources_remover_ = DemoModeResourcesRemover::CreateIfNeeded(
      g_browser_process->local_state());
  // Start measuring crosvm processes resource usage.
  crosvm_metrics_ = std::make_unique<crostini::CrosvmMetrics>();
  crosvm_metrics_->Start();

  ChromeBrowserMainPartsLinux::PostProfileInit();
}

void ChromeBrowserMainPartsChromeos::PreBrowserStart() {
  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- just before MetricsService::LogNeedForCleanShutdown().

  // Start the external metrics service, which collects metrics from Chrome OS
  // and passes them to the browser process.
  external_metrics_ = new chromeos::ExternalMetrics;
  external_metrics_->Start();

  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- immediately after ChildProcess::WaitForDebugger().

  if (ui::ShouldDefaultToNaturalScroll()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        chromeos::switches::kNaturalScrollDefault);
    system::InputDeviceSettings::Get()->SetTapToClick(true);
  }

  ChromeBrowserMainPartsLinux::PreBrowserStart();
}

void ChromeBrowserMainPartsChromeos::PostBrowserStart() {
  // Enable the KeyboardDrivenEventRewriter if the OEM manifest flag is on.
  if (system::InputDeviceSettings::Get()->ForceKeyboardDrivenUINavigation()) {
    content::ServiceManagerConnection* connection =
        content::ServiceManagerConnection::GetForProcess();
    ash::mojom::EventRewriterControllerPtr event_rewriter_controller_ptr;
    connection->GetConnector()->BindInterface(ash::mojom::kServiceName,
                                              &event_rewriter_controller_ptr);
    event_rewriter_controller_ptr->SetKeyboardDrivenEventRewriterEnabled(true);
  }

  // Construct a delegate to connect ChromeVox and SpokenFeedbackEventRewriter.
  spoken_feedback_event_rewriter_delegate_ =
      std::make_unique<SpokenFeedbackEventRewriterDelegate>();

  if (!features::IsMultiProcessMash()) {
    // TODO(mash): Support EventRewriterController; see crbug.com/647781
    ash::EventRewriterController* event_rewriter_controller =
        ash::Shell::Get()->event_rewriter_controller();
    event_rewriter_delegate_ = std::make_unique<EventRewriterDelegateImpl>(
        ash::Shell::Get()->activation_client());
    event_rewriter_controller->AddEventRewriter(
        std::make_unique<ui::EventRewriterChromeOS>(
            event_rewriter_delegate_.get(),
            ash::Shell::Get()->sticky_keys_controller()));
  }

  // In classic ash must occur after ash::Shell is initialized. Triggers a
  // fetch of the initial CrosSettings DeviceRebootOnShutdown policy.
  shutdown_policy_forwarder_ = std::make_unique<ShutdownPolicyForwarder>();

  if (base::FeatureList::IsEnabled(
          features::kAdaptiveScreenBrightnessLogging)) {
    adaptive_screen_brightness_manager_ =
        power::ml::AdaptiveScreenBrightnessManager::CreateInstance();
  }

  if (base::FeatureList::IsEnabled(features::kUserActivityEventLogging)) {
    user_activity_controller_ =
        std::make_unique<power::ml::UserActivityController>();
  }

  ChromeBrowserMainPartsLinux::PostBrowserStart();
}

// Shut down services before the browser process, etc are destroyed.
void ChromeBrowserMainPartsChromeos::PostMainMessageLoopRun() {
  chromeos::ResourceReporter::GetInstance()->StopMonitoring();

  BootTimesRecorder::Get()->AddLogoutTimeMarker("UIMessageLoopEnded", true);

  if (lock_screen_apps_state_controller_)
    lock_screen_apps_state_controller_->Shutdown();

  // This must be shut down before |arc_service_launcher_|.
  NoteTakingHelper::Shutdown();

  arc_service_launcher_->Shutdown();

#if BUILDFLAG(ENABLE_CROS_ASSISTANT)
  // Assistant has to shut down before voice interaction controller client to
  // correctly remove the observer.
  assistant_client_.reset();
#endif

  arc_voice_interaction_controller_client_.reset();

  // Unregister CrosSettings observers before CrosSettings is destroyed.
  shutdown_policy_forwarder_.reset();

  // Destroy the application name notifier for Kiosk mode.
  KioskModeIdleAppNameNotification::Shutdown();

  // Shutdown the upgrade detector for Chrome OS. The upgrade detector
  // stops monitoring changes from the update engine.
  if (UpgradeDetectorChromeos::GetInstance())
    UpgradeDetectorChromeos::GetInstance()->Shutdown();

  // Shutdown the network change notifier for Chrome OS. The network
  // change notifier stops monitoring changes from the power manager and
  // the network manager.
  if (NetworkChangeNotifierFactoryChromeos::GetInstance())
    NetworkChangeNotifierFactoryChromeos::GetInstance()->Shutdown();

  // Tell DeviceSettingsService to stop talking to session_manager. Do not
  // shutdown DeviceSettingsService yet, it might still be accessed by
  // BrowserPolicyConnector (owned by g_browser_process).
  DeviceSettingsService::Get()->UnsetSessionManager();

  // We should remove observers attached to D-Bus clients before
  // DBusThreadManager is shut down.
  network_pref_state_observer_.reset();
  power_metrics_reporter_.reset();
  renderer_freezer_.reset();
  wake_on_wifi_manager_.reset();
  network_throttling_observer_.reset();
  ScreenLocker::ShutDownClass();
  low_disk_notification_.reset();
  demo_mode_resources_remover_.reset();
  user_activity_controller_.reset();
  adaptive_screen_brightness_manager_.reset();

  // Detach D-Bus clients before DBusThreadManager is shut down.
  idle_action_warning_observer_.reset();

  if (!features::IsMultiProcessMash())
    MagnificationManager::Shutdown();

  media::SoundsManager::Shutdown();

  system::StatisticsProvider::GetInstance()->Shutdown();

  chromeos::DemoSession::ShutDownIfInitialized();

  // Inform |NetworkCertLoader| that it should not notify observers anymore.
  // TODO(https://crbug.com/894867): Remove this when the root cause of the
  // crash is found.
  if (NetworkCertLoader::IsInitialized())
    NetworkCertLoader::Get()->set_is_shutting_down();

  // Let the UserManager unregister itself as an observer of the CrosSettings
  // singleton before it is destroyed. This also ensures that the UserManager
  // has no URLRequest pending (see http://crbug.com/276659).
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
  UserSessionManager::GetInstance()->Shutdown();

  // Give BrowserPolicyConnectorChromeOS a chance to unregister any observers
  // on services that are going to be deleted later but before its Shutdown()
  // is called.
  g_browser_process->platform_part()
      ->browser_policy_connector_chromeos()
      ->PreShutdown();

  // NOTE: Closes ash and destroys ash::Shell.
  ChromeBrowserMainPartsLinux::PostMainMessageLoopRun();

  // Destroy classes that may have ash observers or dependencies.
  arc_kiosk_app_manager_.reset();
  chrome_keyboard_controller_client_.reset();

  // All ARC related modules should have been shut down by this point, so
  // destroy ARC.
  // Specifically, this should be done after Profile destruction run in
  // ChromeBrowserMainPartsLinux::PostMainMessageLoopRun().
  arc_service_launcher_.reset();

  // TODO(crbug.com/594887): Mash support.
  if (!features::IsMultiProcessMash())
    AccessibilityManager::Shutdown();

  input_method::Shutdown();

  // Stops all in-flight OAuth2 token fetchers before the IO thread stops.
  DeviceOAuth2TokenServiceFactory::Shutdown();

  content::MediaCaptureDevices::GetInstance()->RemoveAllVideoCaptureObservers();

  // Shutdown after PostMainMessageLoopRun() which should destroy all observers.
  CrasAudioHandler::Shutdown();

  quirks::QuirksManager::Shutdown();

  // Called after
  // ChromeBrowserMainPartsLinux::PostMainMessageLoopRun() to be
  // executed after execution of chrome::CloseAsh(), because some
  // parts of WebUI depends on NetworkPortalDetector.
  network_portal_detector::Shutdown();

  g_browser_process->platform_part()->ShutdownSessionManager();
  // Ash needs to be closed before UserManager is destroyed.
  g_browser_process->platform_part()->DestroyChromeUserManager();
}

void ChromeBrowserMainPartsChromeos::PostDestroyThreads() {
  // Destroy crosvm_metrics_ after threads are stopped so that no weak_ptr is
  // held by any task.
  crosvm_metrics_.reset();

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
