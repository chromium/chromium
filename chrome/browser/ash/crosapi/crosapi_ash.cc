// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/crosapi_ash.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/components/account_manager/account_manager_factory.h"
#include "base/dcheck_is_on.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_instance_registry.h"
#include "chrome/browser/apps/app_service/publishers/standalone_browser_extension_apps.h"
#include "chrome/browser/apps/app_service/publishers/standalone_browser_extension_apps_factory.h"
#include "chrome/browser/apps/app_service/publishers/web_apps_crosapi.h"
#include "chrome/browser/apps/app_service/publishers/web_apps_crosapi_factory.h"
#include "chrome/browser/apps/app_service/subscriber_crosapi.h"
#include "chrome/browser/apps/app_service/subscriber_crosapi_factory.h"
#include "chrome/browser/ash/crosapi/arc_ash.h"
#include "chrome/browser/ash/crosapi/authentication_ash.h"
#include "chrome/browser/ash/crosapi/automation_ash.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_service_host_ash.h"
#include "chrome/browser/ash/crosapi/browser_version_service_ash.h"
#include "chrome/browser/ash/crosapi/cert_database_ash.h"
#include "chrome/browser/ash/crosapi/chrome_app_kiosk_service_ash.h"
#include "chrome/browser/ash/crosapi/chrome_app_window_tracker_ash.h"
#include "chrome/browser/ash/crosapi/clipboard_ash.h"
#include "chrome/browser/ash/crosapi/clipboard_history_ash.h"
#include "chrome/browser/ash/crosapi/content_protection_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_dependency_registry.h"
#include "chrome/browser/ash/crosapi/desk_template_ash.h"
#include "chrome/browser/ash/crosapi/device_attributes_ash.h"
#include "chrome/browser/ash/crosapi/device_settings_ash.h"
#include "chrome/browser/ash/crosapi/dlp_ash.h"
#include "chrome/browser/ash/crosapi/download_controller_ash.h"
#include "chrome/browser/ash/crosapi/drive_integration_service_ash.h"
#include "chrome/browser/ash/crosapi/echo_private_ash.h"
#include "chrome/browser/ash/crosapi/extension_info_private_ash.h"
#include "chrome/browser/ash/crosapi/feedback_ash.h"
#include "chrome/browser/ash/crosapi/field_trial_service_ash.h"
#include "chrome/browser/ash/crosapi/file_manager_ash.h"
#include "chrome/browser/ash/crosapi/force_installed_tracker_ash.h"
#include "chrome/browser/ash/crosapi/geolocation_service_ash.h"
#include "chrome/browser/ash/crosapi/identity_manager_ash.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/browser/ash/crosapi/image_writer_ash.h"
#include "chrome/browser/ash/crosapi/keystore_service_ash.h"
#include "chrome/browser/ash/crosapi/kiosk_session_service_ash.h"
#include "chrome/browser/ash/crosapi/local_printer_ash.h"
#include "chrome/browser/ash/crosapi/login_ash.h"
#include "chrome/browser/ash/crosapi/login_screen_storage_ash.h"
#include "chrome/browser/ash/crosapi/login_state_ash.h"
#include "chrome/browser/ash/crosapi/message_center_ash.h"
#include "chrome/browser/ash/crosapi/metrics_reporting_ash.h"
#include "chrome/browser/ash/crosapi/native_theme_service_ash.h"
#include "chrome/browser/ash/crosapi/network_settings_service_ash.h"
#include "chrome/browser/ash/crosapi/networking_attributes_ash.h"
#include "chrome/browser/ash/crosapi/policy_service_ash.h"
#include "chrome/browser/ash/crosapi/power_ash.h"
#include "chrome/browser/ash/crosapi/prefs_ash.h"
#include "chrome/browser/ash/crosapi/remoting_ash.h"
#include "chrome/browser/ash/crosapi/resource_manager_ash.h"
#include "chrome/browser/ash/crosapi/screen_manager_ash.h"
#include "chrome/browser/ash/crosapi/search_provider_ash.h"
#include "chrome/browser/ash/crosapi/select_file_ash.h"
#include "chrome/browser/ash/crosapi/sharesheet_ash.h"
#include "chrome/browser/ash/crosapi/structured_metrics_service_ash.h"
#include "chrome/browser/ash/crosapi/system_display_ash.h"
#include "chrome/browser/ash/crosapi/task_manager_ash.h"
#include "chrome/browser/ash/crosapi/time_zone_service_ash.h"
#include "chrome/browser/ash/crosapi/url_handler_ash.h"
#include "chrome/browser/ash/crosapi/video_capture_device_factory_ash.h"
#include "chrome/browser/ash/crosapi/web_app_service_ash.h"
#include "chrome/browser/ash/crosapi/web_page_info_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/sync/sync_service_ash.h"
#include "chrome/browser/ash/sync/sync_service_factory_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/tts_ash.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/components/cdm_factory_daemon/cdm_factory_daemon_proxy_ash.h"
#include "chromeos/components/sensors/ash/sensor_hal_dispatcher.h"
#include "chromeos/crosapi/mojom/drive_integration_service.mojom.h"
#include "chromeos/crosapi/mojom/feedback.mojom.h"
#include "chromeos/crosapi/mojom/file_manager.mojom.h"
#include "chromeos/crosapi/mojom/image_writer.mojom.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/crosapi/mojom/select_file.mojom.h"
#include "chromeos/crosapi/mojom/task_manager.mojom.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/media_session_service.h"

#if BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#include "media/mojo/services/stable_video_decoder_factory_service.h"
#endif  // BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)

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
    if (ash::ProfileHelper::IsRegularProfile(profile))
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
      authentication_ash_(std::make_unique<AuthenticationAsh>()),
      automation_ash_(std::make_unique<AutomationAsh>()),
      browser_service_host_ash_(std::make_unique<BrowserServiceHostAsh>()),
      browser_version_service_ash_(std::make_unique<BrowserVersionServiceAsh>(
          g_browser_process->component_updater())),
      cert_database_ash_(std::make_unique<CertDatabaseAsh>()),
      chrome_app_window_tracker_ash_(
          std::make_unique<ChromeAppWindowTrackerAsh>()),
      clipboard_ash_(std::make_unique<ClipboardAsh>()),
      clipboard_history_ash_(std::make_unique<ClipboardHistoryAsh>()),
      content_protection_ash_(std::make_unique<ContentProtectionAsh>()),
      desk_template_ash_(std::make_unique<DeskTemplateAsh>()),
      device_attributes_ash_(std::make_unique<DeviceAttributesAsh>()),
      device_settings_ash_(std::make_unique<DeviceSettingsAsh>()),
      dlp_ash_(std::make_unique<DlpAsh>()),
      download_controller_ash_(std::make_unique<DownloadControllerAsh>()),
      drive_integration_service_ash_(
          std::make_unique<DriveIntegrationServiceAsh>()),
      echo_private_ash_(std::make_unique<EchoPrivateAsh>()),
      extension_info_private_ash_(std::make_unique<ExtensionInfoPrivateAsh>()),
      feedback_ash_(std::make_unique<FeedbackAsh>()),
      field_trial_service_ash_(std::make_unique<FieldTrialServiceAsh>()),
      file_manager_ash_(std::make_unique<FileManagerAsh>()),
      force_installed_tracker_ash_(
          std::make_unique<ForceInstalledTrackerAsh>()),
      geolocation_service_ash_(std::make_unique<GeolocationServiceAsh>()),
      identity_manager_ash_(std::make_unique<IdentityManagerAsh>()),
      idle_service_ash_(std::make_unique<IdleServiceAsh>()),
      image_writer_ash_(std::make_unique<ImageWriterAsh>()),
      keystore_service_ash_(std::make_unique<KeystoreServiceAsh>()),
      kiosk_session_service_ash_(std::make_unique<KioskSessionServiceAsh>()),
      chrome_app_kiosk_service_ash_(
          std::make_unique<ChromeAppKioskServiceAsh>()),
      local_printer_ash_(std::make_unique<LocalPrinterAsh>()),
      login_ash_(std::make_unique<LoginAsh>()),
      login_screen_storage_ash_(std::make_unique<LoginScreenStorageAsh>()),
      login_state_ash_(std::make_unique<LoginStateAsh>()),
      message_center_ash_(std::make_unique<MessageCenterAsh>()),
      metrics_reporting_ash_(registry->CreateMetricsReportingAsh(
          g_browser_process->metrics_service())),
      native_theme_service_ash_(std::make_unique<NativeThemeServiceAsh>()),
      networking_attributes_ash_(std::make_unique<NetworkingAttributesAsh>()),
      network_settings_service_ash_(std::make_unique<NetworkSettingsServiceAsh>(
          g_browser_process->local_state())),
      policy_service_ash_(std::make_unique<PolicyServiceAsh>()),
      power_ash_(std::make_unique<PowerAsh>()),
      prefs_ash_(
          std::make_unique<PrefsAsh>(g_browser_process->profile_manager(),
                                     g_browser_process->local_state())),
      remoting_ash_(std::make_unique<RemotingAsh>()),
      resource_manager_ash_(std::make_unique<ResourceManagerAsh>()),
      screen_manager_ash_(std::make_unique<ScreenManagerAsh>()),
      search_provider_ash_(std::make_unique<SearchProviderAsh>()),
      select_file_ash_(std::make_unique<SelectFileAsh>()),
      sharesheet_ash_(std::make_unique<SharesheetAsh>()),
#if BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)
      stable_video_decoder_factory_ash_(
          std::make_unique<media::StableVideoDecoderFactoryService>()),
#endif  // BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)
      structured_metrics_service_ash_(
          std::make_unique<StructuredMetricsServiceAsh>()),
      system_display_ash_(std::make_unique<SystemDisplayAsh>()),
      web_app_service_ash_(std::make_unique<WebAppServiceAsh>()),
      web_page_info_factory_ash_(std::make_unique<WebPageInfoFactoryAsh>()),
      task_manager_ash_(std::make_unique<TaskManagerAsh>()),
      time_zone_service_ash_(std::make_unique<TimeZoneServiceAsh>()),
      tts_ash_(std::make_unique<TtsAsh>(g_browser_process->profile_manager())),
      url_handler_ash_(std::make_unique<UrlHandlerAsh>()),
      video_capture_device_factory_ash_(
          std::make_unique<VideoCaptureDeviceFactoryAsh>()) {
  receiver_set_.set_disconnect_handler(base::BindRepeating(
      &CrosapiAsh::OnDisconnected, weak_factory_.GetWeakPtr()));
}

CrosapiAsh::~CrosapiAsh() {
  // Invoke all disconnect handlers.
  auto handlers = std::move(disconnect_handler_map_);
  for (auto& entry : handlers)
    std::move(entry.second).Run();
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

void CrosapiAsh::BindArc(mojo::PendingReceiver<mojom::Arc> receiver) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  arc_ash_->MaybeSetProfile(profile);
  arc_ash_->BindReceiver(std::move(receiver));
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

void CrosapiAsh::BindBrowserServiceHost(
    mojo::PendingReceiver<crosapi::mojom::BrowserServiceHost> receiver) {
  browser_service_host_ash_->BindReceiver(receiver_set_.current_context(),
                                          std::move(receiver));
}

void CrosapiAsh::BindBrowserCdmFactory(mojo::GenericPendingReceiver receiver) {
  if (auto r = receiver.As<chromeos::cdm::mojom::BrowserCdmFactory>())
    chromeos::CdmFactoryDaemonProxyAsh::Create(std::move(r));
}

void CrosapiAsh::BindBrowserVersionService(
    mojo::PendingReceiver<crosapi::mojom::BrowserVersionService> receiver) {
  browser_version_service_ash_->BindReceiver(std::move(receiver));
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

void CrosapiAsh::BindExtensionPublisher(
    mojo::PendingReceiver<mojom::AppPublisher> receiver) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  apps::StandaloneBrowserExtensionApps* extensions =
      apps::StandaloneBrowserExtensionAppsFactoryForExtension::GetForProfile(
          profile);
  extensions->RegisterCrosapiHost(std::move(receiver));
}

void CrosapiAsh::BindFieldTrialService(
    mojo::PendingReceiver<crosapi::mojom::FieldTrialService> receiver) {
  field_trial_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindFileManager(
    mojo::PendingReceiver<crosapi::mojom::FileManager> receiver) {
  file_manager_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindForceInstalledTracker(
    mojo::PendingReceiver<crosapi::mojom::ForceInstalledTracker> receiver) {
  force_installed_tracker_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindGeolocationService(
    mojo::PendingReceiver<crosapi::mojom::GeolocationService> receiver) {
  geolocation_service_ash_->BindReceiver(std::move(receiver));
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

void CrosapiAsh::BindKeystoreService(
    mojo::PendingReceiver<crosapi::mojom::KeystoreService> receiver) {
  keystore_service_ash_->BindReceiver(std::move(receiver));
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

void CrosapiAsh::BindMessageCenter(
    mojo::PendingReceiver<mojom::MessageCenter> receiver) {
  message_center_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindMetricsReporting(
    mojo::PendingReceiver<mojom::MetricsReporting> receiver) {
  metrics_reporting_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindNativeThemeService(
    mojo::PendingReceiver<crosapi::mojom::NativeThemeService> receiver) {
  native_theme_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindSelectFile(
    mojo::PendingReceiver<mojom::SelectFile> receiver) {
  select_file_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindSharesheet(
    mojo::PendingReceiver<mojom::Sharesheet> receiver) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  sharesheet_ash_->MaybeSetProfile(profile);
  sharesheet_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindScreenManager(
    mojo::PendingReceiver<mojom::ScreenManager> receiver) {
  screen_manager_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindHidManager(
    mojo::PendingReceiver<device::mojom::HidManager> receiver) {
  content::GetDeviceService().BindHidManager(std::move(receiver));
}

void CrosapiAsh::BindFeedback(mojo::PendingReceiver<mojom::Feedback> receiver) {
  feedback_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindMediaSessionController(
    mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
        receiver) {
  content::GetMediaSessionService().BindMediaControllerManager(
      std::move(receiver));
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

void CrosapiAsh::BindCertDatabase(
    mojo::PendingReceiver<mojom::CertDatabase> receiver) {
  cert_database_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindSearchControllerRegistry(
    mojo::PendingReceiver<mojom::SearchControllerRegistry> receiver) {
  search_provider_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindSyncService(
    mojo::PendingReceiver<mojom::SyncService> receiver) {
  ash::SyncServiceAsh* sync_service_ash =
      ash::SyncServiceFactoryAsh::GetForProfile(GetAshProfile());
  if (!sync_service_ash) {
    // |sync_service_ash| is not always available. In particular, sync can be
    // completely disabled via command line flags.
    return;
  }
  sync_service_ash->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindSystemDisplay(
    mojo::PendingReceiver<mojom::SystemDisplay> receiver) {
  system_display_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindTaskManager(
    mojo::PendingReceiver<mojom::TaskManager> receiver) {
  task_manager_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindTimeZoneService(
    mojo::PendingReceiver<mojom::TimeZoneService> receiver) {
  time_zone_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindTestController(
    mojo::PendingReceiver<mojom::TestController> receiver) {
  if (test_controller_)
    test_controller_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindKioskSessionService(
    mojo::PendingReceiver<mojom::KioskSessionService> receiver) {
  kiosk_session_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindChromeAppKioskService(
    mojo::PendingReceiver<mojom::ChromeAppKioskService> receiver) {
  chrome_app_kiosk_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindTts(mojo::PendingReceiver<mojom::Tts> receiver) {
  tts_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindWebAppService(
    mojo::PendingReceiver<mojom::WebAppService> receiver) {
  web_app_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindWebPageInfoFactory(
    mojo::PendingReceiver<mojom::WebPageInfoFactory> receiver) {
  web_page_info_factory_ash_->BindReceiver(std::move(receiver));
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

void CrosapiAsh::BindDeskTemplate(
    mojo::PendingReceiver<mojom::DeskTemplate> receiver) {
  desk_template_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindDeviceAttributes(
    mojo::PendingReceiver<mojom::DeviceAttributes> receiver) {
  device_attributes_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindDeviceSettingsService(
    mojo::PendingReceiver<mojom::DeviceSettingsService> receiver) {
  device_settings_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindDlp(mojo::PendingReceiver<mojom::Dlp> receiver) {
  dlp_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindDownloadController(
    mojo::PendingReceiver<mojom::DownloadController> receiver) {
  download_controller_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindNetworkingAttributes(
    mojo::PendingReceiver<mojom::NetworkingAttributes> receiver) {
  networking_attributes_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindSensorHalClient(
    mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient> remote) {
  chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterClient(
      std::move(remote));
}

void CrosapiAsh::BindStableVideoDecoderFactory(
    mojo::GenericPendingReceiver receiver) {
#if BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)
  if (auto r = receiver.As<media::stable::mojom::StableVideoDecoderFactory>())
    stable_video_decoder_factory_ash_->BindReceiver(std::move(r));
#endif  // BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)
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

void CrosapiAsh::BindRemoting(mojo::PendingReceiver<mojom::Remoting> receiver) {
  remoting_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindResourceManager(
    mojo::PendingReceiver<mojom::ResourceManager> receiver) {
  resource_manager_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindUrlHandler(
    mojo::PendingReceiver<mojom::UrlHandler> receiver) {
  url_handler_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindMachineLearningService(
    mojo::PendingReceiver<
        chromeos::machine_learning::mojom::MachineLearningService> receiver) {
  chromeos::machine_learning::ServiceConnection::GetInstance()
      ->BindMachineLearningService(std::move(receiver));
}

void CrosapiAsh::BindVideoCaptureDeviceFactory(
    mojo::PendingReceiver<mojom::VideoCaptureDeviceFactory> receiver) {
  video_capture_device_factory_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindWebAppPublisher(
    mojo::PendingReceiver<mojom::AppPublisher> receiver) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  apps::WebAppsCrosapi* web_apps =
      apps::WebAppsCrosapiFactory::GetForProfile(profile);
  web_apps->RegisterWebAppsCrosapiHost(std::move(receiver));
}
void CrosapiAsh::BindNetworkSettingsService(
    ::mojo::PendingReceiver<::crosapi::mojom::NetworkSettingsService>
        receiver) {
  network_settings_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindDriveIntegrationService(
    mojo::PendingReceiver<crosapi::mojom::DriveIntegrationService> receiver) {
  drive_integration_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindEchoPrivate(
    mojo::PendingReceiver<mojom::EchoPrivate> receiver) {
  echo_private_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindExtensionInfoPrivate(
    mojo::PendingReceiver<mojom::ExtensionInfoPrivate> receiver) {
  extension_info_private_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindStructuredMetricsService(
    mojo::PendingReceiver<crosapi::mojom::StructuredMetricsService> receiver) {
  structured_metrics_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::OnBrowserStartup(mojom::BrowserInfoPtr browser_info) {
  BrowserManager::Get()->set_browser_version(browser_info->browser_version);
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
