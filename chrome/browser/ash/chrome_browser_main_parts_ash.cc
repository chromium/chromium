// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chrome_browser_main_parts_ash.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/keyboard/ui/resources/keyboard_resource_util.h"
#include "ash/public/ash_interfaces.h"
#include "ash/public/cpp/event_rewriter_controller.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/diagnostics/diagnostics_log_controller.h"
#include "ash/system/pcie_peripheral/pcie_peripheral_notification_controller.h"
#include "ash/system/usb_peripheral/usb_peripheral_notification_controller.h"
#include "ash/webui/camera_app_ui/document_scanner_installer.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/linux_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/branding_buildflags.h"
#include "build/config/chromebox_for_meetings/buildflags.h"  // PLATFORM_CFM
#include "chrome/browser/ash/accessibility/accessibility_event_rewriter_delegate_impl.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/app_mode/app_launch_utils.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_mode_idle_app_name_notification.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/arc/memory_pressure/container_app_killer.h"
#include "chrome/browser/ash/arc/session/arc_service_launcher.h"
#include "chrome/browser/ash/audio/audio_survey_handler.h"
#include "chrome/browser/ash/bluetooth/hats_bluetooth_revamp_trigger_impl.h"
#include "chrome/browser/ash/boot_times_recorder.h"
#include "chrome/browser/ash/camera/camera_general_survey_handler.h"
#include "chrome/browser/ash/crosapi/browser_data_back_migrator.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/lacros_availability_policy_observer.h"
#include "chrome/browser/ash/crosapi/lacros_data_backward_migration_mode_policy_observer.h"
#include "chrome/browser/ash/crostini/crostini_unsupported_action_notifier.h"
#include "chrome/browser/ash/dbus/ash_dbus_helper.h"
#include "chrome/browser/ash/dbus/chrome_features_service_provider.h"
#include "chrome/browser/ash/dbus/component_updater_service_provider.h"
#include "chrome/browser/ash/dbus/cryptohome_key_delegate_service_provider.h"
#include "chrome/browser/ash/dbus/dlp_files_policy_service_provider.h"
#include "chrome/browser/ash/dbus/drive_file_stream_service_provider.h"
#include "chrome/browser/ash/dbus/encrypted_reporting_service_provider.h"
#include "chrome/browser/ash/dbus/fusebox_service_provider.h"
#include "chrome/browser/ash/dbus/kiosk_info_service_provider.h"
#include "chrome/browser/ash/dbus/libvda_service_provider.h"
#include "chrome/browser/ash/dbus/lock_to_single_user_service_provider.h"
#include "chrome/browser/ash/dbus/machine_learning_decision_service_provider.h"
#include "chrome/browser/ash/dbus/metrics_event_service_provider.h"
#include "chrome/browser/ash/dbus/mojo_connection_service_provider.h"
#include "chrome/browser/ash/dbus/printers_service_provider.h"
#include "chrome/browser/ash/dbus/profiler_status_service_provider.h"
#include "chrome/browser/ash/dbus/proxy_resolution_service_provider.h"
#include "chrome/browser/ash/dbus/screen_lock_service_provider.h"
#include "chrome/browser/ash/dbus/smb_fs_service_provider.h"
#include "chrome/browser/ash/dbus/virtual_file_request_service_provider.h"
#include "chrome/browser/ash/dbus/vm/plugin_vm_service_provider.h"
#include "chrome/browser/ash/dbus/vm/vm_applications_service_provider.h"
#include "chrome/browser/ash/dbus/vm/vm_disk_management_service_provider.h"
#include "chrome/browser/ash/dbus/vm/vm_launch_service_provider.h"
#include "chrome/browser/ash/dbus/vm/vm_permission_service_provider.h"
#include "chrome/browser/ash/dbus/vm/vm_sk_forwarding_service_provider.h"
#include "chrome/browser/ash/dbus/vm/vm_wl_service_provider.h"
#include "chrome/browser/ash/device_name/device_name_store.h"
#include "chrome/browser/ash/diagnostics/diagnostics_browser_delegate_impl.h"
#include "chrome/browser/ash/display/quirks_manager_delegate_impl.h"
#include "chrome/browser/ash/events/event_rewriter_delegate_impl.h"
#include "chrome/browser/ash/extensions/default_app_order.h"
#include "chrome/browser/ash/extensions/login_screen_ui/ui_handler.h"
#include "chrome/browser/ash/external_metrics.h"
#include "chrome/browser/ash/input_method/editor_mediator.h"
#include "chrome/browser/ash/input_method/input_method_configuration.h"
#include "chrome/browser/ash/language_preferences.h"
#include "chrome/browser/ash/lock_screen_apps/state_controller.h"
#include "chrome/browser/ash/logging.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_resources_remover.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/login_screen_extensions_storage_cleaner.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/osauth/chrome_auth_parts.h"
#include "chrome/browser/ash/login/session/chrome_session_manager.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/memory_metrics.h"
#include "chrome/browser/ash/mojo_service_manager/connection_helper.h"
#include "chrome/browser/ash/net/apn_migrator.h"
#include "chrome/browser/ash/net/bluetooth_pref_state_observer.h"
#include "chrome/browser/ash/net/network_health/network_health_manager.h"
#include "chrome/browser/ash/net/network_portal_detector_impl.h"
#include "chrome/browser/ash/net/network_pref_state_observer.h"
#include "chrome/browser/ash/net/network_throttling_observer.h"
#include "chrome/browser/ash/net/rollback_network_config/rollback_network_config_service.h"
#include "chrome/browser/ash/net/system_proxy_manager.h"
#include "chrome/browser/ash/net/traffic_counters_handler.h"
#include "chrome/browser/ash/network_change_manager_client.h"
#include "chrome/browser/ash/note_taking_helper.h"
#include "chrome/browser/ash/notifications/debugd_notification_handler.h"
#include "chrome/browser/ash/notifications/gnubby_notification.h"
#include "chrome/browser/ash/notifications/low_disk_notification.h"
#include "chrome/browser/ash/notifications/multi_capture_notifications.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/pcie_peripheral/ash_usb_detector.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager_impl.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/handlers/lock_to_single_user_manager.h"
#include "chrome/browser/ash/power/auto_screen_brightness/controller.h"
#include "chrome/browser/ash/power/freezer_cgroup_process_manager.h"
#include "chrome/browser/ash/power/idle_action_warning_observer.h"
#include "chrome/browser/ash/power/ml/adaptive_screen_brightness_manager.h"
#include "chrome/browser/ash/power/power_data_collector.h"
#include "chrome/browser/ash/power/power_metrics_reporter.h"
#include "chrome/browser/ash/power/renderer_freezer.h"
#include "chrome/browser/ash/power/smart_charging/smart_charging_manager.h"
#include "chrome/browser/ash/printing/bulk_printers_calculator_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/profiles/signin_profile_handler.h"
#include "chrome/browser/ash/quick_pair/quick_pair_browser_delegate_impl.h"
#include "chrome/browser/ash/scheduler_configuration_manager.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/shutdown_policy_forwarder.h"
#include "chrome/browser/ash/shortcut_mapping_pref_service.h"
#include "chrome/browser/ash/startup_settings_cache.h"
#include "chrome/browser/ash/system/breakpad_consent_watcher.h"
#include "chrome/browser/ash/system/input_device_settings.h"
#include "chrome/browser/ash/system/user_removal_manager.h"
#include "chrome/browser/ash/system_token_cert_db_initializer.h"
#include "chrome/browser/ash/usb/cros_usb_detector.h"
#include "chrome/browser/ash/video_conference/video_conference_app_service_client.h"
#include "chrome/browser/ash/video_conference/video_conference_ash_feature_client.h"
#include "chrome/browser/ash/wilco_dtc_supportd/wilco_dtc_supportd_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/video_conference/video_conference_manager_client.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/metrics/chrome_feature_list_creator.h"
#include "chrome/browser/metrics/structured/chrome_structured_metrics_recorder.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/startup_data.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/ui/ash/assistant/assistant_browser_delegate_impl.h"
#include "chrome/browser/ui/ash/assistant/assistant_state_client.h"
#include "chrome/browser/ui/ash/fwupd_download_client_impl.h"
#include "chrome/browser/ui/ash/image_downloader_impl.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/ash/session_controller_client_impl.h"
#include "chrome/browser/ui/webui/ash/emoji/emoji_ui.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/attestation/attestation_features.h"
#include "chromeos/ash/components/audio/audio_devices_pref_handler_impl.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_flusher.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/biod/fake_biod_client.h"
#include "chromeos/ash/components/dbus/constants/cryptohome_key_delegate_constants.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/patchpanel/patchpanel_client.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/drivefs/fake_drivefs_launcher_client.h"
#include "chromeos/ash/components/fwupd/firmware_update_manager.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy_factory.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "chromeos/ash/components/login/hibernate/hibernate_manager.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/ash/components/network/fast_transition_observer.h"
#include "chromeos/ash/components/network/network_cert_loader.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector_stub.h"
#include "chromeos/ash/components/network/system_token_cert_db_storage.h"
#include "chromeos/ash/components/peripheral_notification/peripheral_notification_manager.h"
#include "chromeos/ash/components/power/dark_resume_controller.h"
#include "chromeos/ash/components/report/device_metrics/use_case/use_case.h"
#include "chromeos/ash/components/report/report_controller.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/ash/components/tpm/tpm_token_loader.h"
#include "chromeos/ash/services/cros_healthd/private/cpp/data_collector.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/components/sensors/ash/sensor_hal_dispatcher.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/version/version_loader.h"
#include "components/account_id/account_id.h"
#include "components/device_event_log/device_event_log.h"
#include "components/language/core/browser/pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/ownership/owner_key_util.h"
#include "components/prefs/pref_service.h"
#include "components/quirks/quirks_manager.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync/base/command_line_switches.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/variations/service/variations_service.h"
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
#include "net/base/network_change_notifier_passive.h"
#include "printing/backend/print_backend.h"
#include "rlz/buildflags/buildflags.h"
#include "services/audio/public/cpp/sounds/sounds_manager.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/vm_launch/dbus-constants.h"
#include "third_party/cros_system_api/dbus/vm_wl/dbus-constants.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"
#include "ui/base/pointer/pointer_device.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/ash/pref_names.h"
#include "ui/events/event_utils.h"

#if BUILDFLAG(PLATFORM_CFM)
#include "chrome/browser/ash/chromebox_for_meetings/cfm_chrome_services.h"
#endif

#if BUILDFLAG(ENABLE_RLZ)
#include "components/rlz/rlz_tracker.h"  // nogncheck
#endif

namespace ash {

namespace {

void ChromeOSVersionCallback(const absl::optional<std::string>& version) {
  base::SetLinuxDistro("CrOS " + version.value_or("0.0.0.0"));
}

// Creates an instance of the NetworkPortalDetector implementation or a stub.
void InitializeNetworkPortalDetector() {
  if (network_portal_detector::SetForTesting()) {
    return;
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kTestType)) {
    network_portal_detector::SetNetworkPortalDetector(
        new NetworkPortalDetectorStub());
  } else {
    network_portal_detector::SetNetworkPortalDetector(
        new NetworkPortalDetectorImpl());
  }
  network_portal_detector::GetInstance()->Enable();
}

void ApplySigninProfileModifications(Profile* profile) {
  DCHECK(ProfileHelper::IsSigninProfile(profile));
  auto* prefs = profile->GetPrefs();

  prefs->SetBoolean(::prefs::kSafeBrowsingEnabled, false);
}

#if !defined(USE_REAL_DBUS_CLIENTS)
FakeSessionManagerClient* FakeSessionManagerClient() {
  auto* fake_session_manager_client = FakeSessionManagerClient::Get();
  DCHECK(fake_session_manager_client);
  return fake_session_manager_client;
}

chromeos::FakePowerManagerClient* FakePowerManagerClient() {
  chromeos::FakePowerManagerClient* fake_power_manager_client =
      chromeos::FakePowerManagerClient::Get();
  DCHECK(fake_power_manager_client);
  return fake_power_manager_client;
}

void FakeShutdownSignal() {
  // Receiving SIGTERM would result in `ExitIgnoreUnloadHandlers`.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&chrome::ExitIgnoreUnloadHandlers));
}

void InstallFakeShutdownCalls() {
  FakeSessionManagerClient()->set_stop_session_callback(
      base::BindOnce(&FakeShutdownSignal));
  FakePowerManagerClient()->set_restart_callback(
      base::BindOnce(&FakeShutdownSignal));
}
#endif  // !defined(USE_REAL_DBUS_CLIENTS)

void ShillSetPropertyErrorCallback(std::string_view property_name,
                                   const std::string& error_name,
                                   const std::string& error_message) {
  NET_LOG(ERROR) << "Failed to set shill property " << property_name
                 << ", error:" << error_name << ", message: " << error_message;
}

}  // namespace

namespace internal {

// Wrapper class for initializing D-Bus services and shutting them down.
class DBusServices {
 public:
  explicit DBusServices(
      std::unique_ptr<base::FeatureList::Accessor> feature_list_accessor) {
    chromeos::PowerPolicyController::Initialize(
        chromeos::PowerManagerClient::Get());

    dbus::Bus* system_bus = DBusThreadManager::Get()->IsUsingFakes()
                                ? nullptr
                                : DBusThreadManager::Get()->GetSystemBus();

    // See also PostBrowserStart() where machine_learning_decision_service_ is
    // initialized.

    proxy_resolution_service_ = CrosDBusService::Create(
        system_bus, chromeos::kNetworkProxyServiceName,
        dbus::ObjectPath(chromeos::kNetworkProxyServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<ProxyResolutionServiceProvider>()));

    kiosk_info_service_ = CrosDBusService::Create(
        system_bus, chromeos::kKioskAppServiceName,
        dbus::ObjectPath(chromeos::kKioskAppServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<KioskInfoService>()));

    metrics_event_service_ = CrosDBusService::Create(
        system_bus, chromeos::kMetricsEventServiceName,
        dbus::ObjectPath(chromeos::kMetricsEventServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<MetricsEventServiceProvider>()));

    plugin_vm_service_ = CrosDBusService::Create(
        system_bus, chromeos::kPluginVmServiceName,
        dbus::ObjectPath(chromeos::kPluginVmServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<PluginVmServiceProvider>()));

    screen_lock_service_ = CrosDBusService::Create(
        system_bus, chromeos::kScreenLockServiceName,
        dbus::ObjectPath(chromeos::kScreenLockServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<ScreenLockServiceProvider>()));

    virtual_file_request_service_ = CrosDBusService::Create(
        system_bus, chromeos::kVirtualFileRequestServiceName,
        dbus::ObjectPath(chromeos::kVirtualFileRequestServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<VirtualFileRequestServiceProvider>()));

    component_updater_service_ = CrosDBusService::Create(
        system_bus, chromeos::kComponentUpdaterServiceName,
        dbus::ObjectPath(chromeos::kComponentUpdaterServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<ComponentUpdaterServiceProvider>(
                g_browser_process->platform_part()
                    ->cros_component_manager()
                    .get())));

    chrome_features_service_ = CrosDBusService::Create(
        system_bus, chromeos::kChromeFeaturesServiceName,
        dbus::ObjectPath(chromeos::kChromeFeaturesServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<ChromeFeaturesServiceProvider>(
                std::move(feature_list_accessor))));

    printers_service_ = CrosDBusService::Create(
        system_bus, chromeos::kPrintersServiceName,
        dbus::ObjectPath(chromeos::kPrintersServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<PrintersServiceProvider>()));

    profiler_status_service_ = CrosDBusService::Create(
        system_bus, ProfilerStatusServiceProvider::kServiceName,
        dbus::ObjectPath(ProfilerStatusServiceProvider::kServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<ProfilerStatusServiceProvider>()));

    vm_applications_service_ = CrosDBusService::Create(
        system_bus, vm_tools::apps::kVmApplicationsServiceName,
        dbus::ObjectPath(vm_tools::apps::kVmApplicationsServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<VmApplicationsServiceProvider>()));

    vm_disk_management_service_ = CrosDBusService::Create(
        system_bus, vm_tools::disk_management::kVmDiskManagementServiceName,
        dbus::ObjectPath(
            vm_tools::disk_management::kVmDiskManagementServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<VmDiskManagementServiceProvider>()));

    vm_launch_service_ = CrosDBusService::Create(
        system_bus, vm_tools::launch::kVmLaunchServiceName,
        dbus::ObjectPath(vm_tools::launch::kVmLaunchServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<VmLaunchServiceProvider>()));

    vm_sk_forwarding_service_ = CrosDBusService::Create(
        system_bus, vm_tools::sk_forwarding::kVmSKForwardingServiceName,
        dbus::ObjectPath(vm_tools::sk_forwarding::kVmSKForwardingServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<VmSKForwardingServiceProvider>()));

    vm_permission_service_ = CrosDBusService::Create(
        system_bus, chromeos::kVmPermissionServiceName,
        dbus::ObjectPath(chromeos::kVmPermissionServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<VmPermissionServiceProvider>()));

    vm_wl_service_ = CrosDBusService::Create(
        system_bus, vm_tools::wl::kVmWlServiceName,
        dbus::ObjectPath(vm_tools::wl::kVmWlServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<VmWlServiceProvider>()));

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

    encrypted_reporting_service_ = CrosDBusService::Create(
        system_bus, chromeos::kChromeReportingServiceName,
        dbus::ObjectPath(chromeos::kChromeReportingServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<EncryptedReportingServiceProvider>()));

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

    fusebox_service_ = CrosDBusService::Create(
        system_bus, fusebox::kFuseBoxServiceName,
        dbus::ObjectPath(fusebox::kFuseBoxServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<FuseBoxServiceProvider>()));

    mojo_connection_service_ = CrosDBusService::Create(
        system_bus,
        ::mojo_connection_service::kMojoConnectionServiceServiceName,
        dbus::ObjectPath(
            ::mojo_connection_service::kMojoConnectionServiceServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<MojoConnectionServiceProvider>()));

    dlp_files_policy_service_ = CrosDBusService::Create(
        system_bus, dlp::kDlpFilesPolicyServiceName,
        dbus::ObjectPath(dlp::kDlpFilesPolicyServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<DlpFilesPolicyServiceProvider>()));

    if (arc::IsArcVmEnabled()) {
      libvda_service_ = CrosDBusService::Create(
          system_bus, libvda::kLibvdaServiceName,
          dbus::ObjectPath(libvda::kLibvdaServicePath),
          CrosDBusService::CreateServiceProviderList(
              std::make_unique<LibvdaServiceProvider>()));
    }

    // Initialize PowerDataCollector after DBusThreadManager is initialized.
    PowerDataCollector::Initialize();

    LoginState::Initialize();
    TPMTokenLoader::Initialize();
    NetworkCertLoader::Initialize();

    disks::DiskMountManager::Initialize();

    NetworkHandler::Initialize();

    chromeos::sensors::SensorHalDispatcher::Initialize();
    chromeos::sensors::SensorHalDispatcher::GetInstance()
        ->TryToEstablishMojoChannelByServiceManager();

    DeviceSettingsService::Get()->SetSessionManager(
        SessionManagerClient::Get(),
        OwnerSettingsServiceAshFactory::GetInstance()->GetOwnerKeyUtil());
  }

  void CreateMachineLearningDecisionProvider() {
    dbus::Bus* system_bus = DBusThreadManager::Get()->IsUsingFakes()
                                ? nullptr
                                : DBusThreadManager::Get()->GetSystemBus();
    // TODO(alanlxl): update Ml here to MachineLearning after powerd is
    // uprevved.
    machine_learning_decision_service_ = CrosDBusService::Create(
        system_bus, chromeos::kMlDecisionServiceName,
        dbus::ObjectPath(chromeos::kMlDecisionServicePath),
        CrosDBusService::CreateServiceProviderList(
            std::make_unique<MachineLearningDecisionServiceProvider>()));
  }

  DBusServices(const DBusServices&) = delete;
  DBusServices& operator=(const DBusServices&) = delete;

  ~DBusServices() {
    rollback_network_config::Shutdown();
    chromeos::sensors::SensorHalDispatcher::Shutdown();
    NetworkHandler::Shutdown();
    disks::DiskMountManager::Shutdown();
    LoginState::Shutdown();
    NetworkCertLoader::Shutdown();
    TPMTokenLoader::Shutdown();
    proxy_resolution_service_.reset();
    kiosk_info_service_.reset();
    metrics_event_service_.reset();
    plugin_vm_service_.reset();
    printers_service_.reset();
    profiler_status_service_.reset();
    virtual_file_request_service_.reset();
    component_updater_service_.reset();
    chrome_features_service_.reset();
    vm_applications_service_.reset();
    vm_disk_management_service_.reset();
    vm_launch_service_.reset();
    vm_sk_forwarding_service_.reset();
    vm_permission_service_.reset();
    vm_wl_service_.reset();
    drive_file_stream_service_.reset();
    cryptohome_key_delegate_service_.reset();
    encrypted_reporting_service_.reset();
    lock_to_single_user_service_.reset();
    fusebox_service_.reset();
    mojo_connection_service_.reset();
    PowerDataCollector::Shutdown();
    chromeos::PowerPolicyController::Shutdown();
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
  std::unique_ptr<CrosDBusService> profiler_status_service_;
  std::unique_ptr<CrosDBusService> screen_lock_service_;
  std::unique_ptr<CrosDBusService> virtual_file_request_service_;
  std::unique_ptr<CrosDBusService> component_updater_service_;
  std::unique_ptr<CrosDBusService> chrome_features_service_;
  std::unique_ptr<CrosDBusService> vm_applications_service_;
  std::unique_ptr<CrosDBusService> vm_disk_management_service_;
  std::unique_ptr<CrosDBusService> vm_launch_service_;
  std::unique_ptr<CrosDBusService> vm_sk_forwarding_service_;
  std::unique_ptr<CrosDBusService> vm_permission_service_;
  std::unique_ptr<CrosDBusService> vm_wl_service_;
  std::unique_ptr<CrosDBusService> drive_file_stream_service_;
  std::unique_ptr<CrosDBusService> cryptohome_key_delegate_service_;
  std::unique_ptr<CrosDBusService> encrypted_reporting_service_;
  std::unique_ptr<CrosDBusService> libvda_service_;
  std::unique_ptr<CrosDBusService> machine_learning_decision_service_;
  std::unique_ptr<CrosDBusService> smb_fs_service_;
  std::unique_ptr<CrosDBusService> lock_to_single_user_service_;
  std::unique_ptr<CrosDBusService> fusebox_service_;
  std::unique_ptr<CrosDBusService> mojo_connection_service_;
  std::unique_ptr<CrosDBusService> dlp_files_policy_service_;
};

}  // namespace internal

// ChromeBrowserMainPartsAsh ---------------------------------------------------

ChromeBrowserMainPartsAsh::ChromeBrowserMainPartsAsh(bool is_integration_test,
                                                     StartupData* startup_data)
    : ChromeBrowserMainPartsLinux(is_integration_test, startup_data),
      feature_list_accessor_(
          startup_data->chrome_feature_list_creator()
              ->GetAndClearFeatureListAccessor(
                  base::PassKey<ChromeBrowserMainPartsAsh>())) {}

ChromeBrowserMainPartsAsh::~ChromeBrowserMainPartsAsh() {
  // To be precise, logout (browser shutdown) is not yet done, but the
  // remaining work is negligible, hence we say LogoutDone here.
  BootTimesRecorder::Get()->AddLogoutTimeMarker("LogoutDone", false);
  BootTimesRecorder::Get()->WriteLogoutTimes();
}

// content::BrowserMainParts and ChromeBrowserMainExtraParts overrides ---------

int ChromeBrowserMainPartsAsh::PreEarlyInitialization() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Initialize mojo service manager. Note that this depends on the
  // |mojo_ipc_support_| in |content::BrowserMainLoop| to be created. This
  // should be initialized before sending any mojo invitations. Thus, those dbus
  // service which send mojo invitation should be initialized after.
  mojo_service_manager_closer_ =
      mojo_service_manager::CreateConnectionAndPassCloser();

  if (command_line->HasSwitch(switches::kGuestSession)) {
    // Disable sync and extensions if we're in "browse without sign-in" mode.
    command_line->AppendSwitch(::syncer::kDisableSync);
    command_line->AppendSwitch(::switches::kDisableExtensions);
    browser_defaults::bookmarks_enabled = false;
  }

  // If we're not running on real Chrome OS hardware (or under VM), and are not
  // showing the login manager or attempting a command line login, login with a
  // stub user.
  if (!base::SysInfo::IsRunningOnChromeOS() &&
      !command_line->HasSwitch(switches::kLoginManager) &&
      !command_line->HasSwitch(switches::kLoginUser) &&
      !command_line->HasSwitch(switches::kGuestSession)) {
    command_line->AppendSwitchASCII(
        switches::kLoginUser,
        cryptohome::Identification(user_manager::StubAccountId()).id());
    if (!command_line->HasSwitch(switches::kLoginProfile)) {
      command_line->AppendSwitchASCII(
          switches::kLoginProfile,
          BrowserContextHelper::kTestUserBrowserContextDirName);
    }
    LOG(WARNING)
        << "Running as stub user with profile dir: "
        << command_line->GetSwitchValuePath(switches::kLoginProfile).value();
  }

  // DBus is initialized in ChromeMainDelegate::PostEarlyInitialization().
  CHECK(DBusThreadManager::IsInitialized());

  // Triggers the installation as earlier as possible.
  DocumentScannerInstaller::GetInstance()->TriggerInstall();

  if (auto* fake_biod_client = FakeBiodClient::Get()) {
    // The Fake biod saves the fake records to the chrome::DIR_USER_DATA
    // directory, since we can't retrieve this path from chromeos
    // we have to pass it in this way.
    base::FilePath user_data_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    fake_biod_client->SetFakeUserDataDir(std::move(user_data_dir));
  }

#if !defined(USE_REAL_DBUS_CLIENTS)
  // USE_REAL_DBUS clients may be undefined even if the device is using real
  // dbus clients.
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    if (command_line->HasSwitch(switches::kFakeDriveFsLauncherChrootPath) &&
        command_line->HasSwitch(switches::kFakeDriveFsLauncherSocketPath)) {
      drivefs::FakeDriveFsLauncherClient::Init(
          command_line->GetSwitchValuePath(
              switches::kFakeDriveFsLauncherChrootPath),
          command_line->GetSwitchValuePath(
              switches::kFakeDriveFsLauncherSocketPath));
    }

    base::FilePath user_data_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    FakeUserDataAuthClient::Get()->SetUserDataDir(std::move(user_data_dir));

    // If we're not running on a device, i.e. either in a test or in ash Chrome
    // on linux, fake dbus calls that would result in a shutdown of Chrome by
    // the system.
    InstallFakeShutdownCalls();
  }
#endif  // !defined(USE_REAL_DBUS_CLIENTS)

  return ChromeBrowserMainPartsLinux::PreEarlyInitialization();
}

void ChromeBrowserMainPartsAsh::PreCreateMainMessageLoop() {
  // Initialize session manager in early stage in case others want to listen
  // to session state change right after browser is started.
  g_browser_process->platform_part()->InitializeSessionManager();

  ChromeBrowserMainPartsLinux::PreCreateMainMessageLoop();
}

void ChromeBrowserMainPartsAsh::PostCreateMainMessageLoop() {
  // Used by ChromeOS components to retrieve the system token certificate
  // database.
  SystemTokenCertDbStorage::Initialize();

  // device_event_log must be initialized after the message loop.
  device_event_log::Initialize(0 /* default max entries */);

  // This has to be initialized before DBusServices
  // (ComponentUpdaterServiceProvider).
  g_browser_process->platform_part()->InitializeCrosComponentManager();

  dbus_services_ = std::make_unique<internal::DBusServices>(
      std::move(feature_list_accessor_));

  // Need to be done after LoginState has been initialized in DBusServices().
  ::memory::MemoryKillsMonitor::Initialize();

  ChromeBrowserMainPartsLinux::PostCreateMainMessageLoop();
}

// Threads are initialized between CreateMainMessageLoop and MainMessageLoopRun.
// about_flags settings are applied in ChromeBrowserMainParts::PreCreateThreads.
int ChromeBrowserMainPartsAsh::PreMainMessageLoopRun() {
  network_change_manager_client_ = std::make_unique<NetworkChangeManagerClient>(
      static_cast<net::NetworkChangeNotifierPassive*>(
          content::GetNetworkChangeNotifier()));

  // Set the crypto thread after the IO thread has been created/started.
  TPMTokenLoader::Get()->SetCryptoTaskRunner(
      content::GetIOThreadTaskRunner({}));

  // Initialize NSS database for system token.
  system_token_certdb_initializer_ =
      std::make_unique<SystemTokenCertDBInitializer>();

  system_token_key_permissions_manager_ = platform_keys::
      KeyPermissionsManagerImpl::CreateSystemTokenKeyPermissionsManager();

  mojo::PendingRemote<media_session::mojom::MediaControllerManager>
      media_controller_manager;
  content::GetMediaSessionService().BindMediaControllerManager(
      media_controller_manager.InitWithNewPipeAndPassReceiver());
  CrasAudioHandler::Initialize(
      std::move(media_controller_manager),
      new AudioDevicesPrefHandlerImpl(g_browser_process->local_state()));

  audio_survey_handler_ = std::make_unique<AudioSurveyHandler>();

  camera_general_survey_handler_ =
      std::make_unique<CameraGeneralSurveyHandler>();

  content::MediaCaptureDevices::GetInstance()->AddVideoCaptureObserver(
      CrasAudioHandler::Get());

  quirks::QuirksManager::Initialize(
      base::WrapUnique<quirks::QuirksManager::Delegate>(
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

  fast_transition_observer_ = std::make_unique<FastTransitionObserver>(
      g_browser_process->local_state());
  network_throttling_observer_ = std::make_unique<NetworkThrottlingObserver>(
      g_browser_process->local_state());

  g_browser_process->platform_part()->InitializeSchedulerConfigurationManager();
  arc_service_launcher_ = std::make_unique<arc::ArcServiceLauncher>(
      g_browser_process->platform_part()->scheduler_configuration_manager());

  session_termination_manager_ = std::make_unique<SessionTerminationManager>();

  // This should be in PreProfileInit but it needs to be created before the
  // policy connector is started.
  bulk_printers_calculator_factory_ =
      std::make_unique<BulkPrintersCalculatorFactory>();

  // StatsReportingController is created in
  // ChromeBrowserMainParts::PreCreateThreads, so this must come afterwards.
  auto* stats_controller = StatsReportingController::Get();
  // |stats_controller| can be nullptr if ChromeBrowserMainParts's
  // browser_process_->GetApplicationLocale() returns empty. We're trying to
  // show an error message in that case, so don't just crash. (See
  // ChromeBrowserMainParts::PreCreateThreadsImpl()).
  if (stats_controller != nullptr) {
    breakpad_consent_watcher_ =
        system::BreakpadConsentWatcher::Initialize(stats_controller);
  }

#if BUILDFLAG(PLATFORM_CFM)
  cfm::InitializeCfmServices();
#endif  // BUILDFLAG(PLATFORM_CFM)

  SystemProxyManager::Initialize(g_browser_process->local_state());

  debugd_notification_handler_ =
      std::make_unique<DebugdNotificationHandler>(DebugDaemonClient::Get());

  auth_events_recorder_ =
      base::WrapUnique<AuthEventsRecorder>(new AuthEventsRecorder());

  auth_parts_ = std::make_unique<ChromeAuthParts>();

  return ChromeBrowserMainPartsLinux::PreMainMessageLoopRun();
}

void ChromeBrowserMainPartsAsh::PreProfileInit() {
  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- immediately before Profile creation().

  // PreProfileInit() is not always called if no browser process is started
  // (e.g. during some browser tests). Set a boolean so that we do not try to
  // destroy singletons that are initialized here.
  pre_profile_init_called_ = true;

  // Now that the file thread exists we can record our stats.
  BootTimesRecorder::Get()->RecordChromeMainStats();

  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- just before CreateProfile().

  g_browser_process->platform_part()->InitializeChromeUserManager();

  if (base::FeatureList::IsEnabled(features::kPerUserMetrics)) {
    // Enable per-user metrics support as soon as user_manager is created.
    g_browser_process->metrics_service()->InitPerUserMetrics();
  }

  if (base::FeatureList::IsEnabled(::features::kWilcoDtc)) {
    wilco_dtc_supportd_manager_ = std::make_unique<WilcoDtcSupportdManager>();
  }

  ScreenLocker::InitClass();

  // This forces the ProfileManager to be created and register for the
  // notification it needs to track the logged in user.
  g_browser_process->profile_manager();

  // Must come after User Manager is inited.
  lock_to_single_user_manager_ =
      std::make_unique<policy::LockToSingleUserManager>();

  shortcut_mapping_pref_service_ =
      std::make_unique<ShortcutMappingPrefService>();

  // AccessibilityManager and SystemKeyEventListener use InputMethodManager.
  input_method::Initialize();

  // keyboard::KeyboardController initializes ChromeKeyboardUI which depends
  // on ChromeKeyboardControllerClient.
  chrome_keyboard_controller_client_ = ChromeKeyboardControllerClient::Create();

  // Instantiate ProfileHelper as some following code depending on this
  // behavior.
  // TODO(crbug.com/1325210): Switch to explicit initialization.
  ProfileHelper::Get();
  signin_profile_handler_ = std::make_unique<SigninProfileHandler>();

  // If kLoginUser is passed this indicates that user has already
  // logged in and we should behave accordingly.
  bool immediate_login =
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kLoginUser);
  if (immediate_login) {
    // Redirects Chrome logging to the user data dir.
    RedirectChromeLogging(*base::CommandLine::ForCurrentProcess());

    // Load the default app order synchronously for restarting case.
    app_order_loader_ =
        std::make_unique<chromeos::default_app_order::ExternalLoader>(
            false /* async */);
  }

  if (!app_order_loader_) {
    app_order_loader_ =
        std::make_unique<chromeos::default_app_order::ExternalLoader>(
            true /* async */);
  }

  audio::SoundsManager::Create(content::GetAudioServiceStreamFactoryBinder());

  // |arc_service_launcher_| must be initialized before NoteTakingHelper.
  NoteTakingHelper::Initialize();

  AccessibilityManager::Initialize();

  // Initialize magnification manager before ash tray is created. And this
  // must be placed after UserManager initialization.
  MagnificationManager::Initialize();

  g_browser_process->platform_part()->InitializeAshProxyMonitor();

  // Has to be initialized before |assistant_delegate_|;
  image_downloader_ = std::make_unique<ImageDownloaderImpl>();

  // Requires UserManager.
  assistant_state_client_ = std::make_unique<AssistantStateClient>();

  // Assistant has to be initialized before
  // ChromeBrowserMainExtraPartsAsh::session_controller_client_ to avoid race of
  // SessionChanged event and assistant_client initialization. It must come
  // after AssistantStateClient.
  assistant_delegate_ = std::make_unique<AssistantBrowserDelegateImpl>();

  quick_pair_delegate_ =
      std::make_unique<quick_pair::QuickPairBrowserDelegateImpl>();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&chromeos::version_loader::GetVersion,
                     chromeos::version_loader::VERSION_FULL),
      base::BindOnce(&ChromeOSVersionCallback));

  kiosk_app_manager_ = std::make_unique<KioskAppManager>();
  arc_kiosk_app_manager_ = std::make_unique<ArcKioskAppManager>();
  web_kiosk_app_manager_ = std::make_unique<WebKioskAppManager>();

  if (base::FeatureList::IsEnabled(features::kEnableHostnameSetting)) {
    DeviceNameStore::Initialize(g_browser_process->local_state(),
                                g_browser_process->platform_part()
                                    ->browser_policy_connector_ash()
                                    ->GetDeviceNamePolicyHandler());
  }

  if (base::FeatureList::IsEnabled(features::kEnableLocalSearchService)) {
    // Set |local_state| for LocalSearchServiceProxyFactory.
    local_search_service::LocalSearchServiceProxyFactory::GetInstance()
        ->SetLocalState(g_browser_process->local_state());
  }

  // Make sure that wallpaper boot transition and other delays in OOBE
  // are disabled for tests and kiosk app launch by default.
  // Individual tests may enable them if they want.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kTestType) ||
      ShouldAutoLaunchKioskApp(*base::CommandLine::ForCurrentProcess(),
                               g_browser_process->local_state())) {
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
  // `Shell`.
  ChromeBrowserMainPartsLinux::PreProfileInit();

  arc_service_launcher_->Initialize();

  // Needs to be initialized after `Shell`.
  chrome_keyboard_controller_client_->Init(KeyboardController::Get());

  // Initialize the keyboard before any session state changes (i.e. before
  // loading the default profile).
  keyboard::InitializeKeyboardResources();

  lock_screen_apps_state_controller_ =
      std::make_unique<lock_screen_apps::StateController>();
  lock_screen_apps_state_controller_->Initialize();

  // Always construct BrowserManager, even if the lacros flag is disabled, so
  // it can do cleanup work if needed. Initialized in PreProfileInit because the
  // profile-keyed service AppService can call into it.
  crosapi_manager_ = std::make_unique<crosapi::CrosapiManager>();
  browser_manager_ = std::make_unique<crosapi::BrowserManager>(
      g_browser_process->platform_part()->cros_component_manager());
  browser_manager_->AddObserver(SessionControllerClientImpl::Get());
  lacros_availability_policy_observer_ =
      std::make_unique<crosapi::LacrosAvailabilityPolicyObserver>();
  lacros_data_backward_migration_mode_policy_observer_ = std::make_unique<
      crosapi::LacrosDataBackwardMigrationModePolicyObserver>();

  chromeos::machine_learning::ServiceConnection::GetInstance()->Initialize();

  // Needs to be initialized after crosapi_manager_.
  metrics::structured::ChromeStructuredMetricsRecorder::Get()->Initialize();

  multi_capture_notifications_ = std::make_unique<MultiCaptureNotifications>();

  if (immediate_login) {
    const user_manager::CryptohomeId cryptohome_id(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kLoginUser));
    user_manager::KnownUser known_user(g_browser_process->local_state());
    const AccountId account_id(
        known_user.GetAccountIdByCryptohomeId(cryptohome_id));

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
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kLoginProfile);

    if (BrowserDataMigratorImpl::MaybeForceResumeMoveMigration(
            g_browser_process->local_state(), account_id, user_id_hash,
            crosapi::browser_util::PolicyInitState::kBeforeInit)) {
      LOG(WARNING) << "Restarting chrome to resume move migration.";
      return;
    }

    if (BrowserDataMigratorImpl::MaybeRestartToMigrate(
            account_id, user_id_hash,
            crosapi::browser_util::PolicyInitState::kBeforeInit)) {
      LOG(WARNING) << "Restarting chrome to run profile migration.";
      return;
    }

    if (BrowserDataBackMigrator::MaybeRestartToMigrateBack(
            account_id, user_id_hash,
            crosapi::browser_util::PolicyInitState::kBeforeInit)) {
      LOG(WARNING) << "Restarting chrome to run backward profile migration.";
      return;
    }

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

  raw_ptr<Profile, ExperimentalAsh> profile;
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
  for (size_t i = 0; i < input_methods.size(); ++i) {
    ime_state->EnableInputMethod(input_methods[i]);
  }

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
  locale_util::SwitchLanguageCallback callback(
      base::BindOnce(&GuestLanguageSetCallbackData::Callback, std::move(data)));
  const user_manager::User* const user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  UserSessionManager::GetInstance()->RespectLocalePreference(
      profile, user, std::move(callback));
}

void ChromeBrowserMainPartsAsh::PostProfileInit(Profile* profile,
                                                bool is_initial_profile) {
  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- just after CreateProfile().

  if (is_initial_profile) {
    if (ProfileHelper::IsSigninProfile(profile)) {
      // Flush signin profile if it is just created (new device or after
      // recovery) to ensure it is correctly persisted.
      if (profile->IsNewProfile()) {
        BrowserContextFlusher::Get()->ScheduleFlush(profile);
      }

      ApplySigninProfileModifications(profile);
    } else {
      // Force loading of signin profile if it was not loaded before. It is
      // possible when we are restoring session or skipping login screen for
      // some other reason.
      ProfileHelper::GetSigninProfile();
    }

    ui::SetShowEmojiKeyboardCallback(base::BindRepeating(&EmojiUI::Show));

    BootTimesRecorder::Get()->OnChromeProcessStart();

    // Initialize the network portal detector for Chrome OS. The network
    // portal detector starts to listen for notifications from
    // NetworkStateHandler and initiates captive portal detection for
    // active networks. Should be called before call to initialize
    // ChromeSessionManager because it depends on NetworkPortalDetector.
    InitializeNetworkPortalDetector();

    // Initialize an observer to update NetworkHandler's pref based services.
    network_pref_state_observer_ = std::make_unique<NetworkPrefStateObserver>();

    // Initialize an observer to update CrosBluetoothConfig's pref based
    // services.
    bluetooth_pref_state_observer_ =
        std::make_unique<BluetoothPrefStateObserver>();

    if (base::FeatureList::IsEnabled(
            ::features::kHappinessTrackingSystemBluetoothRevamp)) {
      hats_bluetooth_revamp_trigger_ =
          std::make_unique<ash::HatsBluetoothRevampTriggerImpl>();
    }

    // Initialize the NetworkHealth aggregator.
    network_health::NetworkHealthManager::GetInstance();

    // Create cros_healthd data collector.
    cros_healthd_data_collector_ =
        std::make_unique<cros_healthd::internal::DataCollector>();

    // Create the service connection to CrosHealthd platform service instance.
    cros_healthd::ServiceConnection::GetInstance();

    if (features::IsTrafficCountersEnabled()) {
      // Initialize the TrafficCountersHandler instance.
      traffic_counters_handler_ =
          std::make_unique<traffic_counters::TrafficCountersHandler>();
      traffic_counters_handler_->Start();
    }

    // Initialize input methods.
    input_method::InputMethodManager* manager =
        input_method::InputMethodManager::Get();
    // TODO(crbug/1264581): Remove this object once kDeviceI18nShortcutsEnabled
    // policy is deprecated.
    UserSessionManager* session_manager = UserSessionManager::GetInstance();
    DCHECK(manager);
    DCHECK(session_manager);

    manager->SetState(session_manager->GetDefaultIMEState(profile));

    misconfigured_user_cleaner_ = std::make_unique<MisconfiguredUserCleaner>(
        g_browser_process->local_state(), ash::SessionController::Get());

    misconfigured_user_cleaner_->ScheduleCleanup();

    if (chromeos::features::IsOrcaEnabled()) {
      editor_mediator_ = std::make_unique<input_method::EditorMediator>(
          /*profile=*/profile,
          /*country_code=*/g_browser_process->variations_service()
              ->GetLatestCountry());
    }

    g_browser_process->platform_part()->session_manager()->Initialize(
        *base::CommandLine::ForCurrentProcess(), profile,
        is_integration_test());

    // Guest user profile is never initialized with locale settings,
    // so we need special handling for Guest session.
    if (user_manager::UserManager::Get()->IsLoggedInAsGuest()) {
      SetGuestLocale(profile);
    }

    renderer_freezer_ = std::make_unique<RendererFreezer>(
        std::make_unique<FreezerCgroupProcessManager>());

    power_metrics_reporter_ = std::make_unique<PowerMetricsReporter>(
        chromeos::PowerManagerClient::Get(), g_browser_process->local_state());

    g_browser_process->platform_part()->InitializeAutomaticRebootManager();
    user_removal_manager::RemoveUsersIfNeeded();

    // This observer cannot be created earlier because it requires the shell to
    // be available.
    idle_action_warning_observer_ =
        std::make_unique<IdleActionWarningObserver>();

    if (!user_manager::UserManager::Get()->IsLoggedInAsGuest()) {
      // Start watching for low disk space events to notify the user if it is
      // not a guest profile.
      low_disk_notification_ = std::make_unique<LowDiskNotification>();
    }

    gnubby_notification_ = std::make_unique<GnubbyNotification>();
    demo_mode_resources_remover_ = DemoModeResourcesRemover::CreateIfNeeded(
        g_browser_process->local_state());

    login_screen_extensions_storage_cleaner_ =
        std::make_unique<LoginScreenExtensionsStorageCleaner>();

    ash::ShillManagerClient::Get()->SetProperty(
        shill::kEnableRFC8925Property,
        base::Value(base::FeatureList::IsEnabled(features::kEnableRFC8925)),
        base::DoNothing(),
        base::BindOnce(ShillSetPropertyErrorCallback,
                       shill::kEnableRFC8925Property));

    // Notify patchpanel and shill about QoS feature enabled flag.
    const bool wifi_qos_enabled =
        base::FeatureList::IsEnabled(features::kEnableWifiQos);
    ash::PatchPanelClient::Get()->SetFeatureFlag(
        patchpanel::SetFeatureFlagRequest::WIFI_QOS, wifi_qos_enabled);
    ash::ShillManagerClient::Get()->SetProperty(
        shill::kEnableDHCPQoSProperty, base::Value(wifi_qos_enabled),
        base::DoNothing(),
        base::BindOnce(ShillSetPropertyErrorCallback,
                       shill::kEnableDHCPQoSProperty));
  }

  ChromeBrowserMainPartsLinux::PostProfileInit(profile, is_initial_profile);
}

void ChromeBrowserMainPartsAsh::PreBrowserStart() {
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

void ChromeBrowserMainPartsAsh::PostBrowserStart() {
  StartReportController();

  // Construct a delegate to connect the accessibility component extensions and
  // AccessibilityEventRewriter.
  accessibility_event_rewriter_delegate_ =
      std::make_unique<AccessibilityEventRewriterDelegateImpl>();

  event_rewriter_delegate_ = std::make_unique<EventRewriterDelegateImpl>(
      Shell::Get()->activation_client());

  // Set up the EventRewriterController after ash itself has finished
  // initialization.
  auto* event_rewriter_controller = EventRewriterController::Get();
  event_rewriter_controller->Initialize(
      event_rewriter_delegate_.get(),
      accessibility_event_rewriter_delegate_.get());

  // Enable the KeyboardDrivenEventRewriter if the OEM manifest flag is on.
  if (system::InputDeviceSettings::Get()->ForceKeyboardDrivenUINavigation()) {
    event_rewriter_controller->SetKeyboardDrivenEventRewriterEnabled(true);
  }

  // Add MagnificationManager as a pretarget handler after `Shell` is
  // initialized.
  Shell::Get()->AddPreTargetHandler(MagnificationManager::Get());

  // In classic ash must occur after `Shell` is initialized. Triggers a fetch of
  // the initial CrosSettings DeviceRebootOnShutdown policy.
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
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&CrosUsbDetector::ConnectToDeviceManager,
                                base::Unretained(cros_usb_detector_.get())));

  // USB detection for ash notifications.
  ash_usb_detector_ = std::make_unique<AshUsbDetector>();
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&AshUsbDetector::ConnectToDeviceManager,
                                base::Unretained(ash_usb_detector_.get())));

  fwupd_download_client_ = std::make_unique<FwupdDownloadClientImpl>();

  // The local_state pref may not be available at this stage of Chrome's
  // lifecycle, default to false for now. The actual state will be set in a
  // later initializer.
  PeripheralNotificationManager::Initialize(
      user_manager::UserManager::Get()->IsLoggedInAsGuest(),
      /*initial_state=*/false);
  Shell::Get()
      ->pcie_peripheral_notification_controller()
      ->OnPeripheralNotificationManagerInitialized();
  Shell::Get()
      ->usb_peripheral_notification_controller()
      ->OnPeripheralNotificationManagerInitialized();

  crostini_unsupported_action_notifier_ =
      std::make_unique<crostini::CrostiniUnsupportedActionNotifier>();

  mojo::PendingRemote<device::mojom::WakeLockProvider> wake_lock_provider;
  content::GetDeviceService().BindWakeLockProvider(
      wake_lock_provider.InitWithNewPipeAndPassReceiver());
  dark_resume_controller_ = std::make_unique<system::DarkResumeController>(
      std::move(wake_lock_provider));

  HibernateManager::InitializePlatformSupport();

  // DiagnosticsBrowserDelegate has to be initialized after ProfilerHelper and
  // UserManager. Initializing in PostProfileInit to ensure Profile data is
  // available and shell has been initialized.
  diagnostics::DiagnosticsLogController::Initialize(
      std::make_unique<diagnostics::DiagnosticsBrowserDelegateImpl>());

  // Start background collection of memory pressure data for Chrome OS.
  memory_pressure_detail_ = base::MakeRefCounted<MemoryMetrics>(
      MemoryMetrics::kDefaultPeriodInSeconds);
  memory_pressure_detail_->Start();

  if (memory::ZramWritebackController::IsSupportedAndEnabled()) {
    zram_writeback_controller_ = memory::ZramWritebackController::Create();
    zram_writeback_controller_->Start();
  }

  // ARCVM defers to Android's LMK to kill apps in low memory situations because
  // memory can't be reclaimed directly to ChromeOS.
  if (!arc::IsArcVmEnabled() &&
      base::FeatureList::IsEnabled(arc::kContainerAppKiller)) {
    arc_container_app_killer_ = std::make_unique<arc::ContainerAppKiller>();
  }

  if (features::IsVideoConferenceEnabled()) {
    video_conference_manager_client_ =
        std::make_unique<video_conference::VideoConferenceManagerClientImpl>();
    vc_app_service_client_ =
        std::make_unique<VideoConferenceAppServiceClient>();
    vc_ash_feature_client_ =
        std::make_unique<VideoConferenceAshFeatureClient>();
  }

  apn_migrator_ = std::make_unique<ApnMigrator>(
      NetworkHandler::Get()->managed_cellular_pref_handler(),
      NetworkHandler::Get()->managed_network_configuration_handler(),
      NetworkHandler::Get()->network_state_handler());

  ChromeBrowserMainPartsLinux::PostBrowserStart();
}

// Shut down services before the browser process, etc are destroyed.
// NOTE: This may get called without PreProfileInit() (or other
// PreMainMessageLoopRun sub-stages) getting called, so be careful with
// shutdown calls and test |pre_profile_init_called_| if necessary. See
// crbug.com/702403 for details.
void ChromeBrowserMainPartsAsh::PostMainMessageLoopRun() {
  video_conference_manager_client_.reset();

  // Do this early to keep logging from taking time during shutdown.
  if (memory_pressure_detail_ != nullptr) {
    memory_pressure_detail_->Stop();
  }

  if (zram_writeback_controller_ != nullptr) {
    zram_writeback_controller_->Stop();
  }

  apn_migrator_.reset();
  SystemProxyManager::Shutdown();
  report_controller_.reset();
  crostini_unsupported_action_notifier_.reset();

  BootTimesRecorder::Get()->AddLogoutTimeMarker("UIMessageLoopEnded", true);

  if (base::FeatureList::IsEnabled(features::kEnableHostnameSetting)) {
    DeviceNameStore::Shutdown();
  }

  // This needs to be called before the
  // ChromeBrowserMainPartsLinux::PostMainMessageLoopRun, because the
  // SessionControllerClientImpl is destroyed there.
  browser_manager_->RemoveObserver(SessionControllerClientImpl::Get());

  if (lock_screen_apps_state_controller_) {
    lock_screen_apps_state_controller_->Shutdown();
  }

  // This must be shut down before |arc_service_launcher_|.
  if (pre_profile_init_called_) {
    NoteTakingHelper::Shutdown();
  }

  arc_service_launcher_->Shutdown();

  // Assistant has to shut down before voice interaction controller client to
  // correctly remove the observer.
  assistant_delegate_.reset();

  assistant_state_client_.reset();

  if (pre_profile_init_called_) {
    Shell::Get()->RemovePreTargetHandler(MagnificationManager::Get());
  }

  // Unregister CrosSettings observers before CrosSettings is destroyed.
  shutdown_policy_forwarder_.reset();

  // Destroy the application name notifier for Kiosk mode.
  if (pre_profile_init_called_) {
    KioskModeIdleAppNameNotification::Shutdown();
  }

  auth_parts_.reset();

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
  if (pre_profile_init_called_) {
    ScreenLocker::ShutDownClass();
  }
  low_disk_notification_.reset();
  demo_mode_resources_remover_.reset();
  smart_charging_manager_.reset();
  adaptive_screen_brightness_manager_.reset();
  auto_screen_brightness_controller_.reset();
  dark_resume_controller_.reset();
  lock_to_single_user_manager_.reset();
  wilco_dtc_supportd_manager_.reset();
  gnubby_notification_.reset();
  login_screen_extensions_storage_cleaner_.reset();
  debugd_notification_handler_.reset();
  shortcut_mapping_pref_service_.reset();
  if (features::IsTrafficCountersEnabled()) {
    traffic_counters_handler_.reset();
  }
  bluetooth_pref_state_observer_.reset();
  auth_events_recorder_.reset();

  // Detach D-Bus clients before DBusThreadManager is shut down.
  idle_action_warning_observer_.reset();

  if (chromeos::login_screen_extension_ui::UiHandler::Get(
          false /*can_create*/)) {
    chromeos::login_screen_extension_ui::UiHandler::Shutdown();
  }

  if (pre_profile_init_called_) {
    MagnificationManager::Shutdown();
    audio::SoundsManager::Shutdown();
  }
  system::StatisticsProvider::GetInstance()->Shutdown();

  DemoSession::ShutDownIfInitialized();

  // Inform |NetworkCertLoader| that it should not notify observers anymore.
  // TODO(https://crbug.com/894867): Remove this when the root cause of the
  // crash is found.
  if (NetworkCertLoader::IsInitialized()) {
    NetworkCertLoader::Get()->set_is_shutting_down();
  }

  // Tear down BulkPrintersCalculators while we still have threads.
  bulk_printers_calculator_factory_.reset();

  CHECK(g_browser_process);
  CHECK(g_browser_process->platform_part());

  // Let the UserManager unregister itself as an observer of the CrosSettings
  // singleton before it is destroyed. This also ensures that the UserManager
  // has no URLRequest pending (see http://crbug.com/276659).
  if (g_browser_process->platform_part()->user_manager()) {
    g_browser_process->platform_part()->user_manager()->Shutdown();
  }

  // Let the DeviceDisablingManager unregister itself as an observer of the
  // CrosSettings singleton before it is destroyed.
  g_browser_process->platform_part()->ShutdownDeviceDisablingManager();

  // Let the AutomaticRebootManager unregister itself as an observer of several
  // subsystems.
  g_browser_process->platform_part()->ShutdownAutomaticRebootManager();

  // Clean up dependency on CrosSettings and stop pending data fetches.
  kiosk_app_manager_.reset();

  // Make sure that there is no pending URLRequests.
  if (pre_profile_init_called_) {
    UserSessionManager::GetInstance()->Shutdown();
  }

  // Give BrowserPolicyConnectorAsh a chance to unregister any observers
  // on services that are going to be deleted later but before its Shutdown()
  // is called.
  g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->PreShutdown();

  // Shutdown the virtual keyboard UI before destroying `Shell` or the primary
  // profile.
  if (chrome_keyboard_controller_client_) {
    chrome_keyboard_controller_client_->Shutdown();
  }

  // Must occur before BrowserProcessImpl::StartTearDown() destroys the
  // ProfileManager.
  if (pre_profile_init_called_) {
    auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
    if (primary_user) {
      // During a login restart-to-apply-flags the primary profile may not be
      // loaded yet. See http://crbug.com/1432237
      auto* primary_profile = Profile::FromBrowserContext(
          BrowserContextHelper::Get()->GetBrowserContextByUser(primary_user));
      if (primary_profile) {
        // See startup_settings_cache::ReadAppLocale() comment.
        startup_settings_cache::WriteAppLocale(
            primary_profile->GetPrefs()->GetString(
                language::prefs::kApplicationLocale));
      }
    }
  }

#if BUILDFLAG(PLATFORM_CFM)
  // Cleanly shutdown all Chromebox For Meetings services before DBus and other
  // critical services are destroyed
  cfm::ShutdownCfmServices();
#endif  // BUILDFLAG(PLATFORM_CFM)

  // Cleans up dbus services depending on ash.
  dbus_services_->PreAshShutdown();

  // LacrosAvailabilityPolicyObserver and
  // LacrosDataBackwardMigrationModePolicyObserver have the dependency to
  // ProfileManager, so they need to be destroyed before ProfileManager
  // destruction, which happens inside PostMainMessageLoop below.
  lacros_availability_policy_observer_.reset();
  lacros_data_backward_migration_mode_policy_observer_.reset();

  multi_capture_notifications_.reset();

  // vc_app_service_client_ has to be destructed before PostMainMessageLoopRun.
  vc_app_service_client_.reset();
  vc_ash_feature_client_.reset();

  // Has a dependency on Profile, so it needs to be destroyed before Profile
  // gets destroyed during ProfileManager destruction, which happens inside
  // PostMainMessageLoop below.
  web_kiosk_app_manager_.reset();

  // NOTE: Closes ash and destroys `Shell`.
  ChromeBrowserMainPartsLinux::PostMainMessageLoopRun();

  // BrowserManager and CrosapiManager need to outlive the Profile, which
  // is destroyed inside ChromeBrowserMainPartsLinux::PostMainMessageLoopRun().
  browser_manager_.reset();
  crosapi_manager_.reset();

  // The `AshProxyMonitor` instance needs to outlive the `crosapi_manager_`
  // because crosapi depends on it.
  g_browser_process->platform_part()->ShutdownAshProxyMonitor();

  // Destroy classes that may have ash observers or dependencies.
  arc_kiosk_app_manager_.reset();
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
  if (pre_profile_init_called_) {
    network_portal_detector::Shutdown();
  }

  g_browser_process->platform_part()->ShutdownSessionManager();
  // Ash needs to be closed before UserManager is destroyed.
  g_browser_process->platform_part()->DestroyChromeUserManager();

  // Shutdown mojo service manager. This should be called before the
  // |mojo_ipc_support_| in |content::BrowserMainLoop| being reset. It is reset
  // after calling |PostMainMessageLoopRun()| and before calling
  // |PostDestroyThreads()|.
  mojo_service_manager_closer_.RunAndReset();
}

void ChromeBrowserMainPartsAsh::PostDestroyThreads() {
  network_change_manager_client_.reset();
  session_termination_manager_.reset();

  // The cert database initializer must be shut down before DBus services are
  // destroyed.
  system_token_certdb_initializer_.reset();

  // Destroy DBus services immediately after threads are stopped.
  dbus_services_.reset();

  // This has to be destroyed after DBusServices
  // (ComponentUpdaterServiceProvider).
  g_browser_process->platform_part()->ShutdownCrosComponentManager();

  ShutdownDBus();

  // Destroy the SystemTokenCertDbStorage global instance which should outlive
  // NetworkCertLoader and |system_token_certdb_initializer_|.
  SystemTokenCertDbStorage::Shutdown();

  ChromeBrowserMainPartsLinux::PostDestroyThreads();

  // Shutdown these services after g_browser_process.
  InstallAttributes::Shutdown();
  DeviceSettingsService::Shutdown();
  attestation::AttestationFeatures::Shutdown();
}

void ChromeBrowserMainPartsAsh::StartReportController() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Terminate immediately if feature is turned off.
  if (!base::FeatureList::IsEnabled(features::kDeviceActiveClient)) {
    return;
  }

  CrosSettingsProvider::TrustedStatus status =
      CrosSettings::Get()->PrepareTrustedValues(
          base::BindOnce(&ChromeBrowserMainPartsAsh::StartReportController,
                         weak_ptr_factory_.GetWeakPtr()));

  if (status == CrosSettingsProvider::TEMPORARILY_UNTRUSTED ||
      status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
    // When status is TEMPORARILY_UNTRUSTED, PrepareTrustedValues method takes
    // ownership of the start report controller callback.
    // It will retry later when the TRUSTED status becomes available.
    //
    // When status is PERMANENTLY_UNTRUSTED, client assumes this status is final
    // until browser restarts. Client does not proceed without signature
    // verification, so retry is not attempted. This status may be caused
    // if device is running pre-OOBE, or if the policy proto blob fails the
    // signature check.
    return;
  }

  // Create a repeating callback to check the time delta that elapsed since the
  // oobe completed file was written.
  base::RepeatingCallback<base::TimeDelta()> check_oobe_completed_callback =
      base::BindRepeating(&StartupUtils::GetTimeSinceOobeFlagFileCreation);

  // CrosSettingsProvider::TRUSTED: device policies are loaded and trusted.
  report_controller_ = std::make_unique<report::ReportController>(
      ash::report::device_metrics::ChromeDeviceMetadataParameters{
          chrome::GetChannel() /* chromeos_channel */,
          report::ReportController::GetMarketSegment(
              g_browser_process->platform_part()
                  ->browser_policy_connector_ash()
                  ->GetDeviceMode(),
              g_browser_process->platform_part()
                  ->browser_policy_connector_ash()
                  ->GetEnterpriseMarketSegment()) /* market_segment */,
      },
      g_browser_process->local_state(),
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory(),
      first_run::GetFirstRunSentinelCreationTime(),
      std::move(check_oobe_completed_callback),
      std::make_unique<report::PsmDelegateImpl>());

#endif
}

}  //  namespace ash
