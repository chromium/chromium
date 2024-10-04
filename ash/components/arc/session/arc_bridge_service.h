// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_ARC_BRIDGE_SERVICE_H_
#define ASH_COMPONENTS_ARC_SESSION_ARC_BRIDGE_SERVICE_H_

#include "ash/components/arc/session/connection_holder.h"
#include "base/observer_list.h"

namespace chromeos::payments::mojom {
class PaymentAppInstance;
}  // namespace chromeos::payments::mojom
namespace ax::android::mojom {
class AccessibilityHelperInstance;
class AccessibilityHelperHost;
}  // namespace ax::android::mojom

namespace arc {

namespace mojom {

// Instead of including ash/components/arc/mojom/arc_bridge.mojom.h, list all
// the instance classes here for faster build.
class AdbdMonitorHost;
class AdbdMonitorInstance;
class AppHost;
class AppInstance;
class AppPermissionsInstance;
class AppfuseHost;
class AppfuseInstance;
class ArcWifiHost;
class ArcWifiInstance;
class AudioHost;
class AudioInstance;
class AuthHost;
class AuthInstance;
class BackupSettingsInstance;
class BluetoothHost;
class BluetoothInstance;
class BootPhaseMonitorHost;
class BootPhaseMonitorInstance;
class CameraHost;
class CameraInstance;
class ChromeFeatureFlagsInstance;
class CastReceiverInstance;
class CompatibilityModeInstance;
class CrashCollectorHost;
class CrashCollectorInstance;
class DigitalGoodsInstance;
class DiskSpaceHost;
class DiskSpaceInstance;
class EnterpriseReportingHost;
class EnterpriseReportingInstance;
class ErrorNotificationHost;
class ErrorNotificationInstance;
class FileSystemHost;
class FileSystemInstance;
class IioSensorHost;
class IioSensorInstance;
class ImeHost;
class ImeInstance;
class InputMethodManagerHost;
class InputMethodManagerInstance;
class IntentHelperHost;
class IntentHelperInstance;
class KeymasterHost;
class KeymasterInstance;
class MediaSessionInstance;
class MemoryInstance;
class MetricsHost;
class MetricsInstance;
class MidisHost;
class MidisInstance;
class NearbyShareHost;
class NearbyShareInstance;
class NetHost;
class NetInstance;
class ObbMounterHost;
class ObbMounterInstance;
class OemCryptoHost;
class OemCryptoInstance;
class OnDeviceSafetyInstance;
class PipHost;
class PipInstance;
class PolicyHost;
class PolicyInstance;
class PowerHost;
class PowerInstance;
class PrintSpoolerHost;
class PrintSpoolerInstance;
class PrivacyItemsHost;
class PrivacyItemsInstance;
class ProcessInstance;
class ScreenCaptureHost;
class ScreenCaptureInstance;
class SharesheetHost;
class SharesheetInstance;
class SystemStateHost;
class SystemStateInstance;
class SystemUiInstance;
class TimerHost;
class TimerInstance;
class TracingInstance;
class TtsHost;
class TtsInstance;
class UsbHostHost;
class UsbHostInstance;
class VideoHost;
class VideoInstance;
class VolumeMounterHost;
class VolumeMounterInstance;
class WakeLockHost;
class WakeLockInstance;
class WallpaperHost;
class WallpaperInstance;
class WebApkInstance;

namespace keymint {
class KeyMintHost;
class KeyMintInstance;
}  // namespace keymint

}  // namespace mojom

// Holds Mojo channels which proxy to ARC side implementation. The actual
// instances are set/removed via ArcBridgeHostImpl.
class ArcBridgeService {
 public:
  // Observer for those services outside of ArcBridgeService which want to know
  // ArcBridgeService events.
  class Observer : public base::CheckedObserver {
   public:
    // Called immediately before the ArcBridgeHost is closed.
    virtual void BeforeArcBridgeClosed() {}
    // Called immediately after the ArcBridgeHost is closed.
    virtual void AfterArcBridgeClosed() {}

   protected:
    ~Observer() override = default;
  };

  ArcBridgeService();

  ArcBridgeService(const ArcBridgeService&) = delete;
  ArcBridgeService& operator=(const ArcBridgeService&) = delete;

  ~ArcBridgeService();

  // Adds or removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Call these methods to invoke the corresponding methods on the observers.
  void ObserveBeforeArcBridgeClosed();
  void ObserveAfterArcBridgeClosed();

  ConnectionHolder<ax::android::mojom::AccessibilityHelperInstance,
                   ax::android::mojom::AccessibilityHelperHost>*
  accessibility_helper() {
    return &accessibility_helper_;
  }
  ConnectionHolder<mojom::AdbdMonitorInstance, mojom::AdbdMonitorHost>*
  adbd_monitor() {
    return &adbd_monitor_;
  }
  ConnectionHolder<mojom::AppInstance, mojom::AppHost>* app() { return &app_; }
  ConnectionHolder<mojom::AppPermissionsInstance>* app_permissions() {
    return &app_permissions_;
  }
  ConnectionHolder<mojom::AppfuseInstance, mojom::AppfuseHost>* appfuse() {
    return &appfuse_;
  }
  ConnectionHolder<mojom::ArcWifiInstance, mojom::ArcWifiHost>* arc_wifi() {
    return &arc_wifi_;
  }
  ConnectionHolder<mojom::AudioInstance, mojom::AudioHost>* audio() {
    return &audio_;
  }
  ConnectionHolder<mojom::AuthInstance, mojom::AuthHost>* auth() {
    return &auth_;
  }
  ConnectionHolder<mojom::BackupSettingsInstance>* backup_settings() {
    return &backup_settings_;
  }
  ConnectionHolder<mojom::BluetoothInstance, mojom::BluetoothHost>*
  bluetooth() {
    return &bluetooth_;
  }
  ConnectionHolder<mojom::BootPhaseMonitorInstance,
                   mojom::BootPhaseMonitorHost>*
  boot_phase_monitor() {
    return &boot_phase_monitor_;
  }
  ConnectionHolder<mojom::CameraInstance, mojom::CameraHost>* camera() {
    return &camera_;
  }
  ConnectionHolder<mojom::CastReceiverInstance>* cast_receiver() {
    return &cast_receiver_;
  }
  ConnectionHolder<mojom::ChromeFeatureFlagsInstance>* chrome_feature_flags() {
    return &chrome_feature_flags_;
  }
  ConnectionHolder<mojom::CompatibilityModeInstance>* compatibility_mode() {
    return &compatibility_mode_;
  }
  ConnectionHolder<mojom::CrashCollectorInstance, mojom::CrashCollectorHost>*
  crash_collector() {
    return &crash_collector_;
  }
  ConnectionHolder<mojom::DigitalGoodsInstance>* digital_goods() {
    return &digital_goods_;
  }
  ConnectionHolder<mojom::DiskSpaceInstance, mojom::DiskSpaceHost>*
  disk_space() {
    return &disk_space_;
  }
  ConnectionHolder<mojom::EnterpriseReportingInstance,
                   mojom::EnterpriseReportingHost>*
  enterprise_reporting() {
    return &enterprise_reporting_;
  }
  ConnectionHolder<mojom::ErrorNotificationInstance,
                   mojom::ErrorNotificationHost>*
  error_notification() {
    return &error_notification_;
  }
  ConnectionHolder<mojom::FileSystemInstance, mojom::FileSystemHost>*
  file_system() {
    return &file_system_;
  }
  ConnectionHolder<mojom::IioSensorInstance, mojom::IioSensorHost>*
  iio_sensor() {
    return &iio_sensor_;
  }
  ConnectionHolder<mojom::ImeInstance, mojom::ImeHost>* ime() { return &ime_; }
  ConnectionHolder<mojom::InputMethodManagerInstance,
                   mojom::InputMethodManagerHost>*
  input_method_manager() {
    return &input_method_manager_;
  }
  ConnectionHolder<mojom::IntentHelperInstance, mojom::IntentHelperHost>*
  intent_helper() {
    return &intent_helper_;
  }
  ConnectionHolder<mojom::KeymasterInstance, mojom::KeymasterHost>*
  keymaster() {
    return &keymaster_;
  }
  ConnectionHolder<mojom::keymint::KeyMintInstance,
                   mojom::keymint::KeyMintHost>*
  keymint() {
    return &keymint_;
  }
  ConnectionHolder<mojom::MediaSessionInstance>* media_session() {
    return &media_session_;
  }
  ConnectionHolder<mojom::MemoryInstance>* memory() { return &memory_; }
  ConnectionHolder<mojom::MetricsInstance, mojom::MetricsHost>* metrics() {
    return &metrics_;
  }
  ConnectionHolder<mojom::MidisInstance, mojom::MidisHost>* midis() {
    return &midis_;
  }
  ConnectionHolder<mojom::NearbyShareInstance, mojom::NearbyShareHost>*
  nearby_share() {
    return &nearby_share_;
  }
  ConnectionHolder<mojom::NetInstance, mojom::NetHost>* net() { return &net_; }
  ConnectionHolder<mojom::ObbMounterInstance, mojom::ObbMounterHost>*
  obb_mounter() {
    return &obb_mounter_;
  }
  ConnectionHolder<mojom::OemCryptoInstance, mojom::OemCryptoHost>*
  oemcrypto() {
    return &oemcrypto_;
  }
  ConnectionHolder<mojom::OnDeviceSafetyInstance>* on_device_safety() {
    return &on_device_safety_;
  }
  ConnectionHolder<chromeos::payments::mojom::PaymentAppInstance>*
  payment_app() {
    return &payment_app_;
  }
  ConnectionHolder<mojom::PipInstance, mojom::PipHost>* pip() { return &pip_; }
  ConnectionHolder<mojom::PolicyInstance, mojom::PolicyHost>* policy() {
    return &policy_;
  }
  ConnectionHolder<mojom::PowerInstance, mojom::PowerHost>* power() {
    return &power_;
  }
  ConnectionHolder<mojom::PrintSpoolerInstance, mojom::PrintSpoolerHost>*
  print_spooler() {
    return &print_spooler_;
  }
  ConnectionHolder<mojom::PrivacyItemsInstance, mojom::PrivacyItemsHost>*
  privacy_items() {
    return &privacy_items_;
  }
  ConnectionHolder<mojom::ProcessInstance>* process() { return &process_; }
  ConnectionHolder<mojom::ScreenCaptureInstance, mojom::ScreenCaptureHost>*
  screen_capture() {
    return &screen_capture_;
  }
  ConnectionHolder<mojom::SharesheetInstance, mojom::SharesheetHost>*
  sharesheet() {
    return &sharesheet_;
  }
  ConnectionHolder<mojom::SystemStateInstance, mojom::SystemStateHost>*
  system_state() {
    return &system_state_;
  }
  ConnectionHolder<mojom::SystemUiInstance>* system_ui() { return &system_ui_; }
  ConnectionHolder<mojom::TimerInstance, mojom::TimerHost>* timer() {
    return &timer_;
  }
  ConnectionHolder<mojom::TracingInstance>* tracing() { return &tracing_; }
  ConnectionHolder<mojom::TtsInstance, mojom::TtsHost>* tts() { return &tts_; }
  ConnectionHolder<mojom::UsbHostInstance, mojom::UsbHostHost>* usb_host() {
    return &usb_host_;
  }
  ConnectionHolder<mojom::VideoInstance, mojom::VideoHost>* video() {
    return &video_;
  }
  ConnectionHolder<mojom::VolumeMounterInstance, mojom::VolumeMounterHost>*
  volume_mounter() {
    return &volume_mounter_;
  }
  ConnectionHolder<mojom::WakeLockInstance, mojom::WakeLockHost>* wake_lock() {
    return &wake_lock_;
  }
  ConnectionHolder<mojom::WallpaperInstance, mojom::WallpaperHost>*
  wallpaper() {
    return &wallpaper_;
  }
  ConnectionHolder<mojom::WebApkInstance>* webapk() { return &webapk_; }

 private:
  base::ObserverList<Observer> observer_list_;

  ConnectionHolder<ax::android::mojom::AccessibilityHelperInstance,
                   ax::android::mojom::AccessibilityHelperHost>
      accessibility_helper_;
  ConnectionHolder<mojom::AdbdMonitorInstance, mojom::AdbdMonitorHost>
      adbd_monitor_;
  ConnectionHolder<mojom::AppInstance, mojom::AppHost> app_;
  ConnectionHolder<mojom::AppPermissionsInstance> app_permissions_;
  ConnectionHolder<mojom::AppfuseInstance, mojom::AppfuseHost> appfuse_;
  ConnectionHolder<mojom::ArcWifiInstance, mojom::ArcWifiHost> arc_wifi_;
  ConnectionHolder<mojom::AudioInstance, mojom::AudioHost> audio_;
  ConnectionHolder<mojom::AuthInstance, mojom::AuthHost> auth_;
  ConnectionHolder<mojom::BackupSettingsInstance> backup_settings_;
  ConnectionHolder<mojom::BluetoothInstance, mojom::BluetoothHost> bluetooth_;
  ConnectionHolder<mojom::BootPhaseMonitorInstance, mojom::BootPhaseMonitorHost>
      boot_phase_monitor_;
  ConnectionHolder<mojom::CameraInstance, mojom::CameraHost> camera_;
  ConnectionHolder<mojom::CastReceiverInstance> cast_receiver_;
  ConnectionHolder<mojom::ChromeFeatureFlagsInstance> chrome_feature_flags_;
  ConnectionHolder<mojom::CompatibilityModeInstance> compatibility_mode_;
  ConnectionHolder<mojom::CrashCollectorInstance, mojom::CrashCollectorHost>
      crash_collector_;
  ConnectionHolder<mojom::DigitalGoodsInstance> digital_goods_;
  ConnectionHolder<mojom::DiskSpaceInstance, mojom::DiskSpaceHost> disk_space_;
  ConnectionHolder<mojom::EnterpriseReportingInstance,
                   mojom::EnterpriseReportingHost>
      enterprise_reporting_;
  ConnectionHolder<mojom::ErrorNotificationInstance,
                   mojom::ErrorNotificationHost>
      error_notification_;
  ConnectionHolder<mojom::FileSystemInstance, mojom::FileSystemHost>
      file_system_;
  ConnectionHolder<mojom::IioSensorInstance, mojom::IioSensorHost> iio_sensor_;
  ConnectionHolder<mojom::ImeInstance, mojom::ImeHost> ime_;
  ConnectionHolder<mojom::InputMethodManagerInstance,
                   mojom::InputMethodManagerHost>
      input_method_manager_;
  ConnectionHolder<mojom::IntentHelperInstance, mojom::IntentHelperHost>
      intent_helper_;
  ConnectionHolder<mojom::KeymasterInstance, mojom::KeymasterHost> keymaster_;
  ConnectionHolder<mojom::keymint::KeyMintInstance, mojom::keymint::KeyMintHost>
      keymint_;
  ConnectionHolder<mojom::MediaSessionInstance> media_session_;
  ConnectionHolder<mojom::MemoryInstance> memory_;
  ConnectionHolder<mojom::MetricsInstance, mojom::MetricsHost> metrics_;
  ConnectionHolder<mojom::MidisInstance, mojom::MidisHost> midis_;
  ConnectionHolder<mojom::NearbyShareInstance, mojom::NearbyShareHost>
      nearby_share_;
  ConnectionHolder<mojom::NetInstance, mojom::NetHost> net_;
  ConnectionHolder<mojom::ObbMounterInstance, mojom::ObbMounterHost>
      obb_mounter_;
  ConnectionHolder<mojom::OemCryptoInstance, mojom::OemCryptoHost> oemcrypto_;
  ConnectionHolder<mojom::OnDeviceSafetyInstance> on_device_safety_;
  ConnectionHolder<chromeos::payments::mojom::PaymentAppInstance> payment_app_;
  ConnectionHolder<mojom::PipInstance, mojom::PipHost> pip_;
  ConnectionHolder<mojom::PolicyInstance, mojom::PolicyHost> policy_;
  ConnectionHolder<mojom::PowerInstance, mojom::PowerHost> power_;
  ConnectionHolder<mojom::PrintSpoolerInstance, mojom::PrintSpoolerHost>
      print_spooler_;
  ConnectionHolder<mojom::PrivacyItemsInstance, mojom::PrivacyItemsHost>
      privacy_items_;
  ConnectionHolder<mojom::ProcessInstance> process_;
  ConnectionHolder<mojom::ScreenCaptureInstance, mojom::ScreenCaptureHost>
      screen_capture_;
  ConnectionHolder<mojom::SharesheetInstance, mojom::SharesheetHost>
      sharesheet_;
  ConnectionHolder<mojom::SystemStateInstance, mojom::SystemStateHost>
      system_state_;
  ConnectionHolder<mojom::SystemUiInstance> system_ui_;
  ConnectionHolder<mojom::TimerInstance, mojom::TimerHost> timer_;
  ConnectionHolder<mojom::TracingInstance> tracing_;
  ConnectionHolder<mojom::TtsInstance, mojom::TtsHost> tts_;
  ConnectionHolder<mojom::UsbHostInstance, mojom::UsbHostHost> usb_host_;
  ConnectionHolder<mojom::VideoInstance, mojom::VideoHost> video_;
  ConnectionHolder<mojom::VolumeMounterInstance, mojom::VolumeMounterHost>
      volume_mounter_;
  ConnectionHolder<mojom::WakeLockInstance, mojom::WakeLockHost> wake_lock_;
  ConnectionHolder<mojom::WallpaperInstance, mojom::WallpaperHost> wallpaper_;
  ConnectionHolder<mojom::WebApkInstance> webapk_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SESSION_ARC_BRIDGE_SERVICE_H_
