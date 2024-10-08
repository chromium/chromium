// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/crosapi_ash.h"

#include <memory>
#include <utility>

#include "ash/public/ash_interfaces.h"
#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/publishers/standalone_browser_apps.h"
#include "chrome/browser/apps/app_service/publishers/standalone_browser_extension_apps.h"
#include "chrome/browser/apps/app_service/publishers/standalone_browser_extension_apps_factory.h"
#include "chrome/browser/apps/app_service/publishers/web_apps_crosapi.h"
#include "chrome/browser/apps/app_service/publishers/web_apps_crosapi_factory.h"
#include "chrome/browser/apps/app_service/subscriber_crosapi.h"
#include "chrome/browser/apps/app_service/subscriber_crosapi_factory.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_registry.h"
#include "chrome/browser/apps/digital_goods/digital_goods_ash.h"
#include "chrome/browser/ash/crosapi/arc_ash.h"
#include "chrome/browser/ash/crosapi/audio_service_ash.h"
#include "chrome/browser/ash/crosapi/automation_ash.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_service_host_ash.h"
#include "chrome/browser/ash/crosapi/browser_version_service_ash.h"
#include "chrome/browser/ash/crosapi/cec_private_ash.h"
#include "chrome/browser/ash/crosapi/cert_database_ash.h"
#include "chrome/browser/ash/crosapi/cert_provisioning_ash.h"
#include "chrome/browser/ash/crosapi/chaps_service_ash.h"
#include "chrome/browser/ash/crosapi/chrome_app_kiosk_service_ash.h"
#include "chrome/browser/ash/crosapi/chrome_app_window_tracker_ash.h"
#include "chrome/browser/ash/crosapi/clipboard_ash.h"
#include "chrome/browser/ash/crosapi/clipboard_history_ash.h"
#include "chrome/browser/ash/crosapi/content_protection_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_dependency_registry.h"
#include "chrome/browser/ash/crosapi/debug_interface_registerer_ash.h"
#include "chrome/browser/ash/crosapi/desk_ash.h"
#include "chrome/browser/ash/crosapi/desk_profiles_ash.h"
#include "chrome/browser/ash/crosapi/desk_template_ash.h"
#include "chrome/browser/ash/crosapi/device_attributes_ash.h"
#include "chrome/browser/ash/crosapi/device_local_account_extension_service_ash.h"
#include "chrome/browser/ash/crosapi/device_oauth2_token_service_ash.h"
#include "chrome/browser/ash/crosapi/device_settings_ash.h"
#include "chrome/browser/ash/crosapi/dlp_ash.h"
#include "chrome/browser/ash/crosapi/document_scan_ash.h"
#include "chrome/browser/ash/crosapi/download_controller_ash.h"
#include "chrome/browser/ash/crosapi/download_status_updater_ash.h"
#include "chrome/browser/ash/crosapi/drive_integration_service_ash.h"
#include "chrome/browser/ash/crosapi/echo_private_ash.h"
#include "chrome/browser/ash/crosapi/embedded_accessibility_helper_client_ash.h"
#include "chrome/browser/ash/crosapi/emoji_picker_ash.h"
#include "chrome/browser/ash/crosapi/extension_info_private_ash.h"
#include "chrome/browser/ash/crosapi/extension_printer_service_ash.h"
#include "chrome/browser/ash/crosapi/eye_dropper_ash.h"
#include "chrome/browser/ash/crosapi/field_trial_service_ash.h"
#include "chrome/browser/ash/crosapi/file_change_service_bridge_ash.h"
#include "chrome/browser/ash/crosapi/file_manager_ash.h"
#include "chrome/browser/ash/crosapi/file_system_access_cloud_identifier_provider_ash.h"
#include "chrome/browser/ash/crosapi/file_system_provider_service_ash.h"
#include "chrome/browser/ash/crosapi/force_installed_tracker_ash.h"
#include "chrome/browser/ash/crosapi/full_restore_ash.h"
#include "chrome/browser/ash/crosapi/fullscreen_controller_ash.h"
#include "chrome/browser/ash/crosapi/geolocation_service_ash.h"
#include "chrome/browser/ash/crosapi/identity_manager_ash.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/browser/ash/crosapi/image_writer_ash.h"
#include "chrome/browser/ash/crosapi/input_methods_ash.h"
#include "chrome/browser/ash/crosapi/kerberos_in_browser_ash.h"
#include "chrome/browser/ash/crosapi/keystore_service_ash.h"
#include "chrome/browser/ash/crosapi/kiosk_session_service_ash.h"
#include "chrome/browser/ash/crosapi/lacros_shelf_item_tracker.h"
#include "chrome/browser/ash/crosapi/local_printer_ash.h"
#include "chrome/browser/ash/crosapi/login_ash.h"
#include "chrome/browser/ash/crosapi/login_screen_storage_ash.h"
#include "chrome/browser/ash/crosapi/login_state_ash.h"
#include "chrome/browser/ash/crosapi/media_app_ash.h"
#include "chrome/browser/ash/crosapi/media_ui_ash.h"
#include "chrome/browser/ash/crosapi/message_center_ash.h"
#include "chrome/browser/ash/crosapi/metrics_ash.h"
#include "chrome/browser/ash/crosapi/metrics_reporting_ash.h"
#include "chrome/browser/ash/crosapi/multi_capture_service_ash.h"
#include "chrome/browser/ash/crosapi/native_theme_service_ash.h"
#include "chrome/browser/ash/crosapi/network_settings_service_ash.h"
#include "chrome/browser/ash/crosapi/networking_attributes_ash.h"
#include "chrome/browser/ash/crosapi/networking_private_ash.h"
#include "chrome/browser/ash/crosapi/nonclosable_app_toast_service_ash.h"
#include "chrome/browser/ash/crosapi/one_drive_integration_service_ash.h"
#include "chrome/browser/ash/crosapi/one_drive_notification_service_ash.h"
#include "chrome/browser/ash/crosapi/parent_access_ash.h"
#include "chrome/browser/ash/crosapi/payment_app_instance_ash.h"
#include "chrome/browser/ash/crosapi/policy_service_ash.h"
#include "chrome/browser/ash/crosapi/power_ash.h"
#include "chrome/browser/ash/crosapi/prefs_ash.h"
#include "chrome/browser/ash/crosapi/remoting_ash.h"
#include "chrome/browser/ash/crosapi/resource_manager_ash.h"
#include "chrome/browser/ash/crosapi/screen_ai_downloader_ash.h"
#include "chrome/browser/ash/crosapi/screen_manager_ash.h"
#include "chrome/browser/ash/crosapi/search_provider_ash.h"
#include "chrome/browser/ash/crosapi/sharesheet_ash.h"
#include "chrome/browser/ash/crosapi/speech_recognition_ash.h"
#include "chrome/browser/ash/crosapi/structured_metrics_service_ash.h"
#include "chrome/browser/ash/crosapi/suggestion_service_ash.h"
#include "chrome/browser/ash/crosapi/task_manager_ash.h"
#include "chrome/browser/ash/crosapi/time_zone_service_ash.h"
#include "chrome/browser/ash/crosapi/virtual_keyboard_ash.h"
#include "chrome/browser/ash/crosapi/volume_manager_ash.h"
#include "chrome/browser/ash/crosapi/vpn_extension_observer_ash.h"
#include "chrome/browser/ash/crosapi/vpn_service_ash.h"
#include "chrome/browser/ash/crosapi/wallpaper_ash.h"
#include "chrome/browser/ash/crosapi/web_app_service_ash.h"
#include "chrome/browser/ash/crosapi/web_kiosk_service_ash.h"
#include "chrome/browser/ash/crosapi/web_page_info_ash.h"
#include "chrome/browser/ash/input_method/editor_mediator_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/magic_boost/magic_boost_controller_ash.h"
#include "chrome/browser/ash/printing/print_preview/print_preview_webcontents_adapter_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/remote_apps/remote_apps_manager_factory.h"
#include "chrome/browser/ash/sync/sync_mojo_service_ash.h"
#include "chrome/browser/ash/sync/sync_mojo_service_factory_ash.h"
#include "chrome/browser/ash/video_conference/video_conference_manager_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/tts_ash.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/telemetry_extension/diagnostics/diagnostics_service_ash.h"
#include "chromeos/ash/components/telemetry_extension/events/telemetry_event_service_ash.h"
#include "chromeos/ash/components/telemetry_extension/management/telemetry_management_service_ash.h"
#include "chromeos/ash/components/telemetry_extension/routines/telemetry_diagnostic_routine_service_ash.h"
#include "chromeos/ash/components/telemetry_extension/telemetry/probe_service_ash.h"
#include "chromeos/components/cdm_factory_daemon/cdm_factory_daemon_proxy_ash.h"
#include "chromeos/components/in_session_auth/in_process_instances.h"
#include "chromeos/components/in_session_auth/in_session_auth.h"
#include "chromeos/components/sensors/ash/sensor_hal_dispatcher.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/device_local_account_extension_service.mojom.h"
#include "chromeos/crosapi/mojom/drive_integration_service.mojom.h"
#include "chromeos/crosapi/mojom/embedded_accessibility_helper.mojom.h"
#include "chromeos/crosapi/mojom/extension_printer.mojom.h"
#include "chromeos/crosapi/mojom/eye_dropper.mojom.h"
#include "chromeos/crosapi/mojom/file_change_service_bridge.mojom.h"
#include "chromeos/crosapi/mojom/file_manager.mojom.h"
#include "chromeos/crosapi/mojom/firewall_hole.mojom.h"
#include "chromeos/crosapi/mojom/image_writer.mojom.h"
#include "chromeos/crosapi/mojom/kerberos_in_browser.mojom.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "chromeos/crosapi/mojom/lacros_shelf_item_tracker.mojom.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/crosapi/mojom/magic_boost.mojom.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/multi_capture_service.mojom.h"
#include "chromeos/crosapi/mojom/passkeys.mojom.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/crosapi/mojom/task_manager.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"
#include "components/trusted_vault/features.h"
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
// 1. TODO(crbug.com/40704278): Multi-Signin / Fast-User-Switching is disabled.
// 2. ash-chrome has 1 and only 1 "regular" `Profile`.
Profile* GetAshProfile() {
#if DCHECK_IS_ON()
  int num_regular_profiles = 0;
  for (const Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    if (ash::ProfileHelper::IsUserProfile(profile)) {
      ++num_regular_profiles;
    }
  }
  DCHECK_EQ(1, num_regular_profiles);
#endif  // DCHECK_IS_ON()
  return ash::ProfileHelper::Get()->GetProfileByUser(
      user_manager::UserManager::Get()->GetActiveUser());
}

}  // namespace

CrosapiAsh::TestControllerReceiver::~TestControllerReceiver() = default;

CrosapiAsh::CrosapiAsh(CrosapiDependencyRegistry* registry)
    : arc_ash_(std::make_unique<ArcAsh>()),
      audio_service_ash_(std::make_unique<AudioServiceAsh>()),
      automation_ash_(std::make_unique<AutomationAsh>()),
      browser_service_host_ash_(std::make_unique<BrowserServiceHostAsh>()),
      browser_version_service_ash_(std::make_unique<BrowserVersionServiceAsh>(
          g_browser_process->component_updater())),
      cec_private_ash_(std::make_unique<CecPrivateAsh>()),
      cert_database_ash_(std::make_unique<CertDatabaseAsh>()),
      cert_provisioning_ash_(std::make_unique<CertProvisioningAsh>()),
      chaps_service_ash_(std::make_unique<ChapsServiceAsh>()),
      chrome_app_kiosk_service_ash_(
          std::make_unique<ChromeAppKioskServiceAsh>()),
      chrome_app_window_tracker_ash_(
          std::make_unique<ChromeAppWindowTrackerAsh>()),
      clipboard_ash_(std::make_unique<ClipboardAsh>()),
      clipboard_history_ash_(std::make_unique<ClipboardHistoryAsh>()),
      content_protection_ash_(std::make_unique<ContentProtectionAsh>()),
      debug_interface_registerer_ash_(
          std::make_unique<DebugInterfaceRegistererAsh>()),
      desk_ash_(std::make_unique<DeskAsh>()),
      desk_profiles_ash_(std::make_unique<DeskProfilesAsh>()),
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
      embedded_accessibility_helper_client_ash_(
          std::make_unique<EmbeddedAccessibilityHelperClientAsh>()),
      emoji_picker_ash_(std::make_unique<EmojiPickerAsh>()),
      extension_info_private_ash_(std::make_unique<ExtensionInfoPrivateAsh>()),
      extension_printer_service_ash_(
          std::make_unique<ExtensionPrinterServiceAsh>()),
      eye_dropper_ash_(std::make_unique<EyeDropperAsh>()),
      field_trial_service_ash_(std::make_unique<FieldTrialServiceAsh>()),
      file_manager_ash_(std::make_unique<FileManagerAsh>()),
      file_system_access_cloud_identifier_provider_ash_(
          std::make_unique<FileSystemAccessCloudIdentifierProviderAsh>()),
      file_system_provider_service_ash_(
          std::make_unique<FileSystemProviderServiceAsh>()),
      force_installed_tracker_ash_(
          std::make_unique<ForceInstalledTrackerAsh>()),
      full_restore_ash_(std::make_unique<FullRestoreAsh>()),
      fullscreen_controller_ash_(std::make_unique<FullscreenControllerAsh>()),
      geolocation_service_ash_(std::make_unique<GeolocationServiceAsh>()),
      identity_manager_ash_(std::make_unique<IdentityManagerAsh>()),
      idle_service_ash_(std::make_unique<IdleServiceAsh>()),
      input_methods_ash_(std::make_unique<InputMethodsAsh>()),
      image_writer_ash_(std::make_unique<ImageWriterAsh>()),
      kerberos_in_browser_ash_(std::make_unique<KerberosInBrowserAsh>()),
      keystore_service_ash_(std::make_unique<KeystoreServiceAsh>()),
      kiosk_session_service_ash_(std::make_unique<KioskSessionServiceAsh>()),
      lacros_shelf_item_tracker_(std::make_unique<LacrosShelfItemTracker>()),
      local_printer_ash_(std::make_unique<LocalPrinterAsh>()),
      login_ash_(std::make_unique<LoginAsh>()),
      login_screen_storage_ash_(std::make_unique<LoginScreenStorageAsh>()),
      login_state_ash_(std::make_unique<LoginStateAsh>()),
      magic_boost_controller_ash_(
          std::make_unique<ash::MagicBoostControllerAsh>()),
      media_app_ash_(std::make_unique<MediaAppAsh>()),
      media_ui_ash_(std::make_unique<MediaUIAsh>()),
      message_center_ash_(std::make_unique<MessageCenterAsh>()),
      metrics_ash_(std::make_unique<MetricsAsh>()),
      metrics_reporting_ash_(registry->CreateMetricsReportingAsh(
          g_browser_process->metrics_service())),
      multi_capture_service_ash_(std::make_unique<MultiCaptureServiceAsh>()),
      native_theme_service_ash_(std::make_unique<NativeThemeServiceAsh>()),
      networking_attributes_ash_(std::make_unique<NetworkingAttributesAsh>()),
      networking_private_ash_(std::make_unique<NetworkingPrivateAsh>()),
      network_settings_service_ash_(std::make_unique<NetworkSettingsServiceAsh>(
          g_browser_process->platform_part()->ash_proxy_monitor())),
      one_drive_notification_service_ash_(
          std::make_unique<OneDriveNotificationServiceAsh>()),
      one_drive_integration_service_ash_(
          std::make_unique<OneDriveIntegrationServiceAsh>()),
      parent_access_ash_(std::make_unique<ParentAccessAsh>()),
      payment_app_instance_ash_(std::make_unique<PaymentAppInstanceAsh>()),
      policy_service_ash_(std::make_unique<PolicyServiceAsh>()),
      power_ash_(std::make_unique<PowerAsh>()),
      prefs_ash_(
          std::make_unique<PrefsAsh>(g_browser_process->profile_manager(),
                                     g_browser_process->local_state())),
      nonclosable_app_toast_service_ash_(
          std::make_unique<NonclosableAppToastServiceAsh>()),
#if BUILDFLAG(USE_CUPS)
      printing_metrics_ash_(std::make_unique<PrintingMetricsAsh>()),
#endif  // BUILDFLAG(USE_CUPS)
      telemetry_diagnostic_routine_service_ash_(
          std::make_unique<ash::TelemetryDiagnosticsRoutineServiceAsh>()),
      telemetry_event_service_ash_(
          std::make_unique<ash::TelemetryEventServiceAsh>()),
      telemetry_management_service_ash_(
          std::make_unique<ash::TelemetryManagementServiceAsh>()),
      probe_service_ash_(std::make_unique<ash::ProbeServiceAsh>()),
      remoting_ash_(std::make_unique<RemotingAsh>()),
      resource_manager_ash_(std::make_unique<ResourceManagerAsh>()),
      print_preview_webcontents_adapter_ash_(
          std::make_unique<ash::printing::PrintPreviewWebcontentsAdapterAsh>()),
      screen_ai_downloader_ash_(std::make_unique<ScreenAIDownloaderAsh>()),
      screen_manager_ash_(std::make_unique<ScreenManagerAsh>()),
      search_provider_ash_(std::make_unique<SearchProviderAsh>()),
      sharesheet_ash_(std::make_unique<SharesheetAsh>()),
      smart_reader_manager_ash_(std::make_unique<ash::SmartReaderManagerAsh>()),
      speech_recognition_ash_(std::make_unique<SpeechRecognitionAsh>()),
      structured_metrics_service_ash_(
          std::make_unique<StructuredMetricsServiceAsh>()),
      suggestion_service_ash_(std::make_unique<SuggestionServiceAsh>()),
      task_manager_ash_(std::make_unique<TaskManagerAsh>()),
      time_zone_service_ash_(std::make_unique<TimeZoneServiceAsh>()),
      tts_ash_(std::make_unique<TtsAsh>(g_browser_process->profile_manager())),
      video_conference_manager_ash_(
          std::make_unique<ash::VideoConferenceManagerAsh>()),
      virtual_keyboard_ash_(std::make_unique<VirtualKeyboardAsh>()),
      volume_manager_ash_(std::make_unique<VolumeManagerAsh>()),
      vpn_extension_observer_ash_(std::make_unique<VpnExtensionObserverAsh>()),
      vpn_service_ash_(std::make_unique<VpnServiceAsh>()),
      wallpaper_ash_(std::make_unique<WallpaperAsh>()),
      web_app_service_ash_(std::make_unique<WebAppServiceAsh>()),
      web_kiosk_service_ash_(std::make_unique<WebKioskServiceAsh>()),
      web_page_info_factory_ash_(std::make_unique<WebPageInfoFactoryAsh>()) {
  receiver_set_.set_disconnect_handler(base::BindRepeating(
      &CrosapiAsh::OnDisconnected, weak_factory_.GetWeakPtr()));
}

CrosapiAsh::~CrosapiAsh() {
  // Invoke all disconnect handlers.
  auto handlers = std::move(disconnect_handler_map_);
  for (auto& entry : handlers) {
    std::move(entry.second).Run();
  }
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
  if (auto r = receiver.As<chromeos::cdm::mojom::BrowserCdmFactory>()) {
    chromeos::CdmFactoryDaemonProxyAsh::Create(std::move(r));
  }
}

void CrosapiAsh::BindBrowserServiceHost(
    mojo::PendingReceiver<crosapi::mojom::BrowserServiceHost> receiver) {
  browser_service_host_ash_->BindReceiver(receiver_set_.current_context(),
                                          std::move(receiver));
}

void CrosapiAsh::BindBrowserShortcutPublisher(
    mojo::PendingReceiver<mojom::AppShortcutPublisher> receiver) {
  // TODO(b/352513798): Remove after M131.
  NOTIMPLEMENTED_LOG_ONCE();
}

void CrosapiAsh::BindBrowserVersionService(
    mojo::PendingReceiver<crosapi::mojom::BrowserVersionService> receiver) {
  browser_version_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindCecPrivate(
    mojo::PendingReceiver<mojom::CecPrivate> receiver) {
  cec_private_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindCertDatabase(
    mojo::PendingReceiver<mojom::CertDatabase> receiver) {
  cert_database_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindCertProvisioning(
    mojo::PendingReceiver<mojom::CertProvisioning> receiver) {
  cert_provisioning_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindCfmServiceContext(
    mojo::PendingReceiver<chromeos::cfm::mojom::CfmServiceContext> receiver) {
  chromeos::cfm::ServiceConnection::GetInstance()->BindServiceContext(
      std::move(receiver));
}

void CrosapiAsh::BindChapsService(
    mojo::PendingReceiver<mojom::ChapsService> receiver) {
  chaps_service_ash_->BindReceiver(std::move(receiver));
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

void CrosapiAsh::BindDebugInterfaceRegisterer(
    mojo::PendingReceiver<mojom::DebugInterfaceRegisterer> receiver) {
  debug_interface_registerer_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindDesk(mojo::PendingReceiver<mojom::Desk> receiver) {
  desk_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindDeskProfileObserver(
    mojo::PendingReceiver<mojom::DeskProfileObserver> receiver) {
  desk_profiles_ash_->BindReceiver(std::move(receiver));
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

void CrosapiAsh::BindDownloadStatusUpdater(
    mojo::PendingReceiver<mojom::DownloadStatusUpdater> receiver) {
  // Delay creating `download_status_updater_ash_` until binding so that the Ash
  // profile is ready.
  if (!download_status_updater_ash_) {
    Profile* const profile = GetAshProfile();
    CHECK(profile);
    download_status_updater_ash_ =
        std::make_unique<DownloadStatusUpdaterAsh>(profile);
  }

  download_status_updater_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindDriveIntegrationService(
    mojo::PendingReceiver<crosapi::mojom::DriveIntegrationService> receiver) {
  drive_integration_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindEchoPrivate(
    mojo::PendingReceiver<mojom::EchoPrivate> receiver) {
  echo_private_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindEditorPanelManager(
    mojo::PendingReceiver<mojom::EditorPanelManager> receiver) {
  auto* editor_mediator =
      ash::input_method::EditorMediatorFactory::GetInstance()->GetForProfile(
          GetAshProfile());
  if (editor_mediator) {
    editor_mediator->BindEditorPanelManager(std::move(receiver));
  }
}

void CrosapiAsh::BindEmbeddedAccessibilityHelperClientFactory(
    mojo::PendingReceiver<mojom::EmbeddedAccessibilityHelperClientFactory>
        receiver) {
  embedded_accessibility_helper_client_ash_
      ->BindEmbeddedAccessibilityHelperClientFactoryReceiver(
          std::move(receiver));
}

void CrosapiAsh::BindEmojiPicker(
    mojo::PendingReceiver<mojom::EmojiPicker> receiver) {
  emoji_picker_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindExtensionInfoPrivate(
    mojo::PendingReceiver<mojom::ExtensionInfoPrivate> receiver) {
  extension_info_private_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindExtensionPrinterService(
    mojo::PendingReceiver<mojom::ExtensionPrinterService> receiver) {
  extension_printer_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindExtensionPublisher(
    mojo::PendingReceiver<mojom::AppPublisher> receiver) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  apps::StandaloneBrowserExtensionApps* extensions =
      apps::StandaloneBrowserExtensionAppsFactoryForExtension::GetForProfile(
          profile);
  extensions->RegisterCrosapiHost(std::move(receiver));
}

void CrosapiAsh::BindEyeDropper(
    mojo::PendingReceiver<mojom::EyeDropper> receiver) {
  eye_dropper_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindFieldTrialService(
    mojo::PendingReceiver<crosapi::mojom::FieldTrialService> receiver) {
  field_trial_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindFileChangeServiceBridge(
    mojo::PendingReceiver<crosapi::mojom::FileChangeServiceBridge> receiver) {
  // NOTE: The `FileChangeServiceBridgeAsh` is created lazily as the Ash profile
  // is not yet ready on `CrosapiAsh` construction.
  if (!file_change_service_bridge_ash_) {
    Profile* const profile = GetAshProfile();
    CHECK(profile);
    file_change_service_bridge_ash_ =
        std::make_unique<FileChangeServiceBridgeAsh>(profile);
  }
  file_change_service_bridge_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindFileManager(
    mojo::PendingReceiver<crosapi::mojom::FileManager> receiver) {
  file_manager_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindFileSystemAccessCloudIdentifierProvider(
    mojo::PendingReceiver<
        crosapi::mojom::FileSystemAccessCloudIdentifierProvider> receiver) {
  file_system_access_cloud_identifier_provider_ash_->BindReceiver(
      std::move(receiver));
}

void CrosapiAsh::BindFileSystemProviderService(
    mojo::PendingReceiver<crosapi::mojom::FileSystemProviderService> receiver) {
  file_system_provider_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindForceInstalledTracker(
    mojo::PendingReceiver<crosapi::mojom::ForceInstalledTracker> receiver) {
  force_installed_tracker_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindFullRestore(
    mojo::PendingReceiver<crosapi::mojom::FullRestore> receiver) {
  full_restore_ash_->BindReceiver(std::move(receiver));
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

void CrosapiAsh::BindInputMethods(
    mojo::PendingReceiver<mojom::InputMethods> receiver) {
  input_methods_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindInSessionAuth(
    mojo::PendingReceiver<chromeos::auth::mojom::InSessionAuth> receiver) {
  chromeos::auth::BindToInSessionAuthService(std::move(receiver));
}

void CrosapiAsh::BindKerberosInBrowser(
    mojo::PendingReceiver<crosapi::mojom::KerberosInBrowser> receiver) {
  kerberos_in_browser_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindKeystoreService(
    mojo::PendingReceiver<crosapi::mojom::KeystoreService> receiver) {
  keystore_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindKioskSessionService(
    mojo::PendingReceiver<mojom::KioskSessionService> receiver) {
  kiosk_session_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindLacrosShelfItemTracker(
    mojo::PendingReceiver<mojom::LacrosShelfItemTracker> receiver) {
  lacros_shelf_item_tracker_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindLacrosAppPublisher(
    mojo::PendingReceiver<mojom::AppPublisher> receiver) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  auto* app_service_proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  apps::StandaloneBrowserApps* lacros_apps =
      app_service_proxy->StandaloneBrowserApps();
  if (lacros_apps) {
    lacros_apps->RegisterCrosapiHost(std::move(receiver));
  }
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

void CrosapiAsh::BindMahiBrowserDelegate(
    mojo::PendingReceiver<mojom::MahiBrowserDelegate> receiver) {
  NOTIMPLEMENTED();
}

void CrosapiAsh::BindMagicBoostController(
    mojo::PendingReceiver<mojom::MagicBoostController> receiver) {
  magic_boost_controller_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindMediaApp(mojo::PendingRemote<mojom::MediaApp> remote) {
  media_app_ash_->BindRemote(std::move(remote));
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
  NOTREACHED();
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

void CrosapiAsh::BindOneDriveNotificationService(
    mojo::PendingReceiver<mojom::OneDriveNotificationService> receiver) {
  one_drive_notification_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindOneDriveIntegrationService(
    mojo::PendingReceiver<mojom::OneDriveIntegrationService> receiver) {
  one_drive_integration_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindParentAccess(
    mojo::PendingReceiver<mojom::ParentAccess> receiver) {
  parent_access_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindPasskeyAuthenticatorDeprecated(
    mojo::PendingReceiver<mojom::PasskeyAuthenticator> receiver) {
  NOTIMPLEMENTED();
}

void CrosapiAsh::BindPaymentAppInstance(
    mojo::PendingReceiver<chromeos::payments::mojom::PaymentAppInstance>
        receiver) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  payment_app_instance_ash_->Initialize(profile);
  payment_app_instance_ash_->BindReceiver(std::move(receiver));
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

void CrosapiAsh::BindNonclosableAppToastService(
    mojo::PendingReceiver<mojom::NonclosableAppToastService> receiver) {
  nonclosable_app_toast_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindPrintPreviewCrosDelegate(
    mojo::PendingReceiver<mojom::PrintPreviewCrosDelegate> receiver) {
  print_preview_webcontents_adapter_ash_->BindReceiver(std::move(receiver));
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
  if (!disconnect_handler.is_null()) {
    disconnect_handler_map_.emplace(id, std::move(disconnect_handler));
  }
}

void CrosapiAsh::BindRemoteAppsLacrosBridge(
    mojo::PendingReceiver<chromeos::remote_apps::mojom::RemoteAppsLacrosBridge>
        receiver) {
  ash::RemoteAppsManager* remote_apps_manager =
      ash::RemoteAppsManagerFactory::GetForProfile(GetAshProfile());

  // RemoteApps are only available for managed guest sessions.
  if (!remote_apps_manager) {
    return;
  }
  remote_apps_manager->BindLacrosBridgeInterface(std::move(receiver));
}

void CrosapiAsh::BindRemoting(mojo::PendingReceiver<mojom::Remoting> receiver) {
  remoting_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindResourceManager(
    mojo::PendingReceiver<mojom::ResourceManager> receiver) {
  resource_manager_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindScreenAIDownloader(
    mojo::PendingReceiver<mojom::ScreenAIDownloader> receiver) {
  screen_ai_downloader_ash_->Bind(std::move(receiver));
}

void CrosapiAsh::BindScreenManager(
    mojo::PendingReceiver<mojom::ScreenManager> receiver) {
  screen_manager_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindSearchControllerFactory(
    mojo::PendingRemote<mojom::SearchControllerFactory> remote) {
  NOTREACHED();
}

void CrosapiAsh::BindSearchControllerRegistry(
    mojo::PendingReceiver<mojom::SearchControllerRegistry> receiver) {
  search_provider_ash_->BindReceiver(std::move(receiver));
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
  auto r = receiver.As<media::stable::mojom::StableVideoDecoderFactory>();
  if (r && base::FeatureList::IsEnabled(
               media::kExposeOutOfProcessVideoDecodingToLacros)) {
    content::LaunchStableVideoDecoderFactory(std::move(r));
  }
#endif  // BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)
}

void CrosapiAsh::BindStructuredMetricsService(
    mojo::PendingReceiver<crosapi::mojom::StructuredMetricsService> receiver) {
  structured_metrics_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindSuggestionService(
    mojo::PendingReceiver<crosapi::mojom::SuggestionService> receiver) {
  suggestion_service_ash_->BindReceiver(std::move(receiver));
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

void CrosapiAsh::BindTelemetryDiagnosticRoutinesService(
    mojo::PendingReceiver<mojom::TelemetryDiagnosticRoutinesService> receiver) {
  telemetry_diagnostic_routine_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindTelemetryEventService(
    mojo::PendingReceiver<mojom::TelemetryEventService> receiver) {
  telemetry_event_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindTelemetryManagementService(
    mojo::PendingReceiver<mojom::TelemetryManagementService> receiver) {
  telemetry_management_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindTelemetryProbeService(
    mojo::PendingReceiver<mojom::TelemetryProbeService> receiver) {
  probe_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindTestController(
    mojo::PendingReceiver<mojom::TestController> receiver) {
  if (test_controller_) {
    test_controller_->BindReceiver(std::move(receiver));
  }
}

void CrosapiAsh::BindTimeZoneService(
    mojo::PendingReceiver<mojom::TimeZoneService> receiver) {
  time_zone_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindTrustedVaultBackend(
    mojo::PendingReceiver<mojom::TrustedVaultBackend> receiver) {
  // Can be safely removed from Crosapi.
}

void CrosapiAsh::BindTrustedVaultBackendService(
    mojo::PendingReceiver<mojom::TrustedVaultBackendService> receiver) {
  // Can be safely removed from Crosapi.
}

void CrosapiAsh::BindTts(mojo::PendingReceiver<mojom::Tts> receiver) {
  tts_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindUrlHandler(
    mojo::PendingReceiver<mojom::UrlHandler> receiver) {
  NOTREACHED();
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
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  Profile* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(user));
  volume_manager_ash_->SetProfile(profile);
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

void CrosapiAsh::BindWebKioskService(
    mojo::PendingReceiver<mojom::WebKioskService> receiver) {
  web_kiosk_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindWebPageInfoFactory(
    mojo::PendingReceiver<mojom::WebPageInfoFactory> receiver) {
  web_page_info_factory_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindGuestOsSkForwarderFactory(
    mojo::PendingReceiver<mojom::GuestOsSkForwarderFactory> receiver) {
  NOTREACHED();
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

void CrosapiAsh::REMOVED_105(
    mojo::PendingReceiver<crosapi::mojom::FirewallHoleServiceDeprecated>
        receiver) {
  NOTIMPLEMENTED();
}

void CrosapiAsh::REMOVED_62(
    mojo::PendingReceiver<crosapi::mojom::AuthenticationDeprecated> receiver) {
  NOTIMPLEMENTED();
}

void CrosapiAsh::SetTestControllerForTesting(
    std::unique_ptr<TestControllerReceiver> test_controller) {
  test_controller_ = std::move(test_controller);
}

void CrosapiAsh::OnDisconnected() {
  auto it = disconnect_handler_map_.find(receiver_set_.current_receiver());
  if (it == disconnect_handler_map_.end()) {
    return;
  }

  base::OnceClosure callback = std::move(it->second);
  disconnect_handler_map_.erase(it);
  std::move(callback).Run();
}

}  // namespace crosapi
