// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_CROSAPI_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_CROSAPI_ASH_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/crosapi/crosapi_id.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/task_manager.mojom.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace media {
class StableVideoDecoderFactoryService;
}  // namespace media

namespace crosapi {

class ArcAsh;
class AuthenticationAsh;
class AutomationAsh;
class BrowserServiceHostAsh;
class BrowserVersionServiceAsh;
class CertDatabaseAsh;
class ChromeAppWindowTrackerAsh;
class ClipboardAsh;
class ClipboardHistoryAsh;
class ContentProtectionAsh;
class DeskTemplateAsh;
class DeviceAttributesAsh;
class DeviceSettingsAsh;
class DlpAsh;
class DownloadControllerAsh;
class DriveIntegrationServiceAsh;
class FeedbackAsh;
class FieldTrialServiceAsh;
class FileManagerAsh;
class ForceInstalledTrackerAsh;
class GeolocationServiceAsh;
class IdentityManagerAsh;
class IdleServiceAsh;
class ImageWriterAsh;
class KeystoreServiceAsh;
class KioskSessionServiceAsh;
class LocalPrinterAsh;
class LoginAsh;
class LoginScreenStorageAsh;
class LoginStateAsh;
class MessageCenterAsh;
class MetricsReportingAsh;
class NativeThemeServiceAsh;
class NetworkingAttributesAsh;
class PolicyServiceAsh;
class PowerAsh;
class PrefsAsh;
class RemotingAsh;
class ResourceManagerAsh;
class ScreenManagerAsh;
class SearchProviderAsh;
class SelectFileAsh;
class SharesheetAsh;
class StructuredMetricsServiceAsh;
class SystemDisplayAsh;
class TaskManagerAsh;
class TimeZoneServiceAsh;
class TtsAsh;
class WebAppServiceAsh;
class WebPageInfoFactoryAsh;
class UrlHandlerAsh;
class VideoCaptureDeviceFactoryAsh;
class NetworkSettingsServiceAsh;

// Implementation of Crosapi in Ash. It provides a set of APIs that
// crosapi clients, such as lacros-chrome, can call into.
class CrosapiAsh : public mojom::Crosapi {
 public:
  CrosapiAsh();
  ~CrosapiAsh() override;

  // Abstract base class to support dependency injection for tests.
  class TestControllerReceiver {
   public:
    virtual void BindReceiver(
        mojo::PendingReceiver<mojom::TestController> receiver) = 0;
  };

  // Binds the given receiver to this instance.
  // |disconnected_handler| is called on the connection lost.
  void BindReceiver(mojo::PendingReceiver<mojom::Crosapi> pending_receiver,
                    CrosapiId crosapi_id,
                    base::OnceClosure disconnect_handler);

  // crosapi::mojom::Crosapi:
  void BindArc(mojo::PendingReceiver<mojom::Arc> receiver) override;
  void BindAuthentication(
      mojo::PendingReceiver<mojom::Authentication> receiver) override;
  void BindAutomationDeprecated(
      mojo::PendingReceiver<mojom::Automation> receiver) override;
  void BindAutomationFactory(
      mojo::PendingReceiver<mojom::AutomationFactory> receiver) override;
  void BindAccountManager(
      mojo::PendingReceiver<mojom::AccountManager> receiver) override;
  void BindAppServiceProxy(
      mojo::PendingReceiver<mojom::AppServiceProxy> receiver) override;
  void BindBrowserAppInstanceRegistry(
      mojo::PendingReceiver<mojom::BrowserAppInstanceRegistry> receiver)
      override;
  void BindBrowserServiceHost(
      mojo::PendingReceiver<mojom::BrowserServiceHost> receiver) override;
  void BindBrowserCdmFactory(mojo::GenericPendingReceiver receiver) override;
  void BindBrowserVersionService(
      mojo::PendingReceiver<mojom::BrowserVersionService> receiver) override;
  void BindCertDatabase(
      mojo::PendingReceiver<mojom::CertDatabase> receiver) override;
  void BindChromeAppPublisher(
      mojo::PendingReceiver<mojom::AppPublisher> receiver) override;
  void BindChromeAppWindowTracker(
      mojo::PendingReceiver<mojom::AppWindowTracker> receiver) override;
  void BindClipboard(mojo::PendingReceiver<mojom::Clipboard> receiver) override;
  void BindClipboardHistory(
      mojo::PendingReceiver<mojom::ClipboardHistory> receiver) override;
  void BindContentProtection(
      mojo::PendingReceiver<mojom::ContentProtection> receiver) override;
  void BindDeskTemplate(
      mojo::PendingReceiver<mojom::DeskTemplate> receiver) override;
  void BindDeviceAttributes(
      mojo::PendingReceiver<mojom::DeviceAttributes> receiver) override;
  void BindDeviceSettingsService(
      mojo::PendingReceiver<mojom::DeviceSettingsService> receiver) override;
  void BindDlp(mojo::PendingReceiver<mojom::Dlp> receiver) override;
  void BindHoldingSpaceService(
      mojo::PendingReceiver<mojom::HoldingSpaceService> receiver) override;
  void BindDownloadController(
      mojo::PendingReceiver<mojom::DownloadController> receiver) override;
  void BindDriveIntegrationService(
      mojo::PendingReceiver<mojom::DriveIntegrationService> receiver) override;
  void BindFieldTrialService(
      mojo::PendingReceiver<mojom::FieldTrialService> receiver) override;
  void BindFileManager(
      mojo::PendingReceiver<mojom::FileManager> receiver) override;
  void BindForceInstalledTracker(
      mojo::PendingReceiver<mojom::ForceInstalledTracker> receiver) override;
  void BindGeolocationService(
      mojo::PendingReceiver<mojom::GeolocationService> receiver) override;
  void BindIdentityManager(
      mojo::PendingReceiver<mojom::IdentityManager> receiver) override;
  void BindIdleService(
      mojo::PendingReceiver<mojom::IdleService> receiver) override;
  void BindImageWriter(
      mojo::PendingReceiver<mojom::ImageWriter> receiver) override;
  void BindKeystoreService(
      mojo::PendingReceiver<mojom::KeystoreService> receiver) override;
  void BindKioskSessionService(
      mojo::PendingReceiver<mojom::KioskSessionService> receiver) override;
  void BindLocalPrinter(
      mojo::PendingReceiver<mojom::LocalPrinter> receiver) override;
  void BindLogin(mojo::PendingReceiver<mojom::Login> receiver) override;
  void BindLoginScreenStorage(
      mojo::PendingReceiver<mojom::LoginScreenStorage> receiver) override;
  void BindLoginState(
      mojo::PendingReceiver<mojom::LoginState> receiver) override;
  void BindMessageCenter(
      mojo::PendingReceiver<mojom::MessageCenter> receiver) override;
  void BindMetricsReporting(
      mojo::PendingReceiver<mojom::MetricsReporting> receiver) override;
  void BindNativeThemeService(
      mojo::PendingReceiver<mojom::NativeThemeService> receiver) override;
  void BindNetworkingAttributes(
      mojo::PendingReceiver<mojom::NetworkingAttributes> receiver) override;
  void BindPolicyService(
      mojo::PendingReceiver<mojom::PolicyService> receiver) override;
  void BindPower(mojo::PendingReceiver<mojom::Power> receiver) override;
  void BindPrefs(mojo::PendingReceiver<mojom::Prefs> receiver) override;
  void BindRemoting(mojo::PendingReceiver<mojom::Remoting> receiver) override;
  void BindResourceManager(
      mojo::PendingReceiver<mojom::ResourceManager> receiver) override;
  void BindScreenManager(
      mojo::PendingReceiver<mojom::ScreenManager> receiver) override;
  void BindSearchControllerRegistry(
      mojo::PendingReceiver<mojom::SearchControllerRegistry> receiver) override;
  void BindSelectFile(
      mojo::PendingReceiver<mojom::SelectFile> receiver) override;
  void BindSensorHalClient(
      mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient> remote)
      override;
  void BindSharesheet(
      mojo::PendingReceiver<mojom::Sharesheet> receiver) override;
  void BindStableVideoDecoderFactory(
      mojo::GenericPendingReceiver receiver) override;
  void BindHidManager(
      mojo::PendingReceiver<device::mojom::HidManager> receiver) override;
  void BindFeedback(mojo::PendingReceiver<mojom::Feedback> receiver) override;
  void OnBrowserStartup(mojom::BrowserInfoPtr browser_info) override;
  void BindMediaSessionController(
      mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
          receiver) override;
  void BindMediaSessionAudioFocus(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManager> receiver)
      override;
  void BindMediaSessionAudioFocusDebug(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManagerDebug>
          receiver) override;
  void BindSyncService(
      mojo::PendingReceiver<mojom::SyncService> receiver) override;
  void BindSystemDisplay(
      mojo::PendingReceiver<mojom::SystemDisplay> receiver) override;
  void BindWebAppService(
      mojo::PendingReceiver<mojom::WebAppService> receiver) override;
  void BindWebPageInfoFactory(
      mojo::PendingReceiver<mojom::WebPageInfoFactory> receiver) override;
  void BindTaskManager(
      mojo::PendingReceiver<mojom::TaskManager> receiver) override;
  void BindTestController(
      mojo::PendingReceiver<mojom::TestController> receiver) override;
  void BindTimeZoneService(
      mojo::PendingReceiver<mojom::TimeZoneService> receiver) override;
  void BindTts(mojo::PendingReceiver<mojom::Tts> receiver) override;
  void BindUrlHandler(
      mojo::PendingReceiver<mojom::UrlHandler> receiver) override;
  void BindMachineLearningService(
      mojo::PendingReceiver<
          chromeos::machine_learning::mojom::MachineLearningService> receiver)
      override;
  void BindVideoCaptureDeviceFactory(
      mojo::PendingReceiver<mojom::VideoCaptureDeviceFactory> receiver)
      override;
  void BindWebAppPublisher(
      mojo::PendingReceiver<mojom::AppPublisher> receiver) override;
  void BindNetworkSettingsService(
      ::mojo::PendingReceiver<::crosapi::mojom::NetworkSettingsService>
          receiver) override;
  void BindStructuredMetricsService(
      ::mojo::PendingReceiver<::crosapi::mojom::StructuredMetricsService>
          receiver) override;

  BrowserServiceHostAsh* browser_service_host_ash() {
    return browser_service_host_ash_.get();
  }

  AutomationAsh* automation_ash() { return automation_ash_.get(); }

  DeskTemplateAsh* desk_template_ash() { return desk_template_ash_.get(); }

  DownloadControllerAsh* download_controller_ash() {
    return download_controller_ash_.get();
  }

  ForceInstalledTrackerAsh* force_installed_tracker_ash() {
    return force_installed_tracker_ash_.get();
  }

  KioskSessionServiceAsh* kiosk_session_service() {
    return kiosk_session_service_ash_.get();
  }

  SearchProviderAsh* search_provider_ash() {
    return search_provider_ash_.get();
  }

  WebAppServiceAsh* web_app_service_ash() { return web_app_service_ash_.get(); }

  WebPageInfoFactoryAsh* web_page_info_factory_ash() {
    return web_page_info_factory_ash_.get();
  }

  ImageWriterAsh* image_writer_ash() { return image_writer_ash_.get(); }

  LocalPrinterAsh* local_printer_ash() { return local_printer_ash_.get(); }

  NetworkingAttributesAsh* networking_attributes_ash() {
    return networking_attributes_ash_.get();
  }

  TaskManagerAsh* task_manager_ash() { return task_manager_ash_.get(); }

  TtsAsh* tts_ash() { return tts_ash_.get(); }

  KeystoreServiceAsh* keystore_service_ash() {
    return keystore_service_ash_.get();
  }

  CertDatabaseAsh* cert_database_ash() { return cert_database_ash_.get(); }

  LoginAsh* login_ash() { return login_ash_.get(); }

  LoginScreenStorageAsh* login_screen_storage_ash() {
    return login_screen_storage_ash_.get();
  }

  LoginStateAsh* login_state_ash() { return login_state_ash_.get(); }

  SharesheetAsh* sharesheet_ash() { return sharesheet_ash_.get(); }

  StructuredMetricsServiceAsh* structured_metrics_service_ash() {
    return structured_metrics_service_ash_.get();
  }

  ScreenManagerAsh* screen_manager_ash() { return screen_manager_ash_.get(); }

  // Caller is responsible for ensuring that the pointer stays valid.
  void SetTestControllerForTesting(TestControllerReceiver* test_controller);

 private:
  // Called when a connection is lost.
  void OnDisconnected();

  std::unique_ptr<ArcAsh> arc_ash_;
  std::unique_ptr<AuthenticationAsh> authentication_ash_;
  std::unique_ptr<AutomationAsh> automation_ash_;
  std::unique_ptr<BrowserServiceHostAsh> browser_service_host_ash_;
  std::unique_ptr<BrowserVersionServiceAsh> browser_version_service_ash_;
  std::unique_ptr<CertDatabaseAsh> cert_database_ash_;
  std::unique_ptr<ChromeAppWindowTrackerAsh> chrome_app_window_tracker_ash_;
  std::unique_ptr<ClipboardAsh> clipboard_ash_;
  std::unique_ptr<ClipboardHistoryAsh> clipboard_history_ash_;
  std::unique_ptr<ContentProtectionAsh> content_protection_ash_;
  std::unique_ptr<DeskTemplateAsh> desk_template_ash_;
  std::unique_ptr<DeviceAttributesAsh> device_attributes_ash_;
  std::unique_ptr<DeviceSettingsAsh> device_settings_ash_;
  std::unique_ptr<DlpAsh> dlp_ash_;
  std::unique_ptr<DownloadControllerAsh> download_controller_ash_;
  std::unique_ptr<DriveIntegrationServiceAsh> drive_integration_service_ash_;
  std::unique_ptr<FeedbackAsh> feedback_ash_;
  std::unique_ptr<FieldTrialServiceAsh> field_trial_service_ash_;
  std::unique_ptr<FileManagerAsh> file_manager_ash_;
  std::unique_ptr<ForceInstalledTrackerAsh> force_installed_tracker_ash_;
  std::unique_ptr<GeolocationServiceAsh> geolocation_service_ash_;
  std::unique_ptr<IdentityManagerAsh> identity_manager_ash_;
  std::unique_ptr<IdleServiceAsh> idle_service_ash_;
  std::unique_ptr<ImageWriterAsh> image_writer_ash_;
  std::unique_ptr<KeystoreServiceAsh> keystore_service_ash_;
  std::unique_ptr<KioskSessionServiceAsh> kiosk_session_service_ash_;
  std::unique_ptr<LocalPrinterAsh> local_printer_ash_;
  std::unique_ptr<LoginAsh> login_ash_;
  std::unique_ptr<LoginScreenStorageAsh> login_screen_storage_ash_;
  std::unique_ptr<LoginStateAsh> login_state_ash_;
  std::unique_ptr<MessageCenterAsh> message_center_ash_;
  std::unique_ptr<MetricsReportingAsh> metrics_reporting_ash_;
  std::unique_ptr<NativeThemeServiceAsh> native_theme_service_ash_;
  std::unique_ptr<NetworkingAttributesAsh> networking_attributes_ash_;
  std::unique_ptr<NetworkSettingsServiceAsh> network_settings_service_ash_;
  std::unique_ptr<PolicyServiceAsh> policy_service_ash_;
  std::unique_ptr<PowerAsh> power_ash_;
  std::unique_ptr<PrefsAsh> prefs_ash_;
  std::unique_ptr<RemotingAsh> remoting_ash_;
  std::unique_ptr<ResourceManagerAsh> resource_manager_ash_;
  std::unique_ptr<ScreenManagerAsh> screen_manager_ash_;
  std::unique_ptr<SearchProviderAsh> search_provider_ash_;
  std::unique_ptr<SelectFileAsh> select_file_ash_;
  std::unique_ptr<SharesheetAsh> sharesheet_ash_;
  std::unique_ptr<media::StableVideoDecoderFactoryService>
      stable_video_decoder_factory_ash_;
  std::unique_ptr<StructuredMetricsServiceAsh> structured_metrics_service_ash_;
  std::unique_ptr<SystemDisplayAsh> system_display_ash_;
  std::unique_ptr<WebAppServiceAsh> web_app_service_ash_;
  std::unique_ptr<WebPageInfoFactoryAsh> web_page_info_factory_ash_;
  std::unique_ptr<TaskManagerAsh> task_manager_ash_;
  std::unique_ptr<TimeZoneServiceAsh> time_zone_service_ash_;
  std::unique_ptr<TtsAsh> tts_ash_;
  std::unique_ptr<UrlHandlerAsh> url_handler_ash_;
  std::unique_ptr<VideoCaptureDeviceFactoryAsh>
      video_capture_device_factory_ash_;

  // Only set in the test ash chrome binary. In production ash this is always
  // nullptr.
  TestControllerReceiver* test_controller_ = nullptr;

  mojo::ReceiverSet<mojom::Crosapi, CrosapiId> receiver_set_;
  std::map<mojo::ReceiverId, base::OnceClosure> disconnect_handler_map_;

  base::WeakPtrFactory<CrosapiAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_CROSAPI_ASH_H_
