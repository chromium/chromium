// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_arc_bridge_host.h"

#include "ash/components/arc/mojom/adbd.mojom.h"
#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/mojom/app_permissions.mojom.h"
#include "ash/components/arc/mojom/appfuse.mojom.h"
#include "ash/components/arc/mojom/arc_bridge.mojom.h"
#include "ash/components/arc/mojom/audio.mojom.h"
#include "ash/components/arc/mojom/auth.mojom.h"
#include "ash/components/arc/mojom/backup_settings.mojom.h"
#include "ash/components/arc/mojom/bluetooth.mojom.h"
#include "ash/components/arc/mojom/boot_phase_monitor.mojom.h"
#include "ash/components/arc/mojom/camera.mojom.h"
#include "ash/components/arc/mojom/compatibility_mode.mojom.h"
#include "ash/components/arc/mojom/crash_collector.mojom.h"
#include "ash/components/arc/mojom/disk_space.mojom.h"
#include "ash/components/arc/mojom/enterprise_reporting.mojom.h"
#include "ash/components/arc/mojom/error_notification.mojom.h"
#include "ash/components/arc/mojom/file_system.mojom.h"
#include "ash/components/arc/mojom/ime.mojom.h"
#include "ash/components/arc/mojom/input_method_manager.mojom.h"
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/components/arc/mojom/keymaster.mojom.h"
#include "ash/components/arc/mojom/keymint.mojom.h"
#include "ash/components/arc/mojom/media_session.mojom.h"
#include "ash/components/arc/mojom/memory.mojom.h"
#include "ash/components/arc/mojom/metrics.mojom.h"
#include "ash/components/arc/mojom/midis.mojom.h"
#include "ash/components/arc/mojom/nearby_share.mojom.h"
#include "ash/components/arc/mojom/net.mojom.h"
#include "ash/components/arc/mojom/notifications.mojom.h"
#include "ash/components/arc/mojom/obb_mounter.mojom.h"
#include "ash/components/arc/mojom/oemcrypto.mojom.h"
#include "ash/components/arc/mojom/on_device_safety.mojom.h"
#include "ash/components/arc/mojom/pip.mojom.h"
#include "ash/components/arc/mojom/policy.mojom.h"
#include "ash/components/arc/mojom/power.mojom.h"
#include "ash/components/arc/mojom/print_spooler.mojom.h"
#include "ash/components/arc/mojom/privacy_items.mojom.h"
#include "ash/components/arc/mojom/process.mojom.h"
#include "ash/components/arc/mojom/screen_capture.mojom.h"
#include "ash/components/arc/mojom/sharesheet.mojom.h"
#include "ash/components/arc/mojom/system_state.mojom.h"
#include "ash/components/arc/mojom/system_ui.mojom.h"
#include "ash/components/arc/mojom/timer.mojom.h"
#include "ash/components/arc/mojom/tracing.mojom.h"
#include "ash/components/arc/mojom/tts.mojom.h"
#include "ash/components/arc/mojom/usb_host.mojom.h"
#include "ash/components/arc/mojom/video.mojom.h"
#include "ash/components/arc/mojom/volume_mounter.mojom.h"
#include "ash/components/arc/mojom/wake_lock.mojom.h"
#include "ash/components/arc/mojom/wallpaper.mojom.h"
#include "services/accessibility/android/public/mojom/accessibility_helper.mojom.h"

namespace arc {

FakeArcBridgeHost::FakeArcBridgeHost() = default;

FakeArcBridgeHost::~FakeArcBridgeHost() = default;

void FakeArcBridgeHost::OnAccessibilityHelperInstanceReady(
    mojo::PendingRemote<ax::android::mojom::AccessibilityHelperInstance>
        accessibility_helper_remote) {}

void FakeArcBridgeHost::OnAdbdMonitorInstanceReady(
    mojo::PendingRemote<mojom::AdbdMonitorInstance> adbd_monitor_remote) {}

void FakeArcBridgeHost::OnAppInstanceReady(
    mojo::PendingRemote<mojom::AppInstance> app_ptr) {}

void FakeArcBridgeHost::OnAppPermissionsInstanceReady(
    mojo::PendingRemote<mojom::AppPermissionsInstance> app_permissions_remote) {
}

void FakeArcBridgeHost::OnAppfuseInstanceReady(
    mojo::PendingRemote<mojom::AppfuseInstance> app_remote) {}

void FakeArcBridgeHost::OnArcWifiInstanceReady(
    mojo::PendingRemote<mojom::ArcWifiInstance> arc_wifi_remote) {}

void FakeArcBridgeHost::OnAudioInstanceReady(
    mojo::PendingRemote<mojom::AudioInstance> audio_remote) {}

void FakeArcBridgeHost::OnAuthInstanceReady(
    mojo::PendingRemote<mojom::AuthInstance> auth_remote) {}

void FakeArcBridgeHost::OnBackupSettingsInstanceReady(
    mojo::PendingRemote<mojom::BackupSettingsInstance> backup_settings_remote) {
}

void FakeArcBridgeHost::OnBluetoothInstanceReady(
    mojo::PendingRemote<mojom::BluetoothInstance> bluetooth_remote) {}

void FakeArcBridgeHost::OnBootPhaseMonitorInstanceReady(
    mojo::PendingRemote<mojom::BootPhaseMonitorInstance>
        boot_phase_monitor_remote) {}

void FakeArcBridgeHost::OnCameraInstanceReady(
    mojo::PendingRemote<mojom::CameraInstance> camera_remote) {}

void FakeArcBridgeHost::OnChromeFeatureFlagsInstanceReady(
    mojo::PendingRemote<mojom::ChromeFeatureFlagsInstance>
        chrome_feature_flags_remote) {}

void FakeArcBridgeHost::OnCompatibilityModeInstanceReady(
    mojo::PendingRemote<mojom::CompatibilityModeInstance>
        compatibility_mode_remote) {}

void FakeArcBridgeHost::OnCrashCollectorInstanceReady(
    mojo::PendingRemote<mojom::CrashCollectorInstance> crash_collector_remote) {
}

void FakeArcBridgeHost::OnDigitalGoodsInstanceReady(
    mojo::PendingRemote<mojom::DigitalGoodsInstance> digital_goods_remote) {}

void FakeArcBridgeHost::OnDiskSpaceInstanceReady(
    mojo::PendingRemote<mojom::DiskSpaceInstance> disk_space_remote) {}

void FakeArcBridgeHost::OnEnterpriseReportingInstanceReady(
    mojo::PendingRemote<mojom::EnterpriseReportingInstance>
        enterprise_reporting_remote) {}

void FakeArcBridgeHost::OnErrorNotificationInstanceReady(
    mojo::PendingRemote<mojom::ErrorNotificationInstance>
        error_notification_remote) {}

void FakeArcBridgeHost::OnFileSystemInstanceReady(
    mojo::PendingRemote<mojom::FileSystemInstance> file_system_remote) {}

void FakeArcBridgeHost::OnIioSensorInstanceReady(
    mojo::PendingRemote<mojom::IioSensorInstance> iio_sensor_remote) {}

void FakeArcBridgeHost::OnImeInstanceReady(
    mojo::PendingRemote<mojom::ImeInstance> ime_remote) {}

void FakeArcBridgeHost::OnInputMethodManagerInstanceReady(
    mojo::PendingRemote<mojom::InputMethodManagerInstance>
        input_method_manager_remote) {}

void FakeArcBridgeHost::OnIntentHelperInstanceReady(
    mojo::PendingRemote<mojom::IntentHelperInstance> intent_helper_remote) {}

void FakeArcBridgeHost::OnKeymasterInstanceReady(
    mojo::PendingRemote<mojom::KeymasterInstance> keymaster_remote) {}

void FakeArcBridgeHost::OnKeyMintInstanceReady(
    mojo::PendingRemote<mojom::keymint::KeyMintInstance> keymint_remote) {}

void FakeArcBridgeHost::OnMediaSessionInstanceReady(
    mojo::PendingRemote<mojom::MediaSessionInstance> media_sesssion_remote) {}

void FakeArcBridgeHost::OnMemoryInstanceReady(
    mojo::PendingRemote<mojom::MemoryInstance> memory_remote) {}

void FakeArcBridgeHost::OnMetricsInstanceReady(
    mojo::PendingRemote<mojom::MetricsInstance> metrics_remote) {}

void FakeArcBridgeHost::OnMidisInstanceReady(
    mojo::PendingRemote<mojom::MidisInstance> midis_remote) {}

void FakeArcBridgeHost::OnNearbyShareInstanceReady(
    mojo::PendingRemote<mojom::NearbyShareInstance> nearby_share_remote) {}

void FakeArcBridgeHost::OnNetInstanceReady(
    mojo::PendingRemote<mojom::NetInstance> net_remote) {}

void FakeArcBridgeHost::OnNotificationsInstanceReady(
    mojo::PendingRemote<mojom::NotificationsInstance> notifications_remote) {}

void FakeArcBridgeHost::OnObbMounterInstanceReady(
    mojo::PendingRemote<mojom::ObbMounterInstance> obb_mounter_remote) {}

void FakeArcBridgeHost::OnOemCryptoInstanceReady(
    mojo::PendingRemote<mojom::OemCryptoInstance> oemcrypto_remote) {}

void FakeArcBridgeHost::OnOnDeviceSafetyInstanceReady(
    mojo::PendingRemote<mojom::OnDeviceSafetyInstance>
        on_device_safety_remote) {}

void FakeArcBridgeHost::OnPaymentAppInstanceReady(
    mojo::PendingRemote<chromeos::payments::mojom::PaymentAppInstance>
        payment_app_remote) {}

void FakeArcBridgeHost::OnPipInstanceReady(
    mojo::PendingRemote<mojom::PipInstance> pip_remote) {}

void FakeArcBridgeHost::OnPolicyInstanceReady(
    mojo::PendingRemote<mojom::PolicyInstance> policy_remote) {}

void FakeArcBridgeHost::OnPowerInstanceReady(
    mojo::PendingRemote<mojom::PowerInstance> power_remote) {}

void FakeArcBridgeHost::OnPrintSpoolerInstanceReady(
    mojo::PendingRemote<mojom::PrintSpoolerInstance> print_spooler_remote) {}

void FakeArcBridgeHost::OnPrivacyItemsInstanceReady(
    mojo::PendingRemote<mojom::PrivacyItemsInstance> privacy_items_remote) {}

void FakeArcBridgeHost::OnProcessInstanceReady(
    mojo::PendingRemote<mojom::ProcessInstance> process_remote) {}

void FakeArcBridgeHost::OnScreenCaptureInstanceReady(
    mojo::PendingRemote<mojom::ScreenCaptureInstance> screen_capture_remote) {}

void FakeArcBridgeHost::OnSharesheetInstanceReady(
    mojo::PendingRemote<mojom::SharesheetInstance> sharesheet_remote) {}

void FakeArcBridgeHost::OnSystemStateInstanceReady(
    mojo::PendingRemote<mojom::SystemStateInstance> system_state_remote) {}

void FakeArcBridgeHost::OnSystemUiInstanceReady(
    mojo::PendingRemote<mojom::SystemUiInstance> system_ui_remote) {}

void FakeArcBridgeHost::OnTimerInstanceReady(
    mojo::PendingRemote<mojom::TimerInstance> timer_remote) {}

void FakeArcBridgeHost::OnTracingInstanceReady(
    mojo::PendingRemote<mojom::TracingInstance> trace_remote) {}

void FakeArcBridgeHost::OnTtsInstanceReady(
    mojo::PendingRemote<mojom::TtsInstance> tts_remote) {}

void FakeArcBridgeHost::OnUsbHostInstanceReady(
    mojo::PendingRemote<mojom::UsbHostInstance> usb_remote) {}

void FakeArcBridgeHost::OnVideoInstanceReady(
    mojo::PendingRemote<mojom::VideoInstance> video_remote) {}

void FakeArcBridgeHost::OnVolumeMounterInstanceReady(
    mojo::PendingRemote<mojom::VolumeMounterInstance> volume_mounter_remote) {}

void FakeArcBridgeHost::OnWakeLockInstanceReady(
    mojo::PendingRemote<mojom::WakeLockInstance> wakelock_remote) {}

void FakeArcBridgeHost::OnWallpaperInstanceReady(
    mojo::PendingRemote<mojom::WallpaperInstance> wallpaper_remote) {}

void FakeArcBridgeHost::OnWebApkInstanceReady(
    mojo::PendingRemote<mojom::WebApkInstance> wallpaper_remote) {}

}  // namespace arc
