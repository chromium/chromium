// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/session/arc_service_launcher.h"

#include <utility>

#include "ash/public/cpp/default_scale_factor_retriever.h"
#include "ash/public/mojom/constants.mojom.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "chrome/browser/apps/app_service/arc_apps_factory.h"
#include "chrome/browser/chromeos/apps/apk_web_app_service.h"
#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_helper_bridge.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/auth/arc_auth_service.h"
#include "chrome/browser/chromeos/arc/bluetooth/arc_bluetooth_bridge.h"
#include "chrome/browser/chromeos/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "chrome/browser/chromeos/arc/cast_receiver/arc_cast_receiver_service.h"
#include "chrome/browser/chromeos/arc/enterprise/arc_enterprise_reporting_service.h"
#include "chrome/browser/chromeos/arc/enterprise/cert_store/arc_cert_store_bridge.h"
#include "chrome/browser/chromeos/arc/enterprise/cert_store/arc_smart_card_manager_bridge.h"
#include "chrome/browser/chromeos/arc/file_system_watcher/arc_file_system_watcher_service.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_bridge.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_mounter.h"
#include "chrome/browser/chromeos/arc/input_method_manager/arc_input_method_manager_service.h"
#include "chrome/browser/chromeos/arc/instance_throttle/arc_instance_throttle.h"
#include "chrome/browser/chromeos/arc/intent_helper/arc_settings_service.h"
#include "chrome/browser/chromeos/arc/kiosk/arc_kiosk_bridge.h"
#include "chrome/browser/chromeos/arc/metrics/arc_metrics_service_proxy.h"
#include "chrome/browser/chromeos/arc/notification/arc_boot_error_notification.h"
#include "chrome/browser/chromeos/arc/notification/arc_provision_notification_service.h"
#include "chrome/browser/chromeos/arc/oemcrypto/arc_oemcrypto_bridge.h"
#include "chrome/browser/chromeos/arc/pip/arc_pip_bridge.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/chromeos/arc/print/arc_print_service.h"
#include "chrome/browser/chromeos/arc/print_spooler/arc_print_spooler_bridge.h"
#include "chrome/browser/chromeos/arc/process/arc_process_service.h"
#include "chrome/browser/chromeos/arc/screen_capture/arc_screen_capture_bridge.h"
#include "chrome/browser/chromeos/arc/session/arc_play_store_enabled_preference_handler.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/tracing/arc_app_performance_tracing.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_bridge.h"
#include "chrome/browser/chromeos/arc/tts/arc_tts_service.h"
#include "chrome/browser/chromeos/arc/usb/arc_usb_host_bridge_delegate.h"
#include "chrome/browser/chromeos/arc/user_session/arc_user_session_service.h"
#include "chrome/browser/chromeos/arc/video/gpu_arc_video_service_host.h"
#include "chrome/browser/chromeos/arc/wallpaper/arc_wallpaper_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_usb_host_permission_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/common/channel_info.h"
#include "components/arc/app_permissions/arc_app_permissions_bridge.h"
#include "components/arc/appfuse/arc_appfuse_bridge.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/audio/arc_audio_bridge.h"
#include "components/arc/camera/arc_camera_bridge.h"
#include "components/arc/clipboard/arc_clipboard_bridge.h"
#include "components/arc/crash_collector/arc_crash_collector_bridge.h"
#include "components/arc/disk_quota/arc_disk_quota_bridge.h"
#include "components/arc/ime/arc_ime_service.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/keymaster/arc_keymaster_bridge.h"
#include "components/arc/lock_screen/arc_lock_screen_bridge.h"
#include "components/arc/media_session/arc_media_session_bridge.h"
#include "components/arc/metrics/arc_metrics_service.h"
#include "components/arc/midis/arc_midis_bridge.h"
#include "components/arc/net/arc_net_host_impl.h"
#include "components/arc/obb_mounter/arc_obb_mounter_bridge.h"
#include "components/arc/power/arc_power_bridge.h"
#include "components/arc/property/arc_property_bridge.h"
#include "components/arc/rotation_lock/arc_rotation_lock_bridge.h"
#include "components/arc/session/arc_session.h"
#include "components/arc/session/arc_session_runner.h"
#include "components/arc/storage_manager/arc_storage_manager.h"
#include "components/arc/timer/arc_timer_bridge.h"
#include "components/arc/usb/usb_host_bridge.h"
#include "components/arc/volume_mounter/arc_volume_mounter_bridge.h"
#include "components/arc/wake_lock/arc_wake_lock_bridge.h"
#include "components/prefs/pref_member.h"
#include "content/public/browser/system_connector.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/service_manager/public/cpp/connector.h"

namespace arc {
namespace {

// The file that specifies if the environment is ARCVM or ARC. It contains 1 in
// ARCVM and 0 in ARC.
constexpr base::FilePath::CharType kIsArcVm[] = "/run/chrome/is_arcvm";

// ChromeBrowserMainPartsChromeos owns.
ArcServiceLauncher* g_arc_service_launcher = nullptr;

}  // namespace

ArcServiceLauncher::ArcServiceLauncher(
    chromeos::SchedulerConfigurationManagerBase*
        scheduler_configuration_manager)
    : arc_service_manager_(std::make_unique<ArcServiceManager>()),
      arc_session_manager_(std::make_unique<ArcSessionManager>(
          std::make_unique<ArcSessionRunner>(
              base::BindRepeating(ArcSession::Create,
                                  arc_service_manager_->arc_bridge_service(),
                                  &default_scale_factor_retriever_,
                                  chrome::GetChannel(),
                                  scheduler_configuration_manager)))),
      scheduler_configuration_manager_(scheduler_configuration_manager) {
  DCHECK(g_arc_service_launcher == nullptr);
  g_arc_service_launcher = this;

  // Write kIsArcVm file to be 1 or 0.
  base::PostTask(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce([](const base::FilePath& filename, const char* data,
                        int size) { base::WriteFile(filename, data, size); },
                     base::FilePath(kIsArcVm),
                     arc::IsArcVmEnabled() ? "1" : "0", 1));

  mojo::PendingRemote<ash::mojom::CrosDisplayConfigController>
      cros_display_config;
  content::GetSystemConnector()->Connect(
      ash::mojom::kServiceName,
      cros_display_config.InitWithNewPipeAndPassReceiver());
  default_scale_factor_retriever_.Start(std::move(cros_display_config));
}

ArcServiceLauncher::~ArcServiceLauncher() {
  DCHECK_EQ(g_arc_service_launcher, this);
  g_arc_service_launcher = nullptr;
}

// static
ArcServiceLauncher* ArcServiceLauncher::Get() {
  return g_arc_service_launcher;
}

void ArcServiceLauncher::MaybeSetProfile(Profile* profile) {
  if (!IsArcAllowedForProfile(profile))
    return;

  // Do not expect it in real use case, but it is used for testing.
  // Because the ArcService instances tied to the old profile is kept,
  // and ones tied to the new profile are added, which is unexpected situation.
  // For compatibility, call Shutdown() here in case |profile| is not
  // allowed for ARC.
  // TODO(hidehiko): DCHECK(!arc_session_manager_->IsAllowed()) here, and
  // get rid of Shutdown().
  if (arc_session_manager_->profile())
    arc_session_manager_->Shutdown();

  arc_session_manager_->SetProfile(profile);
  arc_service_manager_->set_browser_context(profile);
  arc_service_manager_->set_account_id(
      multi_user_util::GetAccountIdFromProfile(profile));
}

void ArcServiceLauncher::OnPrimaryUserProfilePrepared(Profile* profile) {
  DCHECK(arc_service_manager_);
  DCHECK(arc_session_manager_);

  if (arc_session_manager_->profile() != profile) {
    // Profile is not matched, so the given |profile| is not allowed to use
    // ARC.
    return;
  }

  // Instantiate ARC related BrowserContextKeyedService classes which need
  // to be running at the beginning of the container run.
  // Note that, to keep this list as small as possible, services which don't
  // need to be initialized at the beginning should not be listed here.
  // Those services will be initialized lazily.
  // List in lexicographical order.
  ArcAccessibilityHelperBridge::GetForBrowserContext(profile);
  ArcAppPermissionsBridge::GetForBrowserContext(profile);
  ArcAudioBridge::GetForBrowserContext(profile);
  ArcAuthService::GetForBrowserContext(profile);
  ArcBluetoothBridge::GetForBrowserContext(profile);
  ArcBootErrorNotification::GetForBrowserContext(profile);
  ArcBootPhaseMonitorBridge::GetForBrowserContext(profile);
  ArcCameraBridge::GetForBrowserContext(profile);
  ArcCastReceiverService::GetForBrowserContext(profile);
  ArcCertStoreBridge::GetForBrowserContext(profile);
  ArcClipboardBridge::GetForBrowserContext(profile);
  ArcCrashCollectorBridge::GetForBrowserContext(profile);
  ArcDiskQuotaBridge::GetForBrowserContext(profile);
  ArcEnterpriseReportingService::GetForBrowserContext(profile);
  ArcFileSystemBridge::GetForBrowserContext(profile);
  ArcFileSystemMounter::GetForBrowserContext(profile);
  ArcFileSystemWatcherService::GetForBrowserContext(profile);
  ArcImeService::GetForBrowserContext(profile);
  ArcInputMethodManagerService::GetForBrowserContext(profile);
  ArcInstanceThrottle::GetForBrowserContext(profile);
  ArcIntentHelperBridge::GetForBrowserContext(profile);
  ArcKeymasterBridge::GetForBrowserContext(profile);
  ArcKioskBridge::GetForBrowserContext(profile);
  ArcLockScreenBridge::GetForBrowserContext(profile);
  ArcMediaSessionBridge::GetForBrowserContext(profile);
  ArcMetricsService::GetForBrowserContext(profile);
  ArcMetricsServiceProxy::GetForBrowserContext(profile);
  ArcMidisBridge::GetForBrowserContext(profile);
  ArcNetHostImpl::GetForBrowserContext(profile)->SetPrefService(
      profile->GetPrefs());
  ArcOemCryptoBridge::GetForBrowserContext(profile);
  ArcPipBridge::GetForBrowserContext(profile);
  ArcPolicyBridge::GetForBrowserContext(profile);
  ArcPowerBridge::GetForBrowserContext(profile);
  ArcPrintService::GetForBrowserContext(profile);
  ArcPrintSpoolerBridge::GetForBrowserContext(profile);
  ArcProcessService::GetForBrowserContext(profile);
  ArcPropertyBridge::GetForBrowserContext(profile);
  ArcProvisionNotificationService::GetForBrowserContext(profile);
  ArcRotationLockBridge::GetForBrowserContext(profile);
  ArcScreenCaptureBridge::GetForBrowserContext(profile);
  ArcSettingsService::GetForBrowserContext(profile);
  ArcSmartCardManagerBridge::GetForBrowserContext(profile);
  ArcTimerBridge::GetForBrowserContext(profile);
  ArcTracingBridge::GetForBrowserContext(profile);
  ArcAppPerformanceTracing::GetForBrowserContext(profile);
  ArcTtsService::GetForBrowserContext(profile);
  ArcUsbHostBridge::GetForBrowserContext(profile)->SetDelegate(
      std::make_unique<ArcUsbHostBridgeDelegate>());
  ArcUsbHostPermissionManager::GetForBrowserContext(profile);
  ArcUserSessionService::GetForBrowserContext(profile);
  ArcVolumeMounterBridge::GetForBrowserContext(profile);
  ArcWakeLockBridge::GetForBrowserContext(profile);
  ArcWallpaperService::GetForBrowserContext(profile);
  GpuArcVideoServiceHost::GetForBrowserContext(profile);
  if (apps::ArcAppsFactory::IsEnabled()) {
    apps::ArcAppsFactory::GetForProfile(profile);
  }
  chromeos::ApkWebAppService::Get(profile);

  // ARC Container-only services.
  if (!arc::IsArcVmEnabled()) {
    ArcAppfuseBridge::GetForBrowserContext(profile);
    ArcObbMounterBridge::GetForBrowserContext(profile);
  }

  arc_session_manager_->Initialize();
  arc_play_store_enabled_preference_handler_ =
      std::make_unique<ArcPlayStoreEnabledPreferenceHandler>(
          profile, arc_session_manager_.get());
  arc_play_store_enabled_preference_handler_->Start();
}

void ArcServiceLauncher::Shutdown() {
  arc_play_store_enabled_preference_handler_.reset();
  arc_session_manager_->Shutdown();
}

void ArcServiceLauncher::ResetForTesting() {
  // First destroy the internal states, then re-initialize them.
  // These are for workaround of singletonness DCHECK in their ctors/dtors.
  Shutdown();
  arc_session_manager_.reset();

  // No recreation of arc_service_manager. Pointers to its ArcBridgeService
  // may be referred from existing KeyedService, so destoying it would cause
  // unexpected behavior, specifically on test teardown.
  arc_session_manager_ = std::make_unique<ArcSessionManager>(
      std::make_unique<ArcSessionRunner>(base::BindRepeating(
          ArcSession::Create, arc_service_manager_->arc_bridge_service(),
          &default_scale_factor_retriever_, chrome::GetChannel(),
          scheduler_configuration_manager_)));
}

}  // namespace arc
