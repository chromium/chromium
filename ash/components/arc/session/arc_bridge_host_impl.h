// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_ARC_BRIDGE_HOST_IMPL_H_
#define ASH_COMPONENTS_ARC_SESSION_ARC_BRIDGE_HOST_IMPL_H_

#include <memory>
#include <vector>

#include "ash/components/arc/mojom/arc_bridge.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/connection_holder.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "chromeos/ash/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace arc {

class ArcBridgeService;
class MojoChannelBase;

// Implementation of the ArcBridgeHost.
// The ArcBridgeHost also registers in Mojo Service Manager by default.
// The lifetime of ArcBridgeHost mojo channel is tied to this instance.
// Also, any ARC related Mojo channel will be closed if ArcBridgeHost Mojo
// channel is closed on error.
// When ARC Instance (not Host) Mojo channel gets ready (= passed via
// OnFooInstanceReady(), and the QueryVersion() gets completed), then this sets
// the raw pointer to the ArcBridgeService so that other services can access
// to the pointer, and resets it on channel closing.
// Note that ArcBridgeService must be alive while ArcBridgeHostImpl is alive.
class ArcBridgeHostImpl
    : public mojom::ArcBridgeHost,
      public chromeos::mojo_service_manager::mojom::ServiceProvider {
 public:
  explicit ArcBridgeHostImpl(ArcBridgeService* arc_bridge_service);

  ArcBridgeHostImpl(const ArcBridgeHostImpl&) = delete;
  ArcBridgeHostImpl& operator=(const ArcBridgeHostImpl&) = delete;

  ~ArcBridgeHostImpl() override;

  void AddReceiver(
      mojo::PendingReceiver<mojom::ArcBridgeHost> pending_receiver);

  // ArcBridgeHost overrides.
  void OnAccessibilityHelperInstanceReady(
      mojo::PendingRemote<ax::android::mojom::AccessibilityHelperInstance>
          accessibility_helper_remote) override;
  void OnAdbdMonitorInstanceReady(
      mojo::PendingRemote<mojom::AdbdMonitorInstance> adbd_monitor_remote)
      override;
  void OnAppInstanceReady(
      mojo::PendingRemote<mojom::AppInstance> app_ptr) override;
  void OnAppPermissionsInstanceReady(
      mojo::PendingRemote<mojom::AppPermissionsInstance> app_permissions_remote)
      override;
  void OnAppfuseInstanceReady(
      mojo::PendingRemote<mojom::AppfuseInstance> appfuse_remote) override;
  void OnArcWifiInstanceReady(
      mojo::PendingRemote<mojom::ArcWifiInstance> arc_wifi_remote) override;
  void OnAudioInstanceReady(
      mojo::PendingRemote<mojom::AudioInstance> audio_remote) override;
  void OnAuthInstanceReady(
      mojo::PendingRemote<mojom::AuthInstance> auth_remote) override;
  void OnBackupSettingsInstanceReady(
      mojo::PendingRemote<mojom::BackupSettingsInstance> backup_settings_remote)
      override;
  void OnBluetoothInstanceReady(
      mojo::PendingRemote<mojom::BluetoothInstance> bluetooth_remote) override;
  void OnBootPhaseMonitorInstanceReady(
      mojo::PendingRemote<mojom::BootPhaseMonitorInstance>
          boot_phase_monitor_remote) override;
  void OnCameraInstanceReady(
      mojo::PendingRemote<mojom::CameraInstance> camera_remote) override;
  void OnChromeFeatureFlagsInstanceReady(
      mojo::PendingRemote<mojom::ChromeFeatureFlagsInstance>
          chrome_feature_flags_remote) override;
  void OnCompatibilityModeInstanceReady(
      mojo::PendingRemote<mojom::CompatibilityModeInstance>
          compatibility_mode_remote) override;
  void OnCrashCollectorInstanceReady(
      mojo::PendingRemote<mojom::CrashCollectorInstance> crash_collector_remote)
      override;
  void OnDigitalGoodsInstanceReady(
      mojo::PendingRemote<mojom::DigitalGoodsInstance> digital_goods_remote)
      override;
  void OnDiskSpaceInstanceReady(
      mojo::PendingRemote<mojom::DiskSpaceInstance> disk_space_remote) override;
  void OnEnterpriseReportingInstanceReady(
      mojo::PendingRemote<mojom::EnterpriseReportingInstance>
          enterprise_reporting_remote) override;
  void OnErrorNotificationInstanceReady(
      mojo::PendingRemote<mojom::ErrorNotificationInstance> error_dialog_remote)
      override;
  void OnFileSystemInstanceReady(mojo::PendingRemote<mojom::FileSystemInstance>
                                     file_system_remote) override;
  void OnIioSensorInstanceReady(
      mojo::PendingRemote<mojom::IioSensorInstance> iio_sensor_remote) override;
  void OnImeInstanceReady(
      mojo::PendingRemote<mojom::ImeInstance> ime_remote) override;
  void OnInputMethodManagerInstanceReady(
      mojo::PendingRemote<mojom::InputMethodManagerInstance>
          input_method_manager_remote) override;
  void OnIntentHelperInstanceReady(
      mojo::PendingRemote<mojom::IntentHelperInstance> intent_helper_remote)
      override;
  void OnKeymasterInstanceReady(
      mojo::PendingRemote<mojom::KeymasterInstance> keymaster_remote) override;
  void OnKeyMintInstanceReady(
      mojo::PendingRemote<mojom::keymint::KeyMintInstance> keymint_remote)
      override;
  void OnMediaSessionInstanceReady(
      mojo::PendingRemote<mojom::MediaSessionInstance> media_session_remote)
      override;
  void OnMemoryInstanceReady(
      mojo::PendingRemote<mojom::MemoryInstance> memory_remote) override;
  void OnMetricsInstanceReady(
      mojo::PendingRemote<mojom::MetricsInstance> metrics_remote) override;
  void OnMidisInstanceReady(
      mojo::PendingRemote<mojom::MidisInstance> midis_remote) override;
  void OnNearbyShareInstanceReady(
      mojo::PendingRemote<mojom::NearbyShareInstance> nearby_share_remote)
      override;
  void OnNetInstanceReady(
      mojo::PendingRemote<mojom::NetInstance> net_remote) override;
  void OnNotificationsInstanceReady(
      mojo::PendingRemote<mojom::NotificationsInstance> notifications_remote)
      override;
  void OnObbMounterInstanceReady(mojo::PendingRemote<mojom::ObbMounterInstance>
                                     obb_mounter_remote) override;
  void OnOemCryptoInstanceReady(
      mojo::PendingRemote<mojom::OemCryptoInstance> oemcrypto_remote) override;
  void OnOnDeviceSafetyInstanceReady(
      mojo::PendingRemote<mojom::OnDeviceSafetyInstance>
          on_device_safety_remote) override;
  void OnPaymentAppInstanceReady(
      mojo::PendingRemote<chromeos::payments::mojom::PaymentAppInstance>
          payment_app_remote) override;
  void OnPipInstanceReady(
      mojo::PendingRemote<mojom::PipInstance> policy_remote) override;
  void OnPolicyInstanceReady(
      mojo::PendingRemote<mojom::PolicyInstance> policy_remote) override;
  void OnPowerInstanceReady(
      mojo::PendingRemote<mojom::PowerInstance> power_remote) override;
  void OnPrintSpoolerInstanceReady(
      mojo::PendingRemote<mojom::PrintSpoolerInstance> print_spooler_remote)
      override;
  void OnPrivacyItemsInstanceReady(
      mojo::PendingRemote<mojom::PrivacyItemsInstance> privacy_items_remote)
      override;
  void OnProcessInstanceReady(
      mojo::PendingRemote<mojom::ProcessInstance> process_remote) override;
  void OnScreenCaptureInstanceReady(
      mojo::PendingRemote<mojom::ScreenCaptureInstance> screen_capture_remote)
      override;
  void OnSharesheetInstanceReady(mojo::PendingRemote<mojom::SharesheetInstance>
                                     sharesheet_remote) override;
  void OnSystemStateInstanceReady(
      mojo::PendingRemote<mojom::SystemStateInstance> system_state_remote)
      override;
  void OnSystemUiInstanceReady(
      mojo::PendingRemote<mojom::SystemUiInstance> system_ui_remote) override;
  void OnTimerInstanceReady(
      mojo::PendingRemote<mojom::TimerInstance> timer_remote) override;
  void OnTracingInstanceReady(
      mojo::PendingRemote<mojom::TracingInstance> trace_remote) override;
  void OnTtsInstanceReady(
      mojo::PendingRemote<mojom::TtsInstance> tts_remote) override;
  void OnUsbHostInstanceReady(
      mojo::PendingRemote<mojom::UsbHostInstance> usb_host_remote) override;
  void OnVideoInstanceReady(
      mojo::PendingRemote<mojom::VideoInstance> video_remote) override;
  void OnVolumeMounterInstanceReady(
      mojo::PendingRemote<mojom::VolumeMounterInstance> volume_mounter_remote)
      override;
  void OnWakeLockInstanceReady(
      mojo::PendingRemote<mojom::WakeLockInstance> wake_lock_remote) override;
  void OnWallpaperInstanceReady(
      mojo::PendingRemote<mojom::WallpaperInstance> wallpaper_remote) override;
  void OnWebApkInstanceReady(
      mojo::PendingRemote<mojom::WebApkInstance> webapk_remote) override;

  size_t GetNumMojoChannelsForTesting() const;

 private:
  // chromeos::mojo_service_manager::mojom::ServiceProvider overrides.
  void Request(
      chromeos::mojo_service_manager::mojom::ProcessIdentityPtr identity,
      mojo::ScopedMessagePipeHandle receiver) override;

  // Called when the bridge channel is closed. This typically only happens when
  // the ARC instance crashes.
  void OnClosed();

  // The common implementation to handle ArcBridgeHost overrides.
  // |T| is a ARC Mojo Instance type.
  template <typename InstanceType, typename HostType>
  void OnInstanceReady(ConnectionHolder<InstanceType, HostType>* holder,
                       mojo::PendingRemote<InstanceType> remote);

  // Called if one of the established channels is closed.
  void OnChannelClosed(MojoChannelBase* channel);

  THREAD_CHECKER(thread_checker_);

  // Owned by ArcServiceManager.
  const raw_ptr<ArcBridgeService> arc_bridge_service_;

  // The mojo receiver of service provider.
  mojo::Receiver<chromeos::mojo_service_manager::mojom::ServiceProvider>
      provider_receiver_{this};
  // The mojo receiver set of ArcBridgeHost.
  mojo::ReceiverSet<mojom::ArcBridgeHost> receivers_;

  // Put as a last member to ensure that any callback tied to the elements
  // is not invoked.
  std::vector<std::unique_ptr<MojoChannelBase>> mojo_channels_
      GUARDED_BY_CONTEXT(thread_checker_);
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SESSION_ARC_BRIDGE_HOST_IMPL_H_
