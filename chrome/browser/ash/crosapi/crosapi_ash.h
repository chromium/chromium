// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_CROSAPI_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_CROSAPI_ASH_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/crosapi/crosapi_id.h"
#include "chrome/browser/ash/crosapi/extension_printer_service_ash.h"
#include "chrome/browser/ash/smart_reader/smart_reader_manager_ash.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom-forward.h"
#include "chromeos/crosapi/mojom/emoji_picker.mojom-forward.h"
#include "chromeos/crosapi/mojom/firewall_hole.mojom.h"
#include "chromeos/crosapi/mojom/lacros_shelf_item_tracker.mojom.h"
#include "chromeos/crosapi/mojom/magic_boost.mojom-forward.h"
#include "chromeos/crosapi/mojom/mahi.mojom-forward.h"
#include "chromeos/crosapi/mojom/print_preview_cros.mojom-forward.h"
#include "chromeos/crosapi/mojom/task_manager.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "media/gpu/buildflags.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "printing/buildflags/buildflags.h"

namespace apps {
class DigitalGoodsFactoryAsh;
}

namespace ash {
class DiagnosticsServiceAsh;
class MagicBoostControllerAsh;
class ProbeServiceAsh;
class SmartReaderManagerAsh;
class TelemetryDiagnosticsRoutineServiceAsh;
class TelemetryEventServiceAsh;
class TelemetryManagementServiceAsh;
class VideoConferenceManagerAsh;

namespace auth {
class InSessionAuth;
}  // namespace auth

namespace printing {
class PrintPreviewWebcontentsAdapterAsh;
}  // namespace printing

}  // namespace ash

namespace crosapi {

class ArcAsh;
class AudioServiceAsh;
class AutomationAsh;
class BrowserServiceHostAsh;
class BrowserVersionServiceAsh;
class CecPrivateAsh;
class CertDatabaseAsh;
class CertProvisioningAsh;
class ChapsServiceAsh;
class ChromeAppKioskServiceAsh;
class ChromeAppWindowTrackerAsh;
class ClipboardAsh;
class ClipboardHistoryAsh;
class ContentProtectionAsh;
class CrosapiDependencyRegistry;
class DebugInterfaceRegistererAsh;
class DeskAsh;
class DeskProfilesAsh;
class DeskTemplateAsh;
class DeviceAttributesAsh;
class DeviceLocalAccountExtensionServiceAsh;
class DeviceOAuth2TokenServiceAsh;
class DeviceSettingsAsh;
class DlpAsh;
class DocumentScanAsh;
class DownloadControllerAsh;
class DownloadStatusUpdaterAsh;
class DriveIntegrationServiceAsh;
class EchoPrivateAsh;
class EmbeddedAccessibilityHelperClientAsh;
class EmojiPickerAsh;
class ExtensionInfoPrivateAsh;
class ExtensionPrinterServiceAsh;
class EyeDropperAsh;
class FieldTrialServiceAsh;
class FileChangeServiceBridgeAsh;
class FileManagerAsh;
class FileSystemAccessCloudIdentifierProviderAsh;
class FileSystemProviderServiceAsh;
class ForceInstalledTrackerAsh;
class FullRestoreAsh;
class FullscreenControllerAsh;
class GeolocationServiceAsh;
class IdentityManagerAsh;
class IdleServiceAsh;
class ImageWriterAsh;
class InputMethodsAsh;
class KerberosInBrowserAsh;
class KeystoreServiceAsh;
class KioskSessionServiceAsh;
class LacrosShelfItemTracker;
class LocalPrinterAsh;
class LoginAsh;
class LoginScreenStorageAsh;
class LoginStateAsh;
class MediaAppAsh;
class MediaUIAsh;
class MessageCenterAsh;
class MetricsAsh;
class MetricsReportingAsh;
class MultiCaptureServiceAsh;
class NativeThemeServiceAsh;
class NetworkSettingsServiceAsh;
class NetworkingAttributesAsh;
class NetworkingPrivateAsh;
class OneDriveNotificationServiceAsh;
class OneDriveIntegrationServiceAsh;
class PasskeyAuthenticator;
class ParentAccessAsh;
class PaymentAppInstanceAsh;
class PolicyServiceAsh;
class PowerAsh;
class PrefsAsh;
class NonclosableAppToastServiceAsh;
#if BUILDFLAG(USE_CUPS)
class PrintingMetricsAsh;
#endif  // BUILDFLAG(USE_CUPS)
class RemotingAsh;
class ResourceManagerAsh;
class ScreenAIDownloaderAsh;
class ScreenManagerAsh;
class SearchProviderAsh;
class SharesheetAsh;
class SpeechRecognitionAsh;
class StructuredMetricsServiceAsh;
class SuggestionServiceAsh;
class TaskManagerAsh;
class TimeZoneServiceAsh;
class TtsAsh;
class VpnServiceAsh;
class WallpaperAsh;
class WebAppServiceAsh;
class WebKioskServiceAsh;
class WebPageInfoFactoryAsh;
class VirtualKeyboardAsh;
class VolumeManagerAsh;
class VpnExtensionObserverAsh;

// Implementation of Crosapi in Ash. It provides a set of APIs that
// crosapi clients, such as lacros-chrome, can call into.
class CrosapiAsh : public mojom::Crosapi {
 public:
  // Abstract base class to support dependency injection for tests.
  class TestControllerReceiver {
   public:
    virtual ~TestControllerReceiver();
    virtual void BindReceiver(
        mojo::PendingReceiver<mojom::TestController> receiver) = 0;
  };

  explicit CrosapiAsh(CrosapiDependencyRegistry* registry);
  ~CrosapiAsh() override;

  // Binds the given receiver to this instance.
  // |disconnected_handler| is called on the connection lost.
  void BindReceiver(mojo::PendingReceiver<mojom::Crosapi> pending_receiver,
                    CrosapiId crosapi_id,
                    base::OnceClosure disconnect_handler);

  // crosapi::mojom::Crosapi:
  void BindAccountManager(
      mojo::PendingReceiver<mojom::AccountManager> receiver) override;
  void BindAppServiceProxy(
      mojo::PendingReceiver<mojom::AppServiceProxy> receiver) override;
  void BindArc(mojo::PendingReceiver<mojom::Arc> receiver) override;
  void BindAudioService(
      mojo::PendingReceiver<mojom::AudioService> receiver) override;
  void BindAutomationDeprecated(
      mojo::PendingReceiver<mojom::Automation> receiver) override;
  void BindAutomationFactory(
      mojo::PendingReceiver<mojom::AutomationFactory> receiver) override;
  void BindBrowserAppInstanceRegistry(
      mojo::PendingReceiver<mojom::BrowserAppInstanceRegistry> receiver)
      override;
  void BindBrowserCdmFactory(mojo::GenericPendingReceiver receiver) override;
  void BindBrowserServiceHost(
      mojo::PendingReceiver<mojom::BrowserServiceHost> receiver) override;
  void BindBrowserShortcutPublisher(
      mojo::PendingReceiver<mojom::AppShortcutPublisher> receiver) override;
  void BindBrowserVersionService(
      mojo::PendingReceiver<mojom::BrowserVersionService> receiver) override;
  void BindCecPrivate(
      mojo::PendingReceiver<mojom::CecPrivate> receiver) override;
  void BindCertDatabase(
      mojo::PendingReceiver<mojom::CertDatabase> receiver) override;
  void BindCertProvisioning(
      mojo::PendingReceiver<mojom::CertProvisioning> receiver) override;
  void BindCfmServiceContext(
      mojo::PendingReceiver<chromeos::cfm::mojom::CfmServiceContext> receiver)
      override;
  void BindChapsService(
      mojo::PendingReceiver<mojom::ChapsService> receiver) override;
  void BindChromeAppKioskService(
      mojo::PendingReceiver<mojom::ChromeAppKioskService> receiver) override;
  void BindChromeAppPublisher(
      mojo::PendingReceiver<mojom::AppPublisher> receiver) override;
  void BindChromeAppWindowTracker(
      mojo::PendingReceiver<mojom::AppWindowTracker> receiver) override;
  void BindClipboard(mojo::PendingReceiver<mojom::Clipboard> receiver) override;
  void BindClipboardHistory(
      mojo::PendingReceiver<mojom::ClipboardHistory> receiver) override;
  void BindContentProtection(
      mojo::PendingReceiver<mojom::ContentProtection> receiver) override;
  void BindCrosDisplayConfigController(
      mojo::PendingReceiver<mojom::CrosDisplayConfigController> receiver)
      override;
  void BindDebugInterfaceRegisterer(
      mojo::PendingReceiver<mojom::DebugInterfaceRegisterer> receiver) override;
  void BindDesk(mojo::PendingReceiver<mojom::Desk> receiver) override;
  void BindDeskProfileObserver(
      mojo::PendingReceiver<mojom::DeskProfileObserver> receiver) override;
  void BindDeskTemplate(
      mojo::PendingReceiver<mojom::DeskTemplate> receiver) override;
  void BindDeviceAttributes(
      mojo::PendingReceiver<mojom::DeviceAttributes> receiver) override;
  void BindDeviceLocalAccountExtensionService(
      mojo::PendingReceiver<mojom::DeviceLocalAccountExtensionService> receiver)
      override;
  void BindDeviceOAuth2TokenService(
      mojo::PendingReceiver<mojom::DeviceOAuth2TokenService> receiver) override;
  void BindDeviceSettingsService(
      mojo::PendingReceiver<mojom::DeviceSettingsService> receiver) override;
  void BindDiagnosticsService(
      mojo::PendingReceiver<mojom::DiagnosticsService> receiver) override;
  void BindDigitalGoodsFactory(
      mojo::PendingReceiver<mojom::DigitalGoodsFactory> receiver) override;
  void BindDlp(mojo::PendingReceiver<mojom::Dlp> receiver) override;
  void BindDocumentScan(
      mojo::PendingReceiver<mojom::DocumentScan> receiver) override;
  void BindDownloadController(
      mojo::PendingReceiver<mojom::DownloadController> receiver) override;
  void BindDownloadStatusUpdater(
      mojo::PendingReceiver<mojom::DownloadStatusUpdater> receiver) override;
  void BindDriveIntegrationService(
      mojo::PendingReceiver<mojom::DriveIntegrationService> receiver) override;
  void BindEchoPrivate(
      mojo::PendingReceiver<mojom::EchoPrivate> receiver) override;
  void BindEditorPanelManager(
      mojo::PendingReceiver<mojom::EditorPanelManager> receiver) override;
  void BindEmbeddedAccessibilityHelperClientFactory(
      mojo::PendingReceiver<
          ::crosapi::mojom::EmbeddedAccessibilityHelperClientFactory> receiver)
      override;
  void BindEmojiPicker(
      mojo::PendingReceiver<mojom::EmojiPicker> receiver) override;
  void BindExtensionInfoPrivate(
      mojo::PendingReceiver<mojom::ExtensionInfoPrivate> receiver) override;
  void BindExtensionPrinterService(
      mojo::PendingReceiver<mojom::ExtensionPrinterService> receiver) override;
  void BindExtensionPublisher(
      mojo::PendingReceiver<mojom::AppPublisher> receiver) override;
  void BindEyeDropper(
      mojo::PendingReceiver<mojom::EyeDropper> receiver) override;
  void BindFieldTrialService(
      mojo::PendingReceiver<mojom::FieldTrialService> receiver) override;
  void BindFileChangeServiceBridge(
      mojo::PendingReceiver<mojom::FileChangeServiceBridge> receiver) override;
  void BindFileManager(
      mojo::PendingReceiver<mojom::FileManager> receiver) override;
  void BindFileSystemAccessCloudIdentifierProvider(
      mojo::PendingReceiver<mojom::FileSystemAccessCloudIdentifierProvider>
          receiver) override;
  void BindFileSystemProviderService(
      mojo::PendingReceiver<mojom::FileSystemProviderService> receiver)
      override;
  void BindForceInstalledTracker(
      mojo::PendingReceiver<mojom::ForceInstalledTracker> receiver) override;
  void BindFullRestore(
      mojo::PendingReceiver<mojom::FullRestore> receiver) override;
  void BindFullscreenController(
      mojo::PendingReceiver<mojom::FullscreenController> receiver) override;
  void BindGeolocationService(
      mojo::PendingReceiver<mojom::GeolocationService> receiver) override;
  void BindHidManager(
      mojo::PendingReceiver<device::mojom::HidManager> receiver) override;
  void BindIdentityManager(
      mojo::PendingReceiver<mojom::IdentityManager> receiver) override;
  void BindIdleService(
      mojo::PendingReceiver<mojom::IdleService> receiver) override;
  void BindImageWriter(
      mojo::PendingReceiver<mojom::ImageWriter> receiver) override;
  void BindInputMethods(
      mojo::PendingReceiver<mojom::InputMethods> receiver) override;
  void BindInSessionAuth(
      mojo::PendingReceiver<chromeos::auth::mojom::InSessionAuth> receiver)
      override;
  void BindKerberosInBrowser(
      mojo::PendingReceiver<mojom::KerberosInBrowser> receiver) override;
  void BindKeystoreService(
      mojo::PendingReceiver<mojom::KeystoreService> receiver) override;
  void BindKioskSessionService(
      mojo::PendingReceiver<mojom::KioskSessionService> receiver) override;
  void BindLacrosShelfItemTracker(
      mojo::PendingReceiver<mojom::LacrosShelfItemTracker> receiver) override;
  void BindLacrosAppPublisher(
      mojo::PendingReceiver<mojom::AppPublisher> receiver) override;
  void BindLocalPrinter(
      mojo::PendingReceiver<mojom::LocalPrinter> receiver) override;
  void BindLogin(mojo::PendingReceiver<mojom::Login> receiver) override;
  void BindLoginScreenStorage(
      mojo::PendingReceiver<mojom::LoginScreenStorage> receiver) override;
  void BindLoginState(
      mojo::PendingReceiver<mojom::LoginState> receiver) override;
  void BindMachineLearningService(
      mojo::PendingReceiver<
          chromeos::machine_learning::mojom::MachineLearningService> receiver)
      override;
  void BindMagicBoostController(
      mojo::PendingReceiver<mojom::MagicBoostController> receiver) override;
  void BindMahiBrowserDelegate(
      mojo::PendingReceiver<mojom::MahiBrowserDelegate> receiver) override;
  void BindMediaApp(mojo::PendingRemote<mojom::MediaApp> remote) override;
  void BindMediaUI(mojo::PendingReceiver<mojom::MediaUI> receiver) override;
  void BindMediaSessionAudioFocus(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManager> receiver)
      override;
  void BindMediaSessionAudioFocusDebug(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManagerDebug>
          receiver) override;
  void BindMediaSessionController(
      mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
          receiver) override;
  void BindMessageCenter(
      mojo::PendingReceiver<mojom::MessageCenter> receiver) override;
  void BindMetrics(mojo::PendingReceiver<mojom::Metrics> receiver) override;
  void BindMetricsReporting(
      mojo::PendingReceiver<mojom::MetricsReporting> receiver) override;
  void BindMultiCaptureService(
      mojo::PendingReceiver<mojom::MultiCaptureService> receiver) override;
  void BindNativeThemeService(
      mojo::PendingReceiver<mojom::NativeThemeService> receiver) override;
  void BindNetworkChange(
      mojo::PendingReceiver<mojom::NetworkChange> receiver) override;
  void BindNetworkSettingsService(
      ::mojo::PendingReceiver<::crosapi::mojom::NetworkSettingsService>
          receiver) override;
  void BindNetworkingAttributes(
      mojo::PendingReceiver<mojom::NetworkingAttributes> receiver) override;
  void BindNetworkingPrivate(
      mojo::PendingReceiver<mojom::NetworkingPrivate> receiver) override;
  void BindOneDriveNotificationService(
      mojo::PendingReceiver<mojom::OneDriveNotificationService> receiver)
      override;
  void BindOneDriveIntegrationService(
      mojo::PendingReceiver<mojom::OneDriveIntegrationService> receiver)
      override;
  void BindPasskeyAuthenticatorDeprecated(
      mojo::PendingReceiver<mojom::PasskeyAuthenticator> receiver) override;
  void BindParentAccess(
      mojo::PendingReceiver<mojom::ParentAccess> receiver) override;
  void BindPaymentAppInstance(
      mojo::PendingReceiver<chromeos::payments::mojom::PaymentAppInstance>
          receiver) override;
  void BindPolicyService(
      mojo::PendingReceiver<mojom::PolicyService> receiver) override;
  void BindPower(mojo::PendingReceiver<mojom::Power> receiver) override;
  void BindPrefs(mojo::PendingReceiver<mojom::Prefs> receiver) override;
  void BindPrintPreviewCrosDelegate(
      mojo::PendingReceiver<mojom::PrintPreviewCrosDelegate> receiver) override;
  void BindNonclosableAppToastService(
      mojo::PendingReceiver<mojom::NonclosableAppToastService> receiver)
      override;
  void BindPrintingMetrics(
      mojo::PendingReceiver<mojom::PrintingMetrics> receiver) override;
  void BindRemoteAppsLacrosBridge(
      mojo::PendingReceiver<
          chromeos::remote_apps::mojom::RemoteAppsLacrosBridge> receiver)
      override;
  void BindRemoting(mojo::PendingReceiver<mojom::Remoting> receiver) override;
  void BindResourceManager(
      mojo::PendingReceiver<mojom::ResourceManager> receiver) override;
  void BindScreenAIDownloader(
      mojo::PendingReceiver<mojom::ScreenAIDownloader> receiver) override;
  void BindScreenManager(
      mojo::PendingReceiver<mojom::ScreenManager> receiver) override;
  void BindSearchControllerFactory(
      mojo::PendingRemote<mojom::SearchControllerFactory> remote) override;
  void BindSearchControllerRegistry(
      mojo::PendingReceiver<mojom::SearchControllerRegistry> receiver) override;
  void BindSensorHalClient(
      mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient> remote)
      override;
  void BindSharesheet(
      mojo::PendingReceiver<mojom::Sharesheet> receiver) override;
  void BindSmartReaderClient(
      mojo::PendingRemote<mojom::SmartReaderClient> remote) override;
  void BindSpeechRecognition(
      mojo::PendingReceiver<mojom::SpeechRecognition> receiver) override;
  void BindStableVideoDecoderFactory(
      mojo::GenericPendingReceiver receiver) override;
  void BindStructuredMetricsService(
      ::mojo::PendingReceiver<::crosapi::mojom::StructuredMetricsService>
          receiver) override;
  void BindSuggestionService(
      mojo::PendingReceiver<mojom::SuggestionService> receiver) override;
  void BindSyncService(
      mojo::PendingReceiver<mojom::SyncService> receiver) override;
  void BindTaskManager(
      mojo::PendingReceiver<mojom::TaskManager> receiver) override;
  void BindTelemetryDiagnosticRoutinesService(
      mojo::PendingReceiver<mojom::TelemetryDiagnosticRoutinesService> receiver)
      override;
  void BindTelemetryEventService(
      mojo::PendingReceiver<mojom::TelemetryEventService> receiver) override;
  void BindTelemetryManagementService(
      mojo::PendingReceiver<mojom::TelemetryManagementService> receiver)
      override;
  void BindTelemetryProbeService(
      mojo::PendingReceiver<mojom::TelemetryProbeService> receiver) override;
  void BindTestController(
      mojo::PendingReceiver<mojom::TestController> receiver) override;
  void BindTimeZoneService(
      mojo::PendingReceiver<mojom::TimeZoneService> receiver) override;
  void BindTrustedVaultBackend(
      mojo::PendingReceiver<mojom::TrustedVaultBackend> receiver) override;
  void BindTrustedVaultBackendService(
      mojo::PendingReceiver<mojom::TrustedVaultBackendService> receiver)
      override;
  void BindTts(mojo::PendingReceiver<mojom::Tts> receiver) override;
  void BindUrlHandler(
      mojo::PendingReceiver<mojom::UrlHandler> receiver) override;
  void BindVideoCaptureDeviceFactory(
      mojo::PendingReceiver<mojom::VideoCaptureDeviceFactory> receiver)
      override;
  void BindVideoConferenceManager(
      mojo::PendingReceiver<mojom::VideoConferenceManager> receiver) override;
  void BindVirtualKeyboard(
      mojo::PendingReceiver<mojom::VirtualKeyboard> receiver) override;
  void BindVolumeManager(
      mojo::PendingReceiver<mojom::VolumeManager> receiver) override;
  void BindVpnExtensionObserver(
      mojo::PendingReceiver<crosapi::mojom::VpnExtensionObserver> receiver)
      override;
  void BindVpnService(
      mojo::PendingReceiver<mojom::VpnService> receiver) override;
  void BindWallpaper(
      mojo::PendingReceiver<crosapi::mojom::Wallpaper> receiver) override;
  void BindWebAppPublisher(
      mojo::PendingReceiver<mojom::AppPublisher> receiver) override;
  void BindWebAppService(
      mojo::PendingReceiver<mojom::WebAppService> receiver) override;
  void BindWebKioskService(
      mojo::PendingReceiver<mojom::WebKioskService> receiver) override;
  void BindWebPageInfoFactory(
      mojo::PendingReceiver<mojom::WebPageInfoFactory> receiver) override;
  void BindGuestOsSkForwarderFactory(
      mojo::PendingReceiver<mojom::GuestOsSkForwarderFactory> receiver)
      override;
  void OnBrowserStartup(mojom::BrowserInfoPtr browser_info) override;
  void REMOVED_29(
      mojo::PendingReceiver<mojom::SystemDisplayDeprecated> receiver) override;
  void REMOVED_105(mojo::PendingReceiver<mojom::FirewallHoleServiceDeprecated>
                       receiver) override;
  void REMOVED_62(
      mojo::PendingReceiver<mojom::AuthenticationDeprecated> receiver) override;

  AutomationAsh* automation_ash() { return automation_ash_.get(); }

  BrowserServiceHostAsh* browser_service_host_ash() {
    return browser_service_host_ash_.get();
  }

  CecPrivateAsh* cec_private_ash() { return cec_private_ash_.get(); }

  CertDatabaseAsh* cert_database_ash() { return cert_database_ash_.get(); }

  CertProvisioningAsh* cert_provisioning_ash() {
    return cert_provisioning_ash_.get();
  }

  ChapsServiceAsh* chaps_service_ash() { return chaps_service_ash_.get(); }

  ChromeAppKioskServiceAsh* chrome_app_kiosk_service() {
    return chrome_app_kiosk_service_ash_.get();
  }

  DebugInterfaceRegistererAsh* debug_interface_registerer_ash() {
    return debug_interface_registerer_ash_.get();
  }

  DeskAsh* desk_ash() { return desk_ash_.get(); }

  DeskProfilesAsh* desk_profiles_ash() { return desk_profiles_ash_.get(); }

  DeskTemplateAsh* desk_template_ash() { return desk_template_ash_.get(); }

  DeviceAttributesAsh* device_attributes_ash() {
    return device_attributes_ash_.get();
  }

  DeviceLocalAccountExtensionServiceAsh*
  device_local_account_extension_service() {
    return device_local_account_extension_service_ash_.get();
  }

  DocumentScanAsh* document_scan_ash() { return document_scan_ash_.get(); }

  DownloadControllerAsh* download_controller_ash() {
    return download_controller_ash_.get();
  }

  DownloadStatusUpdaterAsh* download_status_updater_ash() {
    return download_status_updater_ash_.get();
  }

  EchoPrivateAsh* echo_private_ash() { return echo_private_ash_.get(); }

  EmbeddedAccessibilityHelperClientAsh*
  embedded_accessibility_helper_client_ash() {
    return embedded_accessibility_helper_client_ash_.get();
  }

  EmojiPickerAsh* emoji_picker_ash() { return emoji_picker_ash_.get(); }

  ExtensionInfoPrivateAsh* extension_info_private_ash() {
    return extension_info_private_ash_.get();
  }

  ExtensionPrinterServiceAsh* extension_printer_service_ash() {
    return extension_printer_service_ash_.get();
  }

  FileSystemAccessCloudIdentifierProviderAsh*
  file_system_access_cloud_identifier_provider_ash() {
    return file_system_access_cloud_identifier_provider_ash_.get();
  }

  FileSystemProviderServiceAsh* file_system_provider_service_ash() {
    return file_system_provider_service_ash_.get();
  }

  FileManagerAsh* file_manager_ash() { return file_manager_ash_.get(); }

  ForceInstalledTrackerAsh* force_installed_tracker_ash() {
    return force_installed_tracker_ash_.get();
  }

  FullRestoreAsh* full_restore_ash() { return full_restore_ash_.get(); }

  FullscreenControllerAsh* fullscreen_controller_ash() {
    return fullscreen_controller_ash_.get();
  }

  ImageWriterAsh* image_writer_ash() { return image_writer_ash_.get(); }

  InputMethodsAsh* input_methods_ash() { return input_methods_ash_.get(); }

  KeystoreServiceAsh* keystore_service_ash() {
    return keystore_service_ash_.get();
  }

  KioskSessionServiceAsh* kiosk_session_service() {
    return kiosk_session_service_ash_.get();
  }

  LocalPrinterAsh* local_printer_ash() { return local_printer_ash_.get(); }

  LoginAsh* login_ash() { return login_ash_.get(); }

  LoginScreenStorageAsh* login_screen_storage_ash() {
    return login_screen_storage_ash_.get();
  }

  LoginStateAsh* login_state_ash() { return login_state_ash_.get(); }

  ash::MagicBoostControllerAsh* magic_boost_controller_ash() {
    return magic_boost_controller_ash_.get();
  }

  MediaAppAsh* media_app_ash() { return media_app_ash_.get(); }

  MediaUIAsh* media_ui_ash() { return media_ui_ash_.get(); }

  MultiCaptureServiceAsh* multi_capture_service_ash() {
    return multi_capture_service_ash_.get();
  }

  NetworkingAttributesAsh* networking_attributes_ash() {
    return networking_attributes_ash_.get();
  }

  NetworkingPrivateAsh* networking_private_ash() {
    return networking_private_ash_.get();
  }

  ParentAccessAsh* parent_access_ash() { return parent_access_ash_.get(); }

  PaymentAppInstanceAsh* payment_app_instance_ash() {
    return payment_app_instance_ash_.get();
  }

  ash::printing::PrintPreviewWebcontentsAdapterAsh*
  print_preview_webcontents_adapter_ash() {
    return print_preview_webcontents_adapter_ash_.get();
  }

#if BUILDFLAG(USE_CUPS)
  PrintingMetricsAsh* printing_metrics_ash() {
    return printing_metrics_ash_.get();
  }
#endif  // BUILDFLAG(USE_CUPS)

  ash::ProbeServiceAsh* probe_service_ash() { return probe_service_ash_.get(); }

  ScreenAIDownloaderAsh* screen_ai_downloader_ash() {
    return screen_ai_downloader_ash_.get();
  }

  ScreenManagerAsh* screen_manager_ash() { return screen_manager_ash_.get(); }

  SearchProviderAsh* search_provider_ash() {
    return search_provider_ash_.get();
  }

  SharesheetAsh* sharesheet_ash() { return sharesheet_ash_.get(); }

  ash::SmartReaderManagerAsh* smart_reader_manager_ash() {
    return smart_reader_manager_ash_.get();
  }

  StructuredMetricsServiceAsh* structured_metrics_service_ash() {
    return structured_metrics_service_ash_.get();
  }

  SuggestionServiceAsh* suggestion_service_ash() {
    return suggestion_service_ash_.get();
  }

  TaskManagerAsh* task_manager_ash() { return task_manager_ash_.get(); }

  TtsAsh* tts_ash() { return tts_ash_.get(); }

  WallpaperAsh* wallpaper_ash() { return wallpaper_ash_.get(); }

  WebAppServiceAsh* web_app_service_ash() { return web_app_service_ash_.get(); }

  WebKioskServiceAsh* web_kiosk_service_ash() {
    return web_kiosk_service_ash_.get();
  }

  WebPageInfoFactoryAsh* web_page_info_factory_ash() {
    return web_page_info_factory_ash_.get();
  }

  ash::VideoConferenceManagerAsh* video_conference_manager_ash() {
    return video_conference_manager_ash_.get();
  }

  VirtualKeyboardAsh* virtual_keyboard_ash() {
    return virtual_keyboard_ash_.get();
  }

  VpnExtensionObserverAsh* vpn_extension_observer_ash() {
    return vpn_extension_observer_ash_.get();
  }

  VpnServiceAsh* vpn_service_ash() { return vpn_service_ash_.get(); }

  NetworkSettingsServiceAsh* network_settings_service_ash() {
    return network_settings_service_ash_.get();
  }

  // Caller is responsible for ensuring that the pointer stays valid.
  void SetTestControllerForTesting(
      std::unique_ptr<TestControllerReceiver> test_controller);

 private:
  // Called when a connection is lost.
  void OnDisconnected();

  std::unique_ptr<ArcAsh> arc_ash_;
  std::unique_ptr<AudioServiceAsh> audio_service_ash_;
  std::unique_ptr<AutomationAsh> automation_ash_;
  std::unique_ptr<BrowserServiceHostAsh> browser_service_host_ash_;
  std::unique_ptr<BrowserVersionServiceAsh> browser_version_service_ash_;
  std::unique_ptr<CecPrivateAsh> cec_private_ash_;
  std::unique_ptr<CertDatabaseAsh> cert_database_ash_;
  std::unique_ptr<CertProvisioningAsh> cert_provisioning_ash_;
  std::unique_ptr<ChapsServiceAsh> chaps_service_ash_;
  std::unique_ptr<ChromeAppKioskServiceAsh> chrome_app_kiosk_service_ash_;
  std::unique_ptr<ChromeAppWindowTrackerAsh> chrome_app_window_tracker_ash_;
  std::unique_ptr<ClipboardAsh> clipboard_ash_;
  std::unique_ptr<ClipboardHistoryAsh> clipboard_history_ash_;
  std::unique_ptr<ContentProtectionAsh> content_protection_ash_;
  std::unique_ptr<DebugInterfaceRegistererAsh> debug_interface_registerer_ash_;
  std::unique_ptr<DeskAsh> desk_ash_;
  std::unique_ptr<DeskProfilesAsh> desk_profiles_ash_;
  std::unique_ptr<DeskTemplateAsh> desk_template_ash_;
  std::unique_ptr<DeviceAttributesAsh> device_attributes_ash_;
  std::unique_ptr<DeviceLocalAccountExtensionServiceAsh>
      device_local_account_extension_service_ash_;
  std::unique_ptr<DeviceOAuth2TokenServiceAsh> device_oauth2_token_service_ash_;
  std::unique_ptr<DeviceSettingsAsh> device_settings_ash_;
  std::unique_ptr<ash::DiagnosticsServiceAsh> diagnostics_service_ash_;
  std::unique_ptr<apps::DigitalGoodsFactoryAsh> digital_goods_factory_ash_;
  std::unique_ptr<DlpAsh> dlp_ash_;
  std::unique_ptr<DocumentScanAsh> document_scan_ash_;
  std::unique_ptr<DownloadControllerAsh> download_controller_ash_;
  std::unique_ptr<DownloadStatusUpdaterAsh> download_status_updater_ash_;
  std::unique_ptr<DriveIntegrationServiceAsh> drive_integration_service_ash_;
  std::unique_ptr<EchoPrivateAsh> echo_private_ash_;
  std::unique_ptr<EmbeddedAccessibilityHelperClientAsh>
      embedded_accessibility_helper_client_ash_;
  std::unique_ptr<EmojiPickerAsh> emoji_picker_ash_;
  std::unique_ptr<ExtensionInfoPrivateAsh> extension_info_private_ash_;
  std::unique_ptr<ExtensionPrinterServiceAsh> extension_printer_service_ash_;
  std::unique_ptr<EyeDropperAsh> eye_dropper_ash_;
  std::unique_ptr<FieldTrialServiceAsh> field_trial_service_ash_;
  std::unique_ptr<FileChangeServiceBridgeAsh> file_change_service_bridge_ash_;
  std::unique_ptr<FileManagerAsh> file_manager_ash_;
  std::unique_ptr<FileSystemAccessCloudIdentifierProviderAsh>
      file_system_access_cloud_identifier_provider_ash_;
  std::unique_ptr<FileSystemProviderServiceAsh>
      file_system_provider_service_ash_;
  std::unique_ptr<ForceInstalledTrackerAsh> force_installed_tracker_ash_;
  std::unique_ptr<FullRestoreAsh> full_restore_ash_;
  std::unique_ptr<FullscreenControllerAsh> fullscreen_controller_ash_;
  std::unique_ptr<GeolocationServiceAsh> geolocation_service_ash_;
  std::unique_ptr<IdentityManagerAsh> identity_manager_ash_;
  std::unique_ptr<IdleServiceAsh> idle_service_ash_;
  std::unique_ptr<InputMethodsAsh> input_methods_ash_;
  std::unique_ptr<ImageWriterAsh> image_writer_ash_;
  std::unique_ptr<KerberosInBrowserAsh> kerberos_in_browser_ash_;
  std::unique_ptr<KeystoreServiceAsh> keystore_service_ash_;
  std::unique_ptr<KioskSessionServiceAsh> kiosk_session_service_ash_;
  std::unique_ptr<LacrosShelfItemTracker> lacros_shelf_item_tracker_;
  std::unique_ptr<LocalPrinterAsh> local_printer_ash_;
  std::unique_ptr<LoginAsh> login_ash_;
  std::unique_ptr<LoginScreenStorageAsh> login_screen_storage_ash_;
  std::unique_ptr<LoginStateAsh> login_state_ash_;
  std::unique_ptr<ash::MagicBoostControllerAsh> magic_boost_controller_ash_;
  std::unique_ptr<MediaAppAsh> media_app_ash_;
  std::unique_ptr<MediaUIAsh> media_ui_ash_;
  std::unique_ptr<MessageCenterAsh> message_center_ash_;
  std::unique_ptr<MetricsAsh> metrics_ash_;
  std::unique_ptr<MetricsReportingAsh> metrics_reporting_ash_;
  std::unique_ptr<MultiCaptureServiceAsh> multi_capture_service_ash_;
  std::unique_ptr<NativeThemeServiceAsh> native_theme_service_ash_;
  std::unique_ptr<NetworkingAttributesAsh> networking_attributes_ash_;
  std::unique_ptr<NetworkingPrivateAsh> networking_private_ash_;
  std::unique_ptr<NetworkSettingsServiceAsh> network_settings_service_ash_;
  std::unique_ptr<OneDriveNotificationServiceAsh>
      one_drive_notification_service_ash_;
  std::unique_ptr<OneDriveIntegrationServiceAsh>
      one_drive_integration_service_ash_;
  std::unique_ptr<ParentAccessAsh> parent_access_ash_;
  std::unique_ptr<PaymentAppInstanceAsh> payment_app_instance_ash_;
  std::unique_ptr<PolicyServiceAsh> policy_service_ash_;
  std::unique_ptr<PowerAsh> power_ash_;
  std::unique_ptr<PrefsAsh> prefs_ash_;
  std::unique_ptr<NonclosableAppToastServiceAsh>
      nonclosable_app_toast_service_ash_;
#if BUILDFLAG(USE_CUPS)
  std::unique_ptr<PrintingMetricsAsh> printing_metrics_ash_;
#endif  // BUILDFLAG(USE_CUPS)
  std::unique_ptr<ash::TelemetryDiagnosticsRoutineServiceAsh>
      telemetry_diagnostic_routine_service_ash_;
  std::unique_ptr<ash::TelemetryEventServiceAsh> telemetry_event_service_ash_;
  std::unique_ptr<ash::TelemetryManagementServiceAsh>
      telemetry_management_service_ash_;
  std::unique_ptr<ash::ProbeServiceAsh> probe_service_ash_;
  std::unique_ptr<RemotingAsh> remoting_ash_;
  std::unique_ptr<ResourceManagerAsh> resource_manager_ash_;
  std::unique_ptr<ash::printing::PrintPreviewWebcontentsAdapterAsh>
      print_preview_webcontents_adapter_ash_;
  std::unique_ptr<ScreenAIDownloaderAsh> screen_ai_downloader_ash_;
  std::unique_ptr<ScreenManagerAsh> screen_manager_ash_;
  std::unique_ptr<SearchProviderAsh> search_provider_ash_;
  std::unique_ptr<SharesheetAsh> sharesheet_ash_;
  std::unique_ptr<ash::SmartReaderManagerAsh> smart_reader_manager_ash_;
  std::unique_ptr<SpeechRecognitionAsh> speech_recognition_ash_;
  std::unique_ptr<StructuredMetricsServiceAsh> structured_metrics_service_ash_;
  std::unique_ptr<SuggestionServiceAsh> suggestion_service_ash_;
  std::unique_ptr<TaskManagerAsh> task_manager_ash_;
  std::unique_ptr<TimeZoneServiceAsh> time_zone_service_ash_;
  std::unique_ptr<TtsAsh> tts_ash_;
  std::unique_ptr<ash::VideoConferenceManagerAsh> video_conference_manager_ash_;
  std::unique_ptr<VirtualKeyboardAsh> virtual_keyboard_ash_;
  std::unique_ptr<VolumeManagerAsh> volume_manager_ash_;
  std::unique_ptr<VpnExtensionObserverAsh> vpn_extension_observer_ash_;
  std::unique_ptr<VpnServiceAsh> vpn_service_ash_;
  std::unique_ptr<WallpaperAsh> wallpaper_ash_;
  std::unique_ptr<WebAppServiceAsh> web_app_service_ash_;
  std::unique_ptr<WebKioskServiceAsh> web_kiosk_service_ash_;
  std::unique_ptr<WebPageInfoFactoryAsh> web_page_info_factory_ash_;

  // Only set in the test ash chrome binary. In production ash this is always
  // unset.
  std::unique_ptr<TestControllerReceiver> test_controller_;

  mojo::ReceiverSet<mojom::Crosapi, CrosapiId> receiver_set_;
  std::map<mojo::ReceiverId, base::OnceClosure> disconnect_handler_map_;

  base::WeakPtrFactory<CrosapiAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_CROSAPI_ASH_H_
