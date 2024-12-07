// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/crosapi/crosapi_util.h"

#include <sys/mman.h>

#include <optional>
#include <string>

#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/field_trial_service_ash.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/browser/ash/crosapi/native_theme_service_ash.h"
#include "chrome/browser/ash/crosapi/resource_manager_ash.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_broker.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/handlers/device_name_policy_handler.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/browser/web_applications/preinstalled_web_app_utils.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/components/channel/channel_info.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/components/cdm_factory_daemon/mojom/browser_cdm_factory.mojom.h"
#include "chromeos/components/in_session_auth/mojom/in_session_auth.mojom.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "chromeos/components/payments/mojom/payment_app.mojom.h"
#include "chromeos/components/remote_apps/mojom/remote_apps.mojom.h"
#include "chromeos/components/sensors/mojom/cros_sensor_service.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/devicetype.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "chromeos/crosapi/mojom/arc.mojom.h"
#include "chromeos/crosapi/mojom/audio_service.mojom.h"
#include "chromeos/crosapi/mojom/authentication.mojom.h"
#include "chromeos/crosapi/mojom/automation.mojom.h"
#include "chromeos/crosapi/mojom/browser_app_instance_registry.mojom.h"
#include "chromeos/crosapi/mojom/cec_private.mojom.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "chromeos/crosapi/mojom/cert_provisioning.mojom.h"
#include "chromeos/crosapi/mojom/chaps_service.mojom.h"
#include "chromeos/crosapi/mojom/chrome_app_kiosk_service.mojom.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/crosapi/mojom/content_protection.mojom.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "chromeos/crosapi/mojom/debug_interface.mojom.h"
#include "chromeos/crosapi/mojom/desk.mojom.h"
#include "chromeos/crosapi/mojom/desk_profiles.mojom.h"
#include "chromeos/crosapi/mojom/desk_template.mojom.h"
#include "chromeos/crosapi/mojom/device_attributes.mojom.h"
#include "chromeos/crosapi/mojom/device_local_account_extension_service.mojom.h"
#include "chromeos/crosapi/mojom/device_oauth2_token_service.mojom.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "chromeos/crosapi/mojom/digital_goods.mojom.h"
#include "chromeos/crosapi/mojom/dlp.mojom.h"
#include "chromeos/crosapi/mojom/document_scan.mojom.h"
#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "chromeos/crosapi/mojom/drive_integration_service.mojom.h"
#include "chromeos/crosapi/mojom/echo_private.mojom.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom.h"
#include "chromeos/crosapi/mojom/embedded_accessibility_helper.mojom.h"
#include "chromeos/crosapi/mojom/emoji_picker.mojom.h"
#include "chromeos/crosapi/mojom/extension_info_private.mojom.h"
#include "chromeos/crosapi/mojom/eye_dropper.mojom.h"
#include "chromeos/crosapi/mojom/file_change_service_bridge.mojom.h"
#include "chromeos/crosapi/mojom/file_system_access_cloud_identifier.mojom.h"
#include "chromeos/crosapi/mojom/file_system_provider.mojom.h"
#include "chromeos/crosapi/mojom/firewall_hole.mojom.h"
#include "chromeos/crosapi/mojom/force_installed_tracker.mojom.h"
#include "chromeos/crosapi/mojom/full_restore.mojom.h"
#include "chromeos/crosapi/mojom/fullscreen_controller.mojom.h"
#include "chromeos/crosapi/mojom/geolocation.mojom.h"
#include "chromeos/crosapi/mojom/guest_os_sk_forwarder.mojom.h"
#include "chromeos/crosapi/mojom/identity_manager.mojom.h"
#include "chromeos/crosapi/mojom/image_writer.mojom.h"
#include "chromeos/crosapi/mojom/input_methods.mojom.h"
#include "chromeos/crosapi/mojom/kerberos_in_browser.mojom.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "chromeos/crosapi/mojom/kiosk_session_service.mojom.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/crosapi/mojom/login.mojom.h"
#include "chromeos/crosapi/mojom/login_screen_storage.mojom.h"
#include "chromeos/crosapi/mojom/login_state.mojom.h"
#include "chromeos/crosapi/mojom/magic_boost.mojom.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "chromeos/crosapi/mojom/media_app.mojom.h"
#include "chromeos/crosapi/mojom/media_ui.mojom.h"
#include "chromeos/crosapi/mojom/metrics.mojom.h"
#include "chromeos/crosapi/mojom/multi_capture_service.mojom.h"
#include "chromeos/crosapi/mojom/network_change.mojom.h"
#include "chromeos/crosapi/mojom/networking_attributes.mojom.h"
#include "chromeos/crosapi/mojom/networking_private.mojom.h"
#include "chromeos/crosapi/mojom/nonclosable_app_toast_service.mojom.h"
#include "chromeos/crosapi/mojom/one_drive_integration_service.mojom.h"
#include "chromeos/crosapi/mojom/one_drive_notification_service.mojom.h"
#include "chromeos/crosapi/mojom/parent_access.mojom.h"
#include "chromeos/crosapi/mojom/passkeys.mojom.h"
#include "chromeos/crosapi/mojom/policy_service.mojom.h"
#include "chromeos/crosapi/mojom/power.mojom.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/crosapi/mojom/print_preview_cros.mojom.h"
#include "chromeos/crosapi/mojom/printing_metrics.mojom.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "chromeos/crosapi/mojom/remoting.mojom.h"
#include "chromeos/crosapi/mojom/screen_ai_downloader.mojom.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/crosapi/mojom/sharesheet.mojom.h"
#include "chromeos/crosapi/mojom/smart_reader.mojom.h"
#include "chromeos/crosapi/mojom/structured_metrics_service.mojom.h"
#include "chromeos/crosapi/mojom/suggestion_service.mojom.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "chromeos/crosapi/mojom/system_display.mojom.h"
#include "chromeos/crosapi/mojom/task_manager.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_management_service.mojom.h"
#include "chromeos/crosapi/mojom/timezone.mojom.h"
#include "chromeos/crosapi/mojom/trusted_vault.mojom.h"
#include "chromeos/crosapi/mojom/url_handler.mojom.h"
#include "chromeos/crosapi/mojom/video_capture.mojom.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "chromeos/crosapi/mojom/virtual_keyboard.mojom.h"
#include "chromeos/crosapi/mojom/volume_manager.mojom.h"
#include "chromeos/crosapi/mojom/vpn_extension_observer.mojom.h"
#include "chromeos/crosapi/mojom/vpn_service.mojom.h"
#include "chromeos/crosapi/mojom/wallpaper.mojom.h"
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#include "chromeos/crosapi/mojom/web_kiosk_service.mojom.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "chromeos/services/machine_learning/public/cpp/ml_switches.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/version/version_loader.h"
#include "components/account_id/account_id.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/component_cloud_policy_service.h"
#include "components/policy/core/common/values_util.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/ukm_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "components/variations/limited_entropy_mode_gate.h"
#include "components/variations/service/limited_entropy_synthetic_trial.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"
#include "device/bluetooth/floss/floss_features.h"
#include "media/base/media_switches.h"
#include "media/capture/mojom/video_capture.mojom.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#include "printing/buildflags/buildflags.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/switches.h"

using MojoOptionalBool = crosapi::mojom::DeviceSettings::OptionalBool;

namespace crosapi {
namespace browser_util {

namespace {

constexpr std::string_view kAshCapabilities[] = {
    // Capability to support reloading the lacros browser on receiving a
    // notification that the browser component was successfully updated.
    "crbug/1237235",

    // Capability to support shared_storage in prefs.
    "b/231890240",

    // Capability to register observers for extension controlled prefs via the
    // prefs api.
    "crbug/1334985",

    // Capability to pass testing ash extension keeplist data via ash
    // commandline switch.
    "crbug/1409199",

    // Capability to accept a package ID for installation without a parsing bug.
    // Once Ash and Lacros are both past M124, then we can remove this
    // capability.
    "b/304680258",

    // Bug fix to launch tabbed web app windows in new windows when requested.
    // We can remove this capability once Ash and Lacros are both past M122.
    "crbug/1490336",

    // Support feedback dialog ai flow.
    // TODO(crbug.com/40941303): Remove this capability once Ash and Lacros are
    // both
    // past M123.
    "crbug/1501057",

    // Support opening installed apps when requesting installation in
    // AppInstallServiceAsh.
    // TODO(b/326167458): Remove this capability once Ash and Lacros are both
    // past M127.
    "b/326167458",

    // Support showing the Ash-driven App Install Dialog when requesting
    // installation in AppInstallServiceAsh.
    // TODO(b/331715712): Remove this capability once Ash and Lacros are both
    // past M128.
    "b/331715712"

    // Support unknown package types when requesting app installation in
    // AppInstallServiceAsh.
    // TODO(b/339548766): Remove this capability once Ash and Lacros are both
    // past M129.
    "b/339106891"

    // Entries added to this list must record the current milestone + 3 with a
    // TODO for removal.
};

policy::UserCloudPolicyManagerAsh* GetUserCloudPolicyManager(
    const user_manager::User& user) {
  DCHECK(user.HasGaiaAccount());
  Profile* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(&user));
  DCHECK(profile);
  return profile->GetUserCloudPolicyManagerAsh();
}

policy::DeviceLocalAccountPolicyBroker* GetDeviceLocalAccountPolicyBroker(
    const user_manager::User& user) {
  DCHECK(user.IsDeviceLocalAccount());
  policy::DeviceLocalAccountPolicyService* policy_service =
      g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->GetDeviceLocalAccountPolicyService();
  // `policy_service` can be nullptr, e.g. in unit tests.
  return policy_service ? policy_service->GetBrokerForUser(
                              user.GetAccountId().GetUserEmail())
                        : nullptr;
}

struct InterfaceVersionEntry {
  base::Token uuid;
  uint32_t version;
};

template <typename T>
constexpr InterfaceVersionEntry MakeInterfaceVersionEntry() {
  return {T::Uuid_, T::Version_};
}

static_assert(crosapi::mojom::Crosapi::Version_ == 144,
              "If you add a new crosapi, please add it to "
              "kInterfaceVersionEntries below.");

constexpr InterfaceVersionEntry kInterfaceVersionEntries[] = {
    MakeInterfaceVersionEntry<chromeos::cdm::mojom::BrowserCdmFactory>(),
    MakeInterfaceVersionEntry<chromeos::payments::mojom::PaymentAppInstance>(),
    MakeInterfaceVersionEntry<
        chromeos::remote_apps::mojom::RemoteAppsLacrosBridge>(),
    MakeInterfaceVersionEntry<chromeos::sensors::mojom::SensorHalClient>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Arc>(),
    MakeInterfaceVersionEntry<crosapi::mojom::AudioService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::AuthenticationDeprecated>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Automation>(),
    MakeInterfaceVersionEntry<crosapi::mojom::AutomationFactory>(),
    MakeInterfaceVersionEntry<crosapi::mojom::AccountManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::AppServiceProxy>(),
    MakeInterfaceVersionEntry<crosapi::mojom::BrowserAppInstanceRegistry>(),
    MakeInterfaceVersionEntry<crosapi::mojom::CecPrivate>(),
    MakeInterfaceVersionEntry<crosapi::mojom::CertDatabase>(),
    MakeInterfaceVersionEntry<crosapi::mojom::CertProvisioning>(),
    MakeInterfaceVersionEntry<chromeos::cfm::mojom::CfmServiceContext>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ChapsService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ChromeAppKioskService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ClipboardHistory>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ContentProtection>(),
    MakeInterfaceVersionEntry<crosapi::mojom::CrosDisplayConfigController>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Crosapi>(),
    MakeInterfaceVersionEntry<crosapi::mojom::DebugInterfaceRegisterer>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Desk>(),
    MakeInterfaceVersionEntry<crosapi::mojom::DeskProfileObserver>(),
    MakeInterfaceVersionEntry<crosapi::mojom::DeskTemplate>(),
    MakeInterfaceVersionEntry<crosapi::mojom::DeviceAttributes>(),
    MakeInterfaceVersionEntry<
        crosapi::mojom::DeviceLocalAccountExtensionService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::DeviceOAuth2TokenService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::DeviceSettingsService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::DiagnosticsService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::DigitalGoodsFactory>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Dlp>(),
    MakeInterfaceVersionEntry<crosapi::mojom::DocumentScan>(),
    MakeInterfaceVersionEntry<crosapi::mojom::DownloadController>(),
    MakeInterfaceVersionEntry<crosapi::mojom::DriveIntegrationService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::EchoPrivate>(),
    MakeInterfaceVersionEntry<crosapi::mojom::EditorPanelManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::EmojiPicker>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ExtensionInfoPrivate>(),
    MakeInterfaceVersionEntry<crosapi::mojom::EyeDropper>(),
    MakeInterfaceVersionEntry<crosapi::mojom::FieldTrialService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::FileChangeServiceBridge>(),
    MakeInterfaceVersionEntry<
        crosapi::mojom::FileSystemAccessCloudIdentifierProvider>(),
    MakeInterfaceVersionEntry<crosapi::mojom::FileSystemProviderService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::FirewallHoleServiceDeprecated>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ForceInstalledTracker>(),
    MakeInterfaceVersionEntry<crosapi::mojom::FullRestore>(),
    MakeInterfaceVersionEntry<crosapi::mojom::FullscreenController>(),
    MakeInterfaceVersionEntry<crosapi::mojom::GeolocationService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::IdentityManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::IdleService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::InputMethods>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ImageWriter>(),
    MakeInterfaceVersionEntry<chromeos::auth::mojom::InSessionAuth>(),
    MakeInterfaceVersionEntry<crosapi::mojom::KerberosInBrowser>(),
    MakeInterfaceVersionEntry<crosapi::mojom::KeystoreService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::KioskSessionService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::LocalPrinter>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Login>(),
    MakeInterfaceVersionEntry<crosapi::mojom::LoginScreenStorage>(),
    MakeInterfaceVersionEntry<crosapi::mojom::LoginState>(),
    MakeInterfaceVersionEntry<
        chromeos::machine_learning::mojom::MachineLearningService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::MagicBoostController>(),
    MakeInterfaceVersionEntry<crosapi::mojom::MahiBrowserDelegate>(),
    MakeInterfaceVersionEntry<crosapi::mojom::MediaApp>(),
    MakeInterfaceVersionEntry<crosapi::mojom::MediaUI>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Metrics>(),
    MakeInterfaceVersionEntry<crosapi::mojom::MultiCaptureService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::NativeThemeService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::NetworkChange>(),
    MakeInterfaceVersionEntry<crosapi::mojom::NetworkingAttributes>(),
    MakeInterfaceVersionEntry<crosapi::mojom::NetworkingPrivate>(),
    MakeInterfaceVersionEntry<crosapi::mojom::OneDriveIntegrationService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::OneDriveNotificationService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::PasskeyAuthenticator>(),
    MakeInterfaceVersionEntry<crosapi::mojom::PolicyService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Power>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Prefs>(),
    MakeInterfaceVersionEntry<crosapi::mojom::NonclosableAppToastService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::PrintPreviewCrosDelegate>(),
    MakeInterfaceVersionEntry<crosapi::mojom::PrintingMetrics>(),
    MakeInterfaceVersionEntry<crosapi::mojom::PrintingMetricsForProfile>(),
    MakeInterfaceVersionEntry<
        crosapi::mojom::TelemetryDiagnosticRoutinesService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::TelemetryEventService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::TelemetryManagementService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::TelemetryProbeService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Remoting>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ResourceManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ScreenAIDownloader>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ScreenManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::SearchControllerRegistry>(),
    MakeInterfaceVersionEntry<crosapi::mojom::SearchControllerFactory>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Sharesheet>(),
    MakeInterfaceVersionEntry<crosapi::mojom::SmartReaderClient>(),
    MakeInterfaceVersionEntry<crosapi::mojom::StructuredMetricsService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::SuggestionService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::SnapshotCapturer>(),
    MakeInterfaceVersionEntry<crosapi::mojom::SyncService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::SystemDisplayDeprecated>(),
    MakeInterfaceVersionEntry<crosapi::mojom::TaskManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::TimeZoneService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::TrustedVaultBackend>(),
    MakeInterfaceVersionEntry<crosapi::mojom::TrustedVaultBackendService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::UrlHandler>(),
    MakeInterfaceVersionEntry<crosapi::mojom::VideoCaptureDeviceFactory>(),
    MakeInterfaceVersionEntry<crosapi::mojom::VideoConferenceManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::VirtualKeyboard>(),
    MakeInterfaceVersionEntry<crosapi::mojom::VolumeManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::VpnExtensionObserver>(),
    MakeInterfaceVersionEntry<crosapi::mojom::VpnService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Wallpaper>(),
    MakeInterfaceVersionEntry<crosapi::mojom::WebAppService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::WebKioskService>(),
    MakeInterfaceVersionEntry<device::mojom::HidConnection>(),
    MakeInterfaceVersionEntry<device::mojom::HidManager>(),
    MakeInterfaceVersionEntry<
        media::stable::mojom::StableVideoDecoderFactory>(),
    MakeInterfaceVersionEntry<media_session::mojom::MediaControllerManager>(),
    MakeInterfaceVersionEntry<media_session::mojom::AudioFocusManager>(),
    MakeInterfaceVersionEntry<media_session::mojom::AudioFocusManagerDebug>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ParentAccess>(),
    MakeInterfaceVersionEntry<
        crosapi::mojom::EmbeddedAccessibilityHelperClient>(),
    MakeInterfaceVersionEntry<
        crosapi::mojom::EmbeddedAccessibilityHelperClientFactory>(),
    MakeInterfaceVersionEntry<crosapi::mojom::GuestOsSkForwarderFactory>(),
};

constexpr bool HasDuplicatedUuid() {
  // We assume the number of entries are small enough so that simple
  // O(N^2) check works.
  const size_t size = std::size(kInterfaceVersionEntries);
  for (size_t i = 0; i < size; ++i) {
    for (size_t j = i + 1; j < size; ++j) {
      if (kInterfaceVersionEntries[i].uuid ==
          kInterfaceVersionEntries[j].uuid) {
        return true;
      }
    }
  }
  return false;
}

static_assert(!HasDuplicatedUuid(),
              "Each Crosapi Mojom interface should have unique UUID.");

}  // namespace

const base::flat_map<base::Token, uint32_t>& GetInterfaceVersions() {
  static base::NoDestructor<base::flat_map<base::Token, uint32_t>> versions([] {
    base::flat_map<base::Token, uint32_t> versions;
    for (const auto& entry : kInterfaceVersionEntries) {
      versions.emplace(entry.uuid, entry.version);
    }
    return versions;
  }());
  return *versions;
}

bool IsSigninProfileOrBelongsToAffiliatedUser(Profile* profile) {
  if (ash::IsSigninBrowserContext(profile)) {
    return true;
  }

  if (profile->IsOffTheRecord()) {
    return false;
  }

  const user_manager::User* user =
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile);
  if (!user) {
    return false;
  }
  return user->IsAffiliated();
}

// Returns the device policy data needed for Lacros.
mojom::DeviceSettingsPtr GetDeviceSettings() {
  mojom::DeviceSettingsPtr result = mojom::DeviceSettings::New();

  result->attestation_for_content_protection_enabled = MojoOptionalBool::kUnset;
  result->device_restricted_managed_guest_session_enabled =
      MojoOptionalBool::kUnset;
  result->device_extensions_system_log_enabled = MojoOptionalBool::kUnset;
  if (ash::CrosSettings::IsInitialized()) {
    // It's expected that the CrosSettings values are trusted. The only
    // theoretical exception is when device ownership is taken on consumer
    // device. Then there's no settings to be passed to Lacros anyway.
    auto trusted_result =
        ash::CrosSettings::Get()->PrepareTrustedValues(base::DoNothing());
    if (trusted_result == ash::CrosSettingsProvider::TRUSTED) {
      const auto* cros_settings = ash::CrosSettings::Get();
      bool attestation_enabled = false;
      if (cros_settings->GetBoolean(
              ash::kAttestationForContentProtectionEnabled,
              &attestation_enabled)) {
        result->attestation_for_content_protection_enabled =
            attestation_enabled ? MojoOptionalBool::kTrue
                                : MojoOptionalBool::kFalse;
      }

      const base::Value::List* usb_detachable_allow_list;
      if (cros_settings->GetList(ash::kUsbDetachableAllowlist,
                                 &usb_detachable_allow_list)) {
        mojom::UsbDetachableAllowlistPtr allow_list =
            mojom::UsbDetachableAllowlist::New();
        for (const auto& entry : *usb_detachable_allow_list) {
          mojom::UsbDeviceIdPtr usb_device_id = mojom::UsbDeviceId::New();
          std::optional<int> vid =
              entry.GetDict().FindInt(ash::kUsbDetachableAllowlistKeyVid);
          if (vid) {
            usb_device_id->has_vendor_id = true;
            usb_device_id->vendor_id = vid.value();
          }
          std::optional<int> pid =
              entry.GetDict().FindInt(ash::kUsbDetachableAllowlistKeyPid);
          if (pid) {
            usb_device_id->has_product_id = true;
            usb_device_id->product_id = pid.value();
          }
          allow_list->usb_device_ids.push_back(std::move(usb_device_id));
        }
        result->usb_detachable_allow_list = std::move(allow_list);
      }

      bool device_restricted_managed_guest_session_enabled = false;
      if (cros_settings->GetBoolean(
              ash::kDeviceRestrictedManagedGuestSessionEnabled,
              &device_restricted_managed_guest_session_enabled)) {
        result->device_restricted_managed_guest_session_enabled =
            device_restricted_managed_guest_session_enabled
                ? MojoOptionalBool::kTrue
                : MojoOptionalBool::kFalse;
      }

      bool report_device_network_status = true;
      if (cros_settings->GetBoolean(ash::kReportDeviceNetworkStatus,
                                    &report_device_network_status)) {
        result->report_device_network_status = report_device_network_status
                                                   ? MojoOptionalBool::kTrue
                                                   : MojoOptionalBool::kFalse;
      }

      int report_upload_frequency;
      if (cros_settings->GetInteger(ash::kReportUploadFrequency,
                                    &report_upload_frequency)) {
        result->report_upload_frequency =
            crosapi::mojom::NullableInt64::New(report_upload_frequency);
      }

      int report_device_network_telemetry_collection_rate_ms;
      if (cros_settings->GetInteger(
              ash::kReportDeviceNetworkTelemetryCollectionRateMs,
              &report_device_network_telemetry_collection_rate_ms)) {
        result->report_device_network_telemetry_collection_rate_ms =
            crosapi::mojom::NullableInt64::New(
                report_device_network_telemetry_collection_rate_ms);
      }

      std::string device_variations_restrict_parameter;
      if (cros_settings->GetString(ash::kVariationsRestrictParameter,
                                   &device_variations_restrict_parameter)) {
        result->device_variations_restrict_parameter =
            device_variations_restrict_parameter;
      }

      bool device_guest_mode_enabled = false;
      if (cros_settings->GetBoolean(ash::kAccountsPrefAllowGuest,
                                    &device_guest_mode_enabled)) {
        result->device_guest_mode_enabled = device_guest_mode_enabled
                                                ? MojoOptionalBool::kTrue
                                                : MojoOptionalBool::kFalse;
      }

      bool device_extensions_system_log_enabled = false;
      if (cros_settings->GetBoolean(ash::kDeviceExtensionsSystemLogEnabled,
                                    &device_extensions_system_log_enabled)) {
        result->device_extensions_system_log_enabled =
            device_extensions_system_log_enabled ? MojoOptionalBool::kTrue
                                                 : MojoOptionalBool::kFalse;
      }
    } else {
      LOG(WARNING) << "Unexpected crossettings trusted values status: "
                   << trusted_result;
    }
  }

  result->device_system_wide_tracing_enabled = MojoOptionalBool::kUnset;
  auto* local_state = g_browser_process->local_state();
  if (local_state) {
    auto* pref = local_state->FindPreference(
        ash::prefs::kDeviceSystemWideTracingEnabled);
    if (pref && pref->IsManaged()) {
      result->device_system_wide_tracing_enabled =
          pref->GetValue()->GetBool() ? MojoOptionalBool::kTrue
                                      : MojoOptionalBool::kFalse;
    }
  }

  return result;
}

policy::CloudPolicyCore* GetCloudPolicyCoreForUser(
    const user_manager::User& user) {
  switch (user.GetType()) {
    case user_manager::UserType::kRegular:
    case user_manager::UserType::kChild: {
      policy::UserCloudPolicyManagerAsh* manager =
          GetUserCloudPolicyManager(user);
      return manager ? manager->core() : nullptr;
    }
    case user_manager::UserType::kKioskApp:
    case user_manager::UserType::kPublicAccount:
    case user_manager::UserType::kWebKioskApp:
    case user_manager::UserType::kKioskIWA: {
      policy::DeviceLocalAccountPolicyBroker* broker =
          GetDeviceLocalAccountPolicyBroker(user);
      return broker ? broker->core() : nullptr;
    }
    case user_manager::UserType::kGuest:
      return nullptr;
  }
}

policy::ComponentCloudPolicyService* GetComponentCloudPolicyServiceForUser(
    const user_manager::User& user) {
  switch (user.GetType()) {
    case user_manager::UserType::kRegular:
    case user_manager::UserType::kChild: {
      policy::UserCloudPolicyManagerAsh* manager =
          GetUserCloudPolicyManager(user);
      return manager ? manager->component_policy_service() : nullptr;
    }
    case user_manager::UserType::kKioskApp:
    case user_manager::UserType::kPublicAccount:
    case user_manager::UserType::kWebKioskApp:
    case user_manager::UserType::kKioskIWA: {
      policy::DeviceLocalAccountPolicyBroker* broker =
          GetDeviceLocalAccountPolicyBroker(user);
      return broker ? broker->component_policy_service() : nullptr;
    }
    case user_manager::UserType::kGuest:
      return nullptr;
  }
}

base::span<const std::string_view> GetAshCapabilities() {
  return kAshCapabilities;
}

}  // namespace browser_util
}  // namespace crosapi
