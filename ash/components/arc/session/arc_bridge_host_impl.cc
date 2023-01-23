// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_bridge_host_impl.h"

#include <utility>

#include "ash/components/arc/mojom/accessibility_helper.mojom.h"
#include "ash/components/arc/mojom/adbd.mojom.h"
#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/mojom/app_permissions.mojom.h"
#include "ash/components/arc/mojom/appfuse.mojom.h"
#include "ash/components/arc/mojom/audio.mojom.h"
#include "ash/components/arc/mojom/auth.mojom.h"
#include "ash/components/arc/mojom/backup_settings.mojom.h"
#include "ash/components/arc/mojom/bluetooth.mojom.h"
#include "ash/components/arc/mojom/boot_phase_monitor.mojom.h"
#include "ash/components/arc/mojom/camera.mojom.h"
#include "ash/components/arc/mojom/clipboard.mojom.h"
#include "ash/components/arc/mojom/compatibility_mode.mojom.h"
#include "ash/components/arc/mojom/crash_collector.mojom.h"
#include "ash/components/arc/mojom/digital_goods.mojom.h"
#include "ash/components/arc/mojom/disk_quota.mojom.h"
#include "ash/components/arc/mojom/enterprise_reporting.mojom.h"
#include "ash/components/arc/mojom/file_system.mojom.h"
#include "ash/components/arc/mojom/iio_sensor.mojom.h"
#include "ash/components/arc/mojom/ime.mojom.h"
#include "ash/components/arc/mojom/input_method_manager.mojom.h"
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/components/arc/mojom/keyboard_shortcut.mojom.h"
#include "ash/components/arc/mojom/keymaster.mojom.h"
#include "ash/components/arc/mojom/kiosk.mojom.h"
#include "ash/components/arc/mojom/lock_screen.mojom.h"
#include "ash/components/arc/mojom/media_session.mojom.h"
#include "ash/components/arc/mojom/memory.mojom.h"
#include "ash/components/arc/mojom/metrics.mojom.h"
#include "ash/components/arc/mojom/midis.mojom.h"
#include "ash/components/arc/mojom/nearby_share.mojom.h"
#include "ash/components/arc/mojom/net.mojom.h"
#include "ash/components/arc/mojom/notifications.mojom.h"
#include "ash/components/arc/mojom/obb_mounter.mojom.h"
#include "ash/components/arc/mojom/oemcrypto.mojom.h"
#include "ash/components/arc/mojom/payment_app.mojom.h"
#include "ash/components/arc/mojom/pip.mojom.h"
#include "ash/components/arc/mojom/policy.mojom.h"
#include "ash/components/arc/mojom/power.mojom.h"
#include "ash/components/arc/mojom/print_spooler.mojom.h"
#include "ash/components/arc/mojom/privacy_items.mojom.h"
#include "ash/components/arc/mojom/process.mojom.h"
#include "ash/components/arc/mojom/property.mojom.h"
#include "ash/components/arc/mojom/rotation_lock.mojom.h"
#include "ash/components/arc/mojom/screen_capture.mojom.h"
#include "ash/components/arc/mojom/sharesheet.mojom.h"
#include "ash/components/arc/mojom/storage_manager.mojom.h"
#include "ash/components/arc/mojom/system_ui.mojom.h"
#include "ash/components/arc/mojom/timer.mojom.h"
#include "ash/components/arc/mojom/tracing.mojom.h"
#include "ash/components/arc/mojom/tts.mojom.h"
#include "ash/components/arc/mojom/usb_host.mojom.h"
#include "ash/components/arc/mojom/video.mojom.h"
#include "ash/components/arc/mojom/volume_mounter.mojom.h"
#include "ash/components/arc/mojom/wake_lock.mojom.h"
#include "ash/components/arc/mojom/wallpaper.mojom.h"
#include "ash/components/arc/mojom/webapk.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/mojo_channel.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_manager.h"
#include "ash/public/cpp/message_center/arc_notifications_host_initializer.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "chromeos/components/sensors/mojom/cros_sensor_service.mojom.h"

namespace arc {

ArcBridgeHostImpl::ArcBridgeHostImpl(
    ArcBridgeService* arc_bridge_service,
    mojo::PendingReceiver<mojom::ArcBridgeHost> pending_receiver)
    : arc_bridge_service_(arc_bridge_service),
      receiver_(this, std::move(pending_receiver)) {
  DCHECK(arc_bridge_service_);
  receiver_.set_disconnect_handler(
      base::BindOnce(&ArcBridgeHostImpl::OnClosed, base::Unretained(this)));
}

ArcBridgeHostImpl::~ArcBridgeHostImpl() {
  OnClosed();
}

void ArcBridgeHostImpl::OnAccessibilityHelperInstanceReady(
    mojo::PendingRemote<mojom::AccessibilityHelperInstance>
        accessibility_helper_remote) {
  OnInstanceReady(arc_bridge_service_->accessibility_helper(),
                  std::move(accessibility_helper_remote));
}

void ArcBridgeHostImpl::OnAdbdMonitorInstanceReady(
    mojo::PendingRemote<mojom::AdbdMonitorInstance> adbd_monitor_remote) {
  OnInstanceReady(arc_bridge_service_->adbd_monitor(),
                  std::move(adbd_monitor_remote));
}

void ArcBridgeHostImpl::OnAppInstanceReady(
    mojo::PendingRemote<mojom::AppInstance> app_remote) {
  OnInstanceReady(arc_bridge_service_->app(), std::move(app_remote));
}

void ArcBridgeHostImpl::OnAppPermissionsInstanceReady(
    mojo::PendingRemote<mojom::AppPermissionsInstance> app_permissions_remote) {
  OnInstanceReady(arc_bridge_service_->app_permissions(),
                  std::move(app_permissions_remote));
}

void ArcBridgeHostImpl::OnAppfuseInstanceReady(
    mojo::PendingRemote<mojom::AppfuseInstance> appfuse_remote) {
  OnInstanceReady(arc_bridge_service_->appfuse(), std::move(appfuse_remote));
}

void ArcBridgeHostImpl::OnAudioInstanceReady(
    mojo::PendingRemote<mojom::AudioInstance> audio_remote) {
  OnInstanceReady(arc_bridge_service_->audio(), std::move(audio_remote));
}

void ArcBridgeHostImpl::OnAuthInstanceReady(
    mojo::PendingRemote<mojom::AuthInstance> auth_remote) {
  OnInstanceReady(arc_bridge_service_->auth(), std::move(auth_remote));
}

void ArcBridgeHostImpl::OnBackupSettingsInstanceReady(
    mojo::PendingRemote<mojom::BackupSettingsInstance> backup_settings_remote) {
  OnInstanceReady(arc_bridge_service_->backup_settings(),
                  std::move(backup_settings_remote));
}

void ArcBridgeHostImpl::OnBluetoothInstanceReady(
    mojo::PendingRemote<mojom::BluetoothInstance> bluetooth_remote) {
  OnInstanceReady(arc_bridge_service_->bluetooth(),
                  std::move(bluetooth_remote));
}

void ArcBridgeHostImpl::OnBootPhaseMonitorInstanceReady(
    mojo::PendingRemote<mojom::BootPhaseMonitorInstance>
        boot_phase_monitor_remote) {
  OnInstanceReady(arc_bridge_service_->boot_phase_monitor(),
                  std::move(boot_phase_monitor_remote));
}

void ArcBridgeHostImpl::OnCameraInstanceReady(
    mojo::PendingRemote<mojom::CameraInstance> camera_remote) {
  OnInstanceReady(arc_bridge_service_->camera(), std::move(camera_remote));
}

void ArcBridgeHostImpl::OnClipboardInstanceReady(
    mojo::PendingRemote<mojom::ClipboardInstance> clipboard_remote) {
  OnInstanceReady(arc_bridge_service_->clipboard(),
                  std::move(clipboard_remote));
}

void ArcBridgeHostImpl::OnCompatibilityModeInstanceReady(
    mojo::PendingRemote<mojom::CompatibilityModeInstance>
        compatibility_mode_remote) {
  OnInstanceReady(arc_bridge_service_->compatibility_mode(),
                  std::move(compatibility_mode_remote));
}

void ArcBridgeHostImpl::OnCrashCollectorInstanceReady(
    mojo::PendingRemote<mojom::CrashCollectorInstance> crash_collector_remote) {
  OnInstanceReady(arc_bridge_service_->crash_collector(),
                  std::move(crash_collector_remote));
}

void ArcBridgeHostImpl::OnDigitalGoodsInstanceReady(
    mojo::PendingRemote<mojom::DigitalGoodsInstance> digital_goods_remote) {
  OnInstanceReady(arc_bridge_service_->digital_goods(),
                  std::move(digital_goods_remote));
}

void ArcBridgeHostImpl::OnDiskQuotaInstanceReady(
    mojo::PendingRemote<mojom::DiskQuotaInstance> disk_quota_remote) {
  OnInstanceReady(arc_bridge_service_->disk_quota(),
                  std::move(disk_quota_remote));
}

void ArcBridgeHostImpl::OnEnterpriseReportingInstanceReady(
    mojo::PendingRemote<mojom::EnterpriseReportingInstance>
        enterprise_reporting_remote) {
  OnInstanceReady(arc_bridge_service_->enterprise_reporting(),
                  std::move(enterprise_reporting_remote));
}

void ArcBridgeHostImpl::OnFileSystemInstanceReady(
    mojo::PendingRemote<mojom::FileSystemInstance> file_system_remote) {
  OnInstanceReady(arc_bridge_service_->file_system(),
                  std::move(file_system_remote));
}

void ArcBridgeHostImpl::OnImeInstanceReady(
    mojo::PendingRemote<mojom::ImeInstance> ime_remote) {
  OnInstanceReady(arc_bridge_service_->ime(), std::move(ime_remote));
}

void ArcBridgeHostImpl::OnIioSensorInstanceReady(
    mojo::PendingRemote<mojom::IioSensorInstance> iio_sensor_remote) {
  OnInstanceReady(arc_bridge_service_->iio_sensor(),
                  std::move(iio_sensor_remote));
}

void ArcBridgeHostImpl::OnInputMethodManagerInstanceReady(
    mojo::PendingRemote<mojom::InputMethodManagerInstance>
        input_method_manager_remote) {
  OnInstanceReady(arc_bridge_service_->input_method_manager(),
                  std::move(input_method_manager_remote));
}

void ArcBridgeHostImpl::OnIntentHelperInstanceReady(
    mojo::PendingRemote<mojom::IntentHelperInstance> intent_helper_remote) {
  OnInstanceReady(arc_bridge_service_->intent_helper(),
                  std::move(intent_helper_remote));
}

void ArcBridgeHostImpl::OnKeyboardShortcutInstanceReady(
    mojo::PendingRemote<mojom::KeyboardShortcutInstance>
        keyboard_shortcut_remote) {
  OnInstanceReady(arc_bridge_service_->keyboard_shortcut(),
                  std::move(keyboard_shortcut_remote));
}

void ArcBridgeHostImpl::OnKeymasterInstanceReady(
    mojo::PendingRemote<mojom::KeymasterInstance> keymaster_remote) {
  OnInstanceReady(arc_bridge_service_->keymaster(),
                  std::move(keymaster_remote));
}

void ArcBridgeHostImpl::OnKioskInstanceReady(
    mojo::PendingRemote<mojom::KioskInstance> kiosk_remote) {
  OnInstanceReady(arc_bridge_service_->kiosk(), std::move(kiosk_remote));
}

void ArcBridgeHostImpl::OnLockScreenInstanceReady(
    mojo::PendingRemote<mojom::LockScreenInstance> lock_screen_remote) {
  OnInstanceReady(arc_bridge_service_->lock_screen(),
                  std::move(lock_screen_remote));
}

void ArcBridgeHostImpl::OnMediaSessionInstanceReady(
    mojo::PendingRemote<mojom::MediaSessionInstance> media_session_remote) {
  OnInstanceReady(arc_bridge_service_->media_session(),
                  std::move(media_session_remote));
}

void ArcBridgeHostImpl::OnMemoryInstanceReady(
    mojo::PendingRemote<mojom::MemoryInstance> memory_remote) {
  OnInstanceReady(arc_bridge_service_->memory(), std::move(memory_remote));
}

void ArcBridgeHostImpl::OnMetricsInstanceReady(
    mojo::PendingRemote<mojom::MetricsInstance> metrics_remote) {
  OnInstanceReady(arc_bridge_service_->metrics(), std::move(metrics_remote));
}

void ArcBridgeHostImpl::OnMidisInstanceReady(
    mojo::PendingRemote<mojom::MidisInstance> midis_remote) {
  OnInstanceReady(arc_bridge_service_->midis(), std::move(midis_remote));
}

void ArcBridgeHostImpl::OnNearbyShareInstanceReady(
    mojo::PendingRemote<mojom::NearbyShareInstance> nearby_share_remote) {
  OnInstanceReady(arc_bridge_service_->nearby_share(),
                  std::move(nearby_share_remote));
}

void ArcBridgeHostImpl::OnNetInstanceReady(
    mojo::PendingRemote<mojom::NetInstance> net_remote) {
  OnInstanceReady(arc_bridge_service_->net(), std::move(net_remote));
}

void ArcBridgeHostImpl::OnNotificationsInstanceReady(
    mojo::PendingRemote<mojom::NotificationsInstance> notifications_remote) {
  auto* host_initializer = ash::ArcNotificationsHostInitializer::Get();
  auto* manager = host_initializer->GetArcNotificationManagerInstance();
  if (manager) {
    static_cast<ash::ArcNotificationManager*>(manager)->SetInstance(
        std::move(notifications_remote));
    return;
  }
  // Forward notification instance to ash by injecting ArcNotificationManager.
  auto new_manager = std::make_unique<ash::ArcNotificationManager>();
  new_manager->SetInstance(std::move(notifications_remote));
  ash::ArcNotificationsHostInitializer::Get()
      ->SetArcNotificationManagerInstance(std::move(new_manager));
}

void ArcBridgeHostImpl::OnObbMounterInstanceReady(
    mojo::PendingRemote<mojom::ObbMounterInstance> obb_mounter_remote) {
  OnInstanceReady(arc_bridge_service_->obb_mounter(),
                  std::move(obb_mounter_remote));
}

void ArcBridgeHostImpl::OnOemCryptoInstanceReady(
    mojo::PendingRemote<mojom::OemCryptoInstance> oemcrypto_remote) {
  OnInstanceReady(arc_bridge_service_->oemcrypto(),
                  std::move(oemcrypto_remote));
}

void ArcBridgeHostImpl::OnPaymentAppInstanceReady(
    mojo::PendingRemote<mojom::PaymentAppInstance> payment_app_remote) {
  OnInstanceReady(arc_bridge_service_->payment_app(),
                  std::move(payment_app_remote));
}

void ArcBridgeHostImpl::OnPipInstanceReady(
    mojo::PendingRemote<mojom::PipInstance> pip_remote) {
  OnInstanceReady(arc_bridge_service_->pip(), std::move(pip_remote));
}

void ArcBridgeHostImpl::OnPolicyInstanceReady(
    mojo::PendingRemote<mojom::PolicyInstance> policy_remote) {
  OnInstanceReady(arc_bridge_service_->policy(), std::move(policy_remote));
}

void ArcBridgeHostImpl::OnPowerInstanceReady(
    mojo::PendingRemote<mojom::PowerInstance> power_remote) {
  OnInstanceReady(arc_bridge_service_->power(), std::move(power_remote));
}

void ArcBridgeHostImpl::OnPrintSpoolerInstanceReady(
    mojo::PendingRemote<mojom::PrintSpoolerInstance> print_spooler_remote) {
  OnInstanceReady(arc_bridge_service_->print_spooler(),
                  std::move(print_spooler_remote));
}

void ArcBridgeHostImpl::OnPrivacyItemsInstanceReady(
    mojo::PendingRemote<mojom::PrivacyItemsInstance> privacy_items_remote) {
  OnInstanceReady(arc_bridge_service_->privacy_items(),
                  std::move(privacy_items_remote));
}

void ArcBridgeHostImpl::OnProcessInstanceReady(
    mojo::PendingRemote<mojom::ProcessInstance> process_remote) {
  OnInstanceReady(arc_bridge_service_->process(), std::move(process_remote));
}

void ArcBridgeHostImpl::OnPropertyInstanceReady(
    mojo::PendingRemote<mojom::PropertyInstance> property_remote) {
  OnInstanceReady(arc_bridge_service_->property(), std::move(property_remote));
}

void ArcBridgeHostImpl::OnRotationLockInstanceReady(
    mojo::PendingRemote<mojom::RotationLockInstance> rotation_lock_remote) {
  OnInstanceReady(arc_bridge_service_->rotation_lock(),
                  std::move(rotation_lock_remote));
}

void ArcBridgeHostImpl::OnScreenCaptureInstanceReady(
    mojo::PendingRemote<mojom::ScreenCaptureInstance> screen_capture_remote) {
  OnInstanceReady(arc_bridge_service_->screen_capture(),
                  std::move(screen_capture_remote));
}

void ArcBridgeHostImpl::OnSharesheetInstanceReady(
    mojo::PendingRemote<mojom::SharesheetInstance> sharesheet_remote) {
  OnInstanceReady(arc_bridge_service_->sharesheet(),
                  std::move(sharesheet_remote));
}

void ArcBridgeHostImpl::OnStorageManagerInstanceReady(
    mojo::PendingRemote<mojom::StorageManagerInstance> storage_manager_remote) {
  OnInstanceReady(arc_bridge_service_->storage_manager(),
                  std::move(storage_manager_remote));
}

void ArcBridgeHostImpl::OnSystemUiInstanceReady(
    mojo::PendingRemote<mojom::SystemUiInstance> system_ui_remote) {
  OnInstanceReady(arc_bridge_service_->system_ui(),
                  std::move(system_ui_remote));
}

void ArcBridgeHostImpl::OnTimerInstanceReady(
    mojo::PendingRemote<mojom::TimerInstance> timer_remote) {
  OnInstanceReady(arc_bridge_service_->timer(), std::move(timer_remote));
}

void ArcBridgeHostImpl::OnTracingInstanceReady(
    mojo::PendingRemote<mojom::TracingInstance> tracing_remote) {
  OnInstanceReady(arc_bridge_service_->tracing(), std::move(tracing_remote));
}

void ArcBridgeHostImpl::OnTtsInstanceReady(
    mojo::PendingRemote<mojom::TtsInstance> tts_remote) {
  OnInstanceReady(arc_bridge_service_->tts(), std::move(tts_remote));
}

void ArcBridgeHostImpl::OnUsbHostInstanceReady(
    mojo::PendingRemote<mojom::UsbHostInstance> usb_host_remote) {
  OnInstanceReady(arc_bridge_service_->usb_host(), std::move(usb_host_remote));
}

void ArcBridgeHostImpl::OnVideoInstanceReady(
    mojo::PendingRemote<mojom::VideoInstance> video_remote) {
  OnInstanceReady(arc_bridge_service_->video(), std::move(video_remote));
}

void ArcBridgeHostImpl::OnVolumeMounterInstanceReady(
    mojo::PendingRemote<mojom::VolumeMounterInstance> volume_mounter_remote) {
  OnInstanceReady(arc_bridge_service_->volume_mounter(),
                  std::move(volume_mounter_remote));
}

void ArcBridgeHostImpl::OnWakeLockInstanceReady(
    mojo::PendingRemote<mojom::WakeLockInstance> wakelock_remote) {
  OnInstanceReady(arc_bridge_service_->wake_lock(), std::move(wakelock_remote));
}

void ArcBridgeHostImpl::OnWallpaperInstanceReady(
    mojo::PendingRemote<mojom::WallpaperInstance> wallpaper_remote) {
  OnInstanceReady(arc_bridge_service_->wallpaper(),
                  std::move(wallpaper_remote));
}

void ArcBridgeHostImpl::OnWebApkInstanceReady(
    mojo::PendingRemote<mojom::WebApkInstance> webapk_remote) {
  OnInstanceReady(arc_bridge_service_->webapk(), std::move(webapk_remote));
}

size_t ArcBridgeHostImpl::GetNumMojoChannelsForTesting() const {
  return mojo_channels_.size();
}

void ArcBridgeHostImpl::OnClosed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(1) << "Mojo connection lost";

  arc_bridge_service_->ObserveBeforeArcBridgeClosed();

  // Close all mojo channels.
  mojo_channels_.clear();
  receiver_.reset();

  arc_bridge_service_->ObserveAfterArcBridgeClosed();
}

template <typename InstanceType, typename HostType>
void ArcBridgeHostImpl::OnInstanceReady(
    ConnectionHolder<InstanceType, HostType>* holder,
    mojo::PendingRemote<InstanceType> remote) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(receiver_.is_bound());
  DCHECK(remote.is_valid());

  // Track |channel|'s lifetime via |mojo_channels_| so that it will be
  // closed on ArcBridgeHost/Instance closing or the ArcBridgeHostImpl's
  // destruction.
  auto* channel =
      new MojoChannel<InstanceType, HostType>(holder, std::move(remote));
  mojo_channels_.emplace_back(channel);

  // Since |channel| is managed by |mojo_channels_|, its lifetime is shorter
  // than |this|. Thus, the connection error handler will be invoked only
  // when |this| is alive and base::Unretained is safe here.
  channel->set_disconnect_handler(base::BindOnce(
      &ArcBridgeHostImpl::OnChannelClosed, base::Unretained(this), channel));

  // Call QueryVersion so that the version info is properly stored in the
  // InterfacePtr<T>.
  channel->QueryVersion();
}

void ArcBridgeHostImpl::OnChannelClosed(MojoChannelBase* channel) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  mojo_channels_.erase(base::ranges::find(
      mojo_channels_, channel, &std::unique_ptr<MojoChannelBase>::get));
}

}  // namespace arc
