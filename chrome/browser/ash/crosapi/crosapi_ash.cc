// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/crosapi_ash.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/ash_interfaces.h"
#include "base/dcheck_is_on.h"
#include "base/functional/callback.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_instance_registry.h"
#include "chrome/browser/apps/app_service/publishers/standalone_browser_extension_apps.h"
#include "chrome/browser/apps/app_service/publishers/standalone_browser_extension_apps_factory.h"
#include "chrome/browser/apps/app_service/publishers/web_apps_crosapi.h"
#include "chrome/browser/apps/app_service/publishers/web_apps_crosapi_factory.h"
#include "chrome/browser/apps/app_service/subscriber_crosapi.h"
#include "chrome/browser/apps/app_service/subscriber_crosapi_factory.h"
#include "chrome/browser/apps/digital_goods/digital_goods_ash.h"
#include "chrome/browser/ash/crosapi/arc_ash.h"
#include "chrome/browser/ash/crosapi/audio_service_ash.h"
#include "chrome/browser/ash/crosapi/authentication_ash.h"
#include "chrome/browser/ash/crosapi/automation_ash.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_service_host_ash.h"
#include "chrome/browser/ash/crosapi/browser_version_service_ash.h"
#include "chrome/browser/ash/crosapi/cert_database_ash.h"
#include "chrome/browser/ash/crosapi/cert_provisioning_ash.h"
#include "chrome/browser/ash/crosapi/chrome_app_kiosk_service_ash.h"
#include "chrome/browser/ash/crosapi/chrome_app_window_tracker_ash.h"
#include "chrome/browser/ash/crosapi/clipboard_ash.h"
#include "chrome/browser/ash/crosapi/clipboard_history_ash.h"
#include "chrome/browser/ash/crosapi/content_protection_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_dependency_registry.h"
#include "chrome/browser/ash/crosapi/desk_ash.h"
#include "chrome/browser/ash/crosapi/desk_template_ash.h"
#include "chrome/browser/ash/crosapi/device_attributes_ash.h"
#include "chrome/browser/ash/crosapi/device_local_account_extension_service_ash.h"
#include "chrome/browser/ash/crosapi/device_oauth2_token_service_ash.h"
#include "chrome/browser/ash/crosapi/device_settings_ash.h"
#include "chrome/browser/ash/crosapi/dlp_ash.h"
#include "chrome/browser/ash/crosapi/document_scan_ash.h"
#include "chrome/browser/ash/crosapi/download_controller_ash.h"
#include "chrome/browser/ash/crosapi/drive_integration_service_ash.h"
#include "chrome/browser/ash/crosapi/echo_private_ash.h"
#include "chrome/browser/ash/crosapi/emoji_picker_ash.h"
#include "chrome/browser/ash/crosapi/extension_info_private_ash.h"
#include "chrome/browser/ash/crosapi/feedback_ash.h"
#include "chrome/browser/ash/crosapi/field_trial_service_ash.h"
#include "chrome/browser/ash/crosapi/file_manager_ash.h"
#include "chrome/browser/ash/crosapi/file_system_provider_service_ash.h"
#include "chrome/browser/ash/crosapi/firewall_hole_ash.h"
#include "chrome/browser/ash/crosapi/force_installed_tracker_ash.h"
#include "chrome/browser/ash/crosapi/fullscreen_controller_ash.h"
#include "chrome/browser/ash/crosapi/geolocation_service_ash.h"
#include "chrome/browser/ash/crosapi/identity_manager_ash.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/browser/ash/crosapi/image_writer_ash.h"
#include "chrome/browser/ash/crosapi/in_session_auth_ash.h"
#include "chrome/browser/ash/crosapi/keystore_service_ash.h"
#include "chrome/browser/ash/crosapi/kiosk_session_service_ash.h"
#include "chrome/browser/ash/crosapi/local_printer_ash.h"
#include "chrome/browser/ash/crosapi/login_ash.h"
#include "chrome/browser/ash/crosapi/login_screen_storage_ash.h"
#include "chrome/browser/ash/crosapi/login_state_ash.h"
#include "chrome/browser/ash/crosapi/media_ui_ash.h"
#include "chrome/browser/ash/crosapi/message_center_ash.h"
#include "chrome/browser/ash/crosapi/metrics_ash.h"
#include "chrome/browser/ash/crosapi/metrics_reporting_ash.h"
#include "chrome/browser/ash/crosapi/multi_capture_service_ash.h"
#include "chrome/browser/ash/crosapi/native_theme_service_ash.h"
#include "chrome/browser/ash/crosapi/network_change_ash.h"
#include "chrome/browser/ash/crosapi/network_settings_service_ash.h"
#include "chrome/browser/ash/crosapi/networking_attributes_ash.h"
#include "chrome/browser/ash/crosapi/networking_private_ash.h"
#include "chrome/browser/ash/crosapi/parent_access_ash.h"
#include "chrome/browser/ash/crosapi/policy_service_ash.h"
#include "chrome/browser/ash/crosapi/power_ash.h"
#include "chrome/browser/ash/crosapi/prefs_ash.h"
#include "chrome/browser/ash/crosapi/remoting_ash.h"
#include "chrome/browser/ash/crosapi/resource_manager_ash.h"
#include "chrome/browser/ash/crosapi/screen_manager_ash.h"
#include "chrome/browser/ash/crosapi/search_provider_ash.h"
#include "chrome/browser/ash/crosapi/select_file_ash.h"
#include "chrome/browser/ash/crosapi/sharesheet_ash.h"
#include "chrome/browser/ash/crosapi/speech_recognition_ash.h"
#include "chrome/browser/ash/crosapi/structured_metrics_service_ash.h"
#include "chrome/browser/ash/crosapi/task_manager_ash.h"
#include "chrome/browser/ash/crosapi/time_zone_service_ash.h"
#include "chrome/browser/ash/crosapi/url_handler_ash.h"
#include "chrome/browser/ash/crosapi/virtual_keyboard_ash.h"
#include "chrome/browser/ash/crosapi/volume_manager_ash.h"
#include "chrome/browser/ash/crosapi/vpn_extension_observer_ash.h"
#include "chrome/browser/ash/crosapi/vpn_service_ash.h"
#include "chrome/browser/ash/crosapi/wallpaper_ash.h"
#include "chrome/browser/ash/crosapi/web_app_service_ash.h"
#include "chrome/browser/ash/crosapi/web_page_info_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/remote_apps/remote_apps_manager_factory.h"
#include "chrome/browser/ash/sync/sync_mojo_service_ash.h"
#include "chrome/browser/ash/sync/sync_mojo_service_factory_ash.h"
#include "chrome/browser/ash/telemetry_extension/diagnostics_service_ash.h"
#include "chrome/browser/ash/telemetry_extension/probe_service_ash.h"
#include "chrome/browser/ash/telemetry_extension/telemetry_event_service_ash.h"
#include "chrome/browser/ash/video_conference/video_conference_manager_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/tts_ash.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "chromeos/components/cdm_factory_daemon/cdm_factory_daemon_proxy_ash.h"
#include "chromeos/components/sensors/ash/sensor_hal_dispatcher.h"
#include "chromeos/crosapi/mojom/device_local_account_extension_service.mojom.h"
#include "chromeos/crosapi/mojom/drive_integration_service.mojom.h"
#include "chromeos/crosapi/mojom/feedback.mojom.h"
#include "chromeos/crosapi/mojom/file_manager.mojom.h"
#include "chromeos/crosapi/mojom/firewall_hole.mojom.h"
#include "chromeos/crosapi/mojom/image_writer.mojom.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/multi_capture_service.mojom.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/crosapi/mojom/select_file.mojom.h"
#include "chromeos/crosapi/mojom/task_manager.mojom.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/browser/video_capture_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "printing/buildflags/buildflags.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"

#if BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)
#include "content/public/browser/stable_video_decoder_factory.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#endif  // BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)

#if BUILDFLAG(USE_CUPS)
#include "chrome/browser/ash/crosapi/printing_metrics_ash.h"
#endif  // BUILDFLAG(USE_CUPS)

namespace crosapi {
namespace {

// Assumptions:
// 1. TODO(crbug.com/1102768): Multi-Signin / Fast-User-Switching is disabled.
// 2. ash-chrome has 1 and only 1 "regular" `Profile`.
Profile* GetAshProfile() {
#if DCHECK_IS_ON()
  int num_regular_profiles = 0;
  for (const Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    if (ash::ProfileHelper::IsUserProfile(profile))
      ++num_regular_profiles;
  }
  DCHECK_EQ(1, num_regular_profiles);
#endif  // DCHECK_IS_ON()
  return ash::ProfileHelper::Get()->GetProfileByUser(
      user_manager::UserManager::Get()->GetActiveUser());
}

}  // namespace

CrosapiAsh::CrosapiAsh(CrosapiDependencyRegistry* registry)
    : arc_ash_(std::make_unique<ArcAsh>()),
      audio_service_ash_(std::make_unique<AudioServiceAsh>()),
      authentication_ash_(std::make_unique<AuthenticationAsh>()),
      automation_ash_(std::make_unique<AutomationAsh>()),
      browser_service_host_ash_(std::make_unique<BrowserServiceHostAsh>()),
      browser_version_service_ash_(std::make_unique<BrowserVersionServiceAsh>(
          g_browser_process->component_updater())),
      cert_database_ash_(std::make_unique<CertDatabaseAsh>()),
      cert_provisioning_ash_(std::make_unique<CertProvisioningAsh>()),
      chrome_app_kiosk_service_ash_(
          std::make_unique<ChromeAppKioskServiceAsh>()),
      chrome_app_window_tracker_ash_(
          std::make_unique<ChromeAppWindowTrackerAsh>()),
      clipboard_ash_(std::make_unique<ClipboardAsh>()),
      clipboard_history_ash_(std::make_unique<ClipboardHistoryAsh>()),
      content_protection_ash_(std::make_unique<ContentProtectionAsh>()),
      desk_ash_(std::make_unique<DeskAsh>()),
      desk_template_ash_(std::make_unique<DeskTemplateAsh>()),
      device_attributes_ash_(std::make_unique<DeviceAttributesAsh>()),
      device_local_account_extension_service_ash_(
          std::make_unique<DeviceLocalAccountExtensionServiceAsh>()),
      device_oauth2_token_service_ash_(
          std::make_unique<DeviceOAuth2TokenServiceAsh>()),
      device_settings_ash_(std::make_unique<DeviceSettingsAsh>()),
      diagnostics_service_ash_(std::make_unique<ash::DiagnosticsServiceAsh>()),
      digital_goods_factory_ash_(
          std::make_unique<apps::DigitalGoodsFactoryAsh>()),
      dlp_ash_(std::make_unique<DlpAsh>()),
      document_scan_ash_(std::make_unique<DocumentScanAsh>()),
      download_controller_ash_(std::make_unique<DownloadControllerAsh>()),
      drive_integration_service_ash_(
          std::make_unique<DriveIntegrationServiceAsh>()),
      echo_private_ash_(std::make_unique<EchoPrivateAsh>()),
      emoji_picker_ash_(std::make_unique<EmojiPickerAsh>()),
      extension_info_private_ash_(std::make_unique<ExtensionInfoPrivateAsh>()),
      feedback_ash_(std::make_unique<FeedbackAsh>()),
      field_trial_service_ash_(std::make_unique<FieldTrialServiceAsh>()),
      file_manager_ash_(std::make_unique<FileManagerAsh>()),
      file_system_provider_service_ash_(
          std::make_unique<FileSystemProviderServiceAsh>()),
      firewall_hole_service_ash_(std::make_unique<FirewallHoleServiceAsh>()),
      force_installed_tracker_ash_(
          std::make_unique<ForceInstalledTrackerAsh>()),
      fullscreen_controller_ash_(std::make_unique<FullscreenControllerAsh>()),
      geolocation_service_ash_(std::make_unique<GeolocationServiceAsh>()),
      identity_manager_ash_(std::make_unique<IdentityManagerAsh>()),
      idle_service_ash_(std::make_unique<IdleServiceAsh>()),
      image_writer_ash_(std::make_unique<ImageWriterAsh>()),
      in_session_auth_ash_(std::make_unique<InSessionAuthAsh>()),
      keystore_service_ash_(std::make_unique<KeystoreServiceAsh>()),
      kiosk_session_service_ash_(std::make_unique<KioskSessionServiceAsh>()),
      local_printer_ash_(std::make_unique<LocalPrinterAsh>()),
      login_ash_(std::make_unique<LoginAsh>()),
      login_screen_storage_ash_(std::make_unique<LoginScreenStorageAsh>()),
      login_state_ash_(std::make_unique<LoginStateAsh>()),
      media_ui_ash_(std::make_unique<MediaUIAsh>()),
      message_center_ash_(std::make_unique<MessageCenterAsh>()),
      metrics_ash_(std::make_unique<MetricsAsh>()),
      metrics_reporting_ash_(registry->CreateMetricsReportingAsh(
          g_browser_process->metrics_service())),
      multi_capture_service_ash_(std::make_unique<MultiCaptureServiceAsh>()),
      native_theme_service_ash_(std::make_unique<NativeThemeServiceAsh>()),
      network_change_ash_(std::make_unique<NetworkChangeAsh>()),
      networking_attributes_ash_(std::make_unique<NetworkingAttributesAsh>()),
      networking_private_ash_(std::make_unique<NetworkingPrivateAsh>()),
      network_settings_service_ash_(std::make_unique<NetworkSettingsServiceAsh>(
          g_browser_process->local_state())),
      parent_access_ash_(std::make_unique<ParentAccessAsh>()),
      policy_service_ash_(std::make_unique<PolicyServiceAsh>()),
      power_ash_(std::make_unique<PowerAsh>()),
      prefs_ash_(
          std::make_unique<PrefsAsh>(g_browser_process->profile_manager(),
                                     g_browser_process->local_state())),
#if BUILDFLAG(USE_CUPS)
      printing_metrics_ash_(std::make_unique<PrintingMetricsAsh>()),
#endif  // BUILDFLAG(USE_CUPS)
      telemetry_event_service_ash_(
          std::make_unique<ash::TelemetryEventServiceAsh>()),
      probe_service_ash_(std::make_unique<ash::ProbeServiceAsh>()),
      remoting_ash_(std::make_unique<RemotingAsh>()),
      resource_manager_ash_(std::make_unique<ResourceManagerAsh>()),
      screen_manager_ash_(std::make_unique<ScreenManagerAsh>()),
      search_provider_ash_(std::make_unique<SearchProviderAsh>()),
      select_file_ash_(std::make_unique<SelectFileAsh>()),
      sharesheet_ash_(std::make_unique<SharesheetAsh>()),
      smart_reader_manager_ash_(std::make_unique<ash::SmartReaderManagerAsh>()),
      speech_recognition_ash_(std::make_unique<SpeechRecognitionAsh>()),
      structured_metrics_service_ash_(
          std::make_unique<StructuredMetricsServiceAsh>()),
      task_manager_ash_(std::make_unique<TaskManagerAsh>()),
      time_zone_service_ash_(std::make_unique<TimeZoneServiceAsh>()),
      tts_ash_(std::make_unique<TtsAsh>(g_browser_process->profile_manager())),
      url_handler_ash_(std::make_unique<UrlHandlerAsh>()),
      video_conference_manager_ash_(
          std::make_unique<ash::VideoConferenceManagerAsh>()),
      virtual_keyboard_ash_(std::make_unique<VirtualKeyboardAsh>()),
      volume_manager_ash_(std::make_unique<VolumeManagerAsh>()),
      vpn_extension_observer_ash_(std::make_unique<VpnExtensionObserverAsh>()),
      vpn_service_ash_(std::make_unique<VpnServiceAsh>()),
      wallpaper_ash_(std::make_unique<WallpaperAsh>()),
      web_app_service_ash_(std::make_unique<WebAppServiceAsh>()),
      web_page_info_factory_ash_(std::make_unique<WebPageInfoFactoryAsh>()) {
  receiver_set_.set_disconnect_handler(base::BindRepeating(
      &CrosapiAsh::OnDisconnected, weak_factory_.GetWeakPtr()));
}

CrosapiAsh::~CrosapiAsh() {
  // Invoke all disconnect handlers.
  auto handlers = std::move(disconnect_handler_map_);
  for (auto& entry : handlers)
    std::move(entry.second).Run();
}

void CrosapiAsh::BindAccountManager(
    mojo::PendingReceiver<mojom::AccountManager> receiver) {
  // Given `GetAshProfile()` assumptions, there is 1 and only 1
  // `AccountManagerMojoService` that can/should be contacted - the one attached
  // to the regular `Profile` in ash-chrome for the active `User`.
  crosapi::AccountManagerMojoService* const account_manager_mojo_service =
      g_browser_process->platform_part()
          ->GetAccountManagerFactory()
          ->GetAccountManagerMojoService(
              /*profile_path=*/GetAshProfile()->GetPath().value());
  account_manager_mojo_service->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindAppServiceProxy(
    mojo::PendingReceiver<crosapi::mojom::AppServiceProxy> receiver) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  auto* subscriber_crosapi =
      apps::SubscriberCrosapiFactory::GetForProfile(profile);
  subscriber_crosapi->RegisterAppServiceProxyFromCrosapi(std::move(receiver));
}

void CrosapiAsh::BindArc(mojo::PendingReceiver<mojom::Arc> receiver) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  arc_ash_->MaybeSetProfile(profile);
  arc_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindAudioService(
    mojo::PendingReceiver<mojom::AudioService> receiver) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  audio_service_ash_->Initialize(profile);
  audio_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindAuthentication(
    mojo::PendingReceiver<mojom::Authentication> receiver) {
  authentication_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindAutomationDeprecated(
    mojo::PendingReceiver<mojom::Automation> receiver) {}

void CrosapiAsh::BindAutomationFactory(
    mojo::PendingReceiver<mojom::AutomationFactory> receiver) {
  automation_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindBrowserAppInstanceRegistry(
    mojo::PendingReceiver<mojom::BrowserAppInstanceRegistry> receiver) {
  if (!web_app::IsWebAppsCrosapiEnabled()) {
    return;
  }
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  auto* app_service_proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  app_service_proxy->BrowserAppInstanceRegistry()->BindReceiver(
      receiver_set_.current_context(), std::move(receiver));
}

void CrosapiAsh::BindBrowserCdmFactory(mojo::GenericPendingReceiver receiver) {
  if (auto r = receiver.As<chromeos::cdm::mojom::BrowserCdmFactory>())
    chromeos::CdmFactoryDaemonProxyAsh::Create(std::move(r));
}

void CrosapiAsh::BindBrowserServiceHost(
    mojo::PendingReceiver<crosapi::mojom::BrowserServiceHost> receiver) {
  browser_service_host_ash_->BindReceiver(receiver_set_.current_context(),
                                          std::move(receiver));
}

void CrosapiAsh::BindBrowserVersionService(
    mojo::PendingReceiver<crosapi::mojom::BrowserVersionService> receiver) {
  browser_version_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindCertDatabase(
    mojo::PendingReceiver<mojom::CertDatabase> receiver) {
  cert_database_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindCertProvisioning(
    mojo::PendingReceiver<mojom::CertProvisioning> receiver) {
  cert_provisioning_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindChromeAppKioskService(
    mojo::PendingReceiver<mojom::ChromeAppKioskService> receiver) {
  chrome_app_kiosk_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindChromeAppPublisher(
    mojo::PendingReceiver<mojom::AppPublisher> receiver) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  apps::StandaloneBrowserExtensionApps* chrome_apps =
      apps::StandaloneBrowserExtensionAppsFactoryForApp::GetForProfile(profile);
  chrome_apps->RegisterCrosapiHost(std::move(receiver));
}

void CrosapiAsh::BindChromeAppWindowTracker(
    mojo::PendingReceiver<mojom::AppWindowTracker> receiver) {
  chrome_app_window_tracker_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindClipboard(
    mojo::PendingReceiver<mojom::Clipboard> receiver) {
  clipboard_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindClipboardHistory(
    mojo::PendingReceiver<mojom::ClipboardHistory> receiver) {
  clipboard_history_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindContentProtection(
    mojo::PendingReceiver<mojom::ContentProtection> receiver) {
  content_protection_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindCrosDisplayConfigController(
    mojo::PendingReceiver<mojom::CrosDisplayConfigController> receiver) {
  ash::BindCrosDisplayConfigController(std::move(receiver));
}

void CrosapiAsh::BindDesk(mojo::PendingReceiver<mojom::Desk> receiver) {
  desk_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindDeskTemplate(
    mojo::PendingReceiver<mojom::DeskTemplate> receiver) {
  desk_template_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindDeviceAttributes(
    mojo::PendingReceiver<mojom::DeviceAttributes> receiver) {
  device_attributes_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindDeviceLocalAccountExtensionService(
    mojo::PendingReceiver<mojom::DeviceLocalAccountExtensionService> receiver) {
  device_local_account_extension_service_ash_->BindReceiver(
      std::move(receiver));
}

void CrosapiAsh::BindDeviceOAuth2TokenService(
    mojo::PendingReceiver<mojom::DeviceOAuth2TokenService> receiver) {
  device_oauth2_token_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindDeviceSettingsService(
    mojo::PendingReceiver<mojom::DeviceSettingsService> receiver) {
  device_settings_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindDiagnosticsService(
    mojo::PendingReceiver<mojom::DiagnosticsService> receiver) {
  diagnostics_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindDigitalGoodsFactory(
    mojo::PendingReceiver<mojom::DigitalGoodsFactory> receiver) {
  digital_goods_factory_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindDlp(mojo::PendingReceiver<mojom::Dlp> receiver) {
  dlp_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindDocumentScan(
    mojo::PendingReceiver<mojom::DocumentScan> receiver) {
  document_scan_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindDownloadController(
    mojo::PendingReceiver<mojom::DownloadController> receiver) {
  download_controller_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindDriveIntegrationService(
    mojo::PendingReceiver<crosapi::mojom::DriveIntegrationService> receiver) {
  drive_integration_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindEchoPrivate(
    mojo::PendingReceiver<mojom::EchoPrivate> receiver) {
  echo_private_ash_->BindReceiver(std::move(receiver));
}
void CrosapiAsh::BindEmojiPicker(
    mojo::PendingReceiver<mojom::EmojiPicker> receiver) {
  emoji_picker_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindExtensionInfoPrivate(
    mojo::PendingReceiver<mojom::ExtensionInfoPrivate> receiver) {
  extension_info_private_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindExtensionPublisher(
    mojo::PendingReceiver<mojom::AppPublisher> receiver) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  apps::StandaloneBrowserExtensionApps* extensions =
      apps::StandaloneBrowserExtensionAppsFactoryForExtension::GetForProfile(
          profile);
  extensions->RegisterCrosapiHost(std::move(receiver));
}

void CrosapiAsh::BindFeedback(mojo::PendingReceiver<mojom::Feedback> receiver) {
  feedback_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindFieldTrialService(
    mojo::PendingReceiver<crosapi::mojom::FieldTrialService> receiver) {
  field_trial_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindFileManager(
    mojo::PendingReceiver<crosapi::mojom::FileManager> receiver) {
  file_manager_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindFileSystemProviderService(
    mojo::PendingReceiver<crosapi::mojom::FileSystemProviderService> receiver) {
  file_system_provider_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindFirewallHoleService(
    mojo::PendingReceiver<crosapi::mojom::FirewallHoleService> receiver) {
  firewall_hole_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindForceInstalledTracker(
    mojo::PendingReceiver<crosapi::mojom::ForceInstalledTracker> receiver) {
  force_installed_tracker_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindFullscreenController(
    mojo::PendingReceiver<crosapi::mojom::FullscreenController> receiver) {
  fullscreen_controller_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindGeolocationService(
    mojo::PendingReceiver<crosapi::mojom::GeolocationService> receiver) {
  geolocation_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindHidManager(
    mojo::PendingReceiver<device::mojom::HidManager> receiver) {
  content::GetDeviceService().BindHidManager(std::move(receiver));
}

void CrosapiAsh::BindHoldingSpaceService(
    mojo::PendingReceiver<mojom::HoldingSpaceService> receiver) {
  // Given `GetAshProfile()` assumptions, there is 1 and only 1
  // `HoldingSpaceKeyedService` that can/should be contacted - the one attached
  // to the regular `Profile` in ash-chrome for the active `User`.
  ash::HoldingSpaceKeyedService* holding_space_keyed_service =
      ash::HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
          GetAshProfile());
  if (holding_space_keyed_service)
    holding_space_keyed_service->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindIdentityManager(
    mojo::PendingReceiver<crosapi::mojom::IdentityManager> receiver) {
  identity_manager_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindIdleService(
    mojo::PendingReceiver<crosapi::mojom::IdleService> receiver) {
  idle_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindImageWriter(
    mojo::PendingReceiver<mojom::ImageWriter> receiver) {
  image_writer_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindInSessionAuth(
    mojo::PendingReceiver<mojom::InSessionAuth> receiver) {
  in_session_auth_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindKeystoreService(
    mojo::PendingReceiver<crosapi::mojom::KeystoreService> receiver) {
  keystore_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindKioskSessionService(
    mojo::PendingReceiver<mojom::KioskSessionService> receiver) {
  kiosk_session_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindLocalPrinter(
    mojo::PendingReceiver<crosapi::mojom::LocalPrinter> receiver) {
  local_printer_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindLogin(
    mojo::PendingReceiver<crosapi::mojom::Login> receiver) {
  login_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindLoginScreenStorage(
    mojo::PendingReceiver<crosapi::mojom::LoginScreenStorage> receiver) {
  login_screen_storage_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindLoginState(
    mojo::PendingReceiver<crosapi::mojom::LoginState> receiver) {
  login_state_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindMachineLearningService(
    mojo::PendingReceiver<
        chromeos::machine_learning::mojom::MachineLearningService> receiver) {
  chromeos::machine_learning::ServiceConnection::GetInstance()
      ->BindMachineLearningService(std::move(receiver));
}

void CrosapiAsh::BindMediaUI(mojo::PendingReceiver<mojom::MediaUI> receiver) {
  media_ui_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindMediaSessionAudioFocus(
    mojo::PendingReceiver<media_session::mojom::AudioFocusManager> receiver) {
  content::GetMediaSessionService().BindAudioFocusManager(std::move(receiver));
}

void CrosapiAsh::BindMediaSessionAudioFocusDebug(
    mojo::PendingReceiver<media_session::mojom::AudioFocusManagerDebug>
        receiver) {
  content::GetMediaSessionService().BindAudioFocusManagerDebug(
      std::move(receiver));
}

void CrosapiAsh::BindMediaSessionController(
    mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
        receiver) {
  content::GetMediaSessionService().BindMediaControllerManager(
      std::move(receiver));
}

void CrosapiAsh::BindMessageCenter(
    mojo::PendingReceiver<mojom::MessageCenter> receiver) {
  message_center_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindMetrics(mojo::PendingReceiver<mojom::Metrics> receiver) {
  metrics_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindMetricsReporting(
    mojo::PendingReceiver<mojom::MetricsReporting> receiver) {
  metrics_reporting_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindMultiCaptureService(
    mojo::PendingReceiver<mojom::MultiCaptureService> receiver) {
  multi_capture_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindNativeThemeService(
    mojo::PendingReceiver<crosapi::mojom::NativeThemeService> receiver) {
  native_theme_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindNetworkChange(
    mojo::PendingReceiver<crosapi::mojom::NetworkChange> receiver) {
  network_change_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindNetworkSettingsService(
    ::mojo::PendingReceiver<::crosapi::mojom::NetworkSettingsService>
        receiver) {
  network_settings_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindNetworkingAttributes(
    mojo::PendingReceiver<mojom::NetworkingAttributes> receiver) {
  networking_attributes_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindNetworkingPrivate(
    mojo::PendingReceiver<mojom::NetworkingPrivate> receiver) {
  networking_private_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindParentAccess(
    mojo::PendingReceiver<mojom::ParentAccess> receiver) {
  parent_access_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindPolicyService(
    mojo::PendingReceiver<mojom::PolicyService> receiver) {
  policy_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindPower(mojo::PendingReceiver<mojom::Power> receiver) {
  power_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindPrefs(mojo::PendingReceiver<mojom::Prefs> receiver) {
  prefs_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindPrintingMetrics(
    mojo::PendingReceiver<mojom::PrintingMetrics> receiver) {
#if BUILDFLAG(USE_CUPS)
  printing_metrics_ash_->BindReceiver(std::move(receiver));
#endif  // BUILDFLAG(USE_CUPS)
}

void CrosapiAsh::BindReceiver(
    mojo::PendingReceiver<mojom::Crosapi> pending_receiver,
    CrosapiId crosapi_id,
    base::OnceClosure disconnect_handler) {
  mojo::ReceiverId id =
      receiver_set_.Add(this, std::move(pending_receiver), crosapi_id);
  if (!disconnect_handler.is_null())
    disconnect_handler_map_.emplace(id, std::move(disconnect_handler));
}

void CrosapiAsh::BindRemoteAppsLacrosBridge(
    mojo::PendingReceiver<chromeos::remote_apps::mojom::RemoteAppsLacrosBridge>
        receiver) {
  ash::RemoteAppsManager* remote_apps_manager =
      ash::RemoteAppsManagerFactory::GetForProfile(GetAshProfile());

  // RemoteApps are only available for managed guest sessions.
  if (!remote_apps_manager)
    return;
  remote_apps_manager->BindLacrosBridgeInterface(std::move(receiver));
}

void CrosapiAsh::BindRemoting(mojo::PendingReceiver<mojom::Remoting> receiver) {
  remoting_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindResourceManager(
    mojo::PendingReceiver<mojom::ResourceManager> receiver) {
  resource_manager_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindScreenManager(
    mojo::PendingReceiver<mojom::ScreenManager> receiver) {
  screen_manager_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindSearchControllerRegistry(
    mojo::PendingReceiver<mojom::SearchControllerRegistry> receiver) {
  search_provider_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindSelectFile(
    mojo::PendingReceiver<mojom::SelectFile> receiver) {
  select_file_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindSensorHalClient(
    mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient> remote) {
  chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterClient(
      std::move(remote));
}

void CrosapiAsh::BindSharesheet(
    mojo::PendingReceiver<mojom::Sharesheet> receiver) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  sharesheet_ash_->MaybeSetProfile(profile);
  sharesheet_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindSmartReaderClient(
    mojo::PendingRemote<mojom::SmartReaderClient> remote) {
  smart_reader_manager_ash_->BindRemote(std::move(remote));
}

void CrosapiAsh::BindSpeechRecognition(
    mojo::PendingReceiver<mojom::SpeechRecognition> receiver) {
  speech_recognition_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindStableVideoDecoderFactory(
    mojo::GenericPendingReceiver receiver) {
#if BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)
  // TODO(b/171813538): if launching out-of-process video decoding for LaCrOS
  // with Finch, we may need to tell LaCrOS somehow if this feature is enabled
  // in ash-chrome. Otherwise, we may run into a situation in which the feature
  // is enabled for LaCrOS but not for ash-chrome.
  auto r = receiver.As<media::stable::mojom::StableVideoDecoderFactory>();
  if (r && base::FeatureList::IsEnabled(media::kUseOutOfProcessVideoDecoding))
    content::LaunchStableVideoDecoderFactory(std::move(r));
#endif  // BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)
}

void CrosapiAsh::BindStructuredMetricsService(
    mojo::PendingReceiver<crosapi::mojom::StructuredMetricsService> receiver) {
  structured_metrics_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindSyncService(
    mojo::PendingReceiver<mojom::SyncService> receiver) {
  ash::SyncMojoServiceAsh* sync_mojo_service_ash =
      ash::SyncMojoServiceFactoryAsh::GetForProfile(GetAshProfile());
  if (!sync_mojo_service_ash) {
    // |sync_mojo_service_ash| is not always available. In particular, sync can
    // be completely disabled via command line flags.
    return;
  }
  sync_mojo_service_ash->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindTaskManager(
    mojo::PendingReceiver<mojom::TaskManager> receiver) {
  task_manager_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindTelemetryEventService(
    mojo::PendingReceiver<mojom::TelemetryEventService> receiver) {
  telemetry_event_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindTelemetryProbeService(
    mojo::PendingReceiver<mojom::TelemetryProbeService> receiver) {
  probe_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindTestController(
    mojo::PendingReceiver<mojom::TestController> receiver) {
  if (test_controller_)
    test_controller_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindTimeZoneService(
    mojo::PendingReceiver<mojom::TimeZoneService> receiver) {
  time_zone_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindTts(mojo::PendingReceiver<mojom::Tts> receiver) {
  tts_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindUrlHandler(
    mojo::PendingReceiver<mojom::UrlHandler> receiver) {
  url_handler_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindVideoCaptureDeviceFactory(
    mojo::PendingReceiver<mojom::VideoCaptureDeviceFactory> receiver) {
  content::GetVideoCaptureService().BindVideoCaptureDeviceFactory(
      std::move(receiver));
}

void CrosapiAsh::BindVideoConferenceManager(
    mojo::PendingReceiver<mojom::VideoConferenceManager> receiver) {
  video_conference_manager_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindVirtualKeyboard(
    mojo::PendingReceiver<mojom::VirtualKeyboard> receiver) {
  virtual_keyboard_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindVolumeManager(
    mojo::PendingReceiver<crosapi::mojom::VolumeManager> receiver) {
  volume_manager_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindVpnExtensionObserver(
    mojo::PendingReceiver<crosapi::mojom::VpnExtensionObserver> receiver) {
  vpn_extension_observer_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindVpnService(
    mojo::PendingReceiver<mojom::VpnService> receiver) {
  vpn_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindWallpaper(
    mojo::PendingReceiver<mojom::Wallpaper> receiver) {
  wallpaper_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindWebAppService(
    mojo::PendingReceiver<mojom::WebAppService> receiver) {
  web_app_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindWebPageInfoFactory(
    mojo::PendingReceiver<mojom::WebPageInfoFactory> receiver) {
  web_page_info_factory_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindWebAppPublisher(
    mojo::PendingReceiver<mojom::AppPublisher> receiver) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  apps::WebAppsCrosapi* web_apps =
      apps::WebAppsCrosapiFactory::GetForProfile(profile);
  web_apps->RegisterWebAppsCrosapiHost(std::move(receiver));
}

void CrosapiAsh::OnBrowserStartup(mojom::BrowserInfoPtr browser_info) {
  BrowserManager::Get()->set_browser_version(browser_info->browser_version);
}

void CrosapiAsh::REMOVED_29(
    mojo::PendingReceiver<mojom::SystemDisplayDeprecated> receiver) {
  NOTIMPLEMENTED();
}

void CrosapiAsh::SetTestControllerForTesting(
    TestControllerReceiver* test_controller) {
  test_controller_ = test_controller;
}

void CrosapiAsh::OnDisconnected() {
  auto it = disconnect_handler_map_.find(receiver_set_.current_receiver());
  if (it == disconnect_handler_map_.end())
    return;

  base::OnceClosure callback = std::move(it->second);
  disconnect_handler_map_.erase(it);
  std::move(callback).Run();
}

}  // namespace crosapi
