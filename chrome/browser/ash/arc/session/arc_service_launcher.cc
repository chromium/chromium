// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_service_launcher.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/apps/app_service/publishers/arc_apps_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_package_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_usb_host_permission_manager.h"
#include "chrome/browser/ash/app_list/arc/arc_usb_host_permission_manager_factory.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler_factory.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/ash/apps/webapk/webapk_manager.h"
#include "chrome/browser/ash/arc/accessibility/arc_accessibility_helper_bridge.h"
#include "chrome/browser/ash/arc/adbd/arc_adbd_monitor_bridge.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/auth/arc_auth_service.h"
#include "chrome/browser/ash/arc/bluetooth/arc_bluetooth_bridge.h"
#include "chrome/browser/ash/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "chrome/browser/ash/arc/enterprise/arc_enterprise_reporting_service.h"
#include "chrome/browser/ash/arc/enterprise/cert_store/cert_store_service_factory.h"
#include "chrome/browser/ash/arc/error_notification/arc_error_notification_bridge.h"
#include "chrome/browser/ash/arc/file_system_watcher/arc_file_system_watcher_service.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root_map_factory.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_bridge.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_mounter.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/ash/arc/idle_manager/arc_idle_manager.h"
#include "chrome/browser/ash/arc/input_method_manager/arc_input_method_manager_service.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_manager.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_instance_throttle.h"
#include "chrome/browser/ash/arc/intent_helper/arc_settings_service.h"
#include "chrome/browser/ash/arc/intent_helper/chrome_arc_intent_helper_delegate.h"
#include "chrome/browser/ash/arc/locked_fullscreen/arc_locked_fullscreen_manager.h"
#include "chrome/browser/ash/arc/metrics/arc_metrics_service_proxy.h"
#include "chrome/browser/ash/arc/nearby_share/arc_nearby_share_bridge.h"
#include "chrome/browser/ash/arc/net/browser_url_opener_impl.h"
#include "chrome/browser/ash/arc/net/cert_manager_impl.h"
#include "chrome/browser/ash/arc/notification/arc_boot_error_notification.h"
#include "chrome/browser/ash/arc/notification/arc_provision_notification_service.h"
#include "chrome/browser/ash/arc/notification/arc_vm_data_migration_notifier.h"
#include "chrome/browser/ash/arc/pip/arc_pip_bridge.h"
#include "chrome/browser/ash/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/ash/arc/print_spooler/arc_print_spooler_bridge.h"
#include "chrome/browser/ash/arc/privacy_items/arc_privacy_items_bridge.h"
#include "chrome/browser/ash/arc/screen_capture/arc_screen_capture_bridge.h"
#include "chrome/browser/ash/arc/session/arc_disk_space_monitor.h"
#include "chrome/browser/ash/arc/session/arc_initial_optin_metrics_recorder_factory.h"
#include "chrome/browser/ash/arc/session/arc_play_store_enabled_preference_handler.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/sharesheet/arc_sharesheet_bridge.h"
#include "chrome/browser/ash/arc/survey/arc_survey_service.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing.h"
#include "chrome/browser/ash/arc/tracing/arc_tracing_bridge.h"
#include "chrome/browser/ash/arc/tts/arc_tts_service.h"
#include "chrome/browser/ash/arc/user_session/arc_user_session_service.h"
#include "chrome/browser/ash/arc/vmm/arc_system_state_bridge.h"
#include "chrome/browser/ash/arc/vmm/arc_vmm_manager.h"
#include "chrome/browser/ash/arc/wallpaper/arc_wallpaper_service.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/channel/channel_info.h"
#include "chromeos/ash/components/memory/swap_configuration.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/experiences/arc/app/arc_app_launch_notifier.h"
#include "chromeos/ash/experiences/arc/appfuse/arc_appfuse_bridge.h"
#include "chromeos/ash/experiences/arc/arc_features.h"
#include "chromeos/ash/experiences/arc/arc_util.h"
#include "chromeos/ash/experiences/arc/audio/arc_audio_bridge.h"
#include "chromeos/ash/experiences/arc/camera/arc_camera_bridge.h"
#include "chromeos/ash/experiences/arc/chrome_feature_flags/arc_chrome_feature_flags_bridge.h"
#include "chromeos/ash/experiences/arc/compat_mode/arc_resize_lock_manager.h"
#include "chromeos/ash/experiences/arc/crash_collector/arc_crash_collector_bridge.h"
#include "chromeos/ash/experiences/arc/disk_space/arc_disk_space_bridge.h"
#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_installer.h"
#include "chromeos/ash/experiences/arc/ime/arc_ime_service.h"
#include "chromeos/ash/experiences/arc/intent_helper/arc_icon_cache_delegate.h"
#include "chromeos/ash/experiences/arc/intent_helper/arc_intent_helper_bridge.h"
#include "chromeos/ash/experiences/arc/keymaster/arc_keymaster_bridge.h"
#include "chromeos/ash/experiences/arc/keymint/arc_keymint_bridge.h"
#include "chromeos/ash/experiences/arc/media_session/arc_media_session_bridge.h"
#include "chromeos/ash/experiences/arc/memory/arc_memory_bridge.h"
#include "chromeos/ash/experiences/arc/metrics/arc_metrics_service.h"
#include "chromeos/ash/experiences/arc/midis/arc_midis_bridge.h"
#include "chromeos/ash/experiences/arc/net/arc_net_host_impl.h"
#include "chromeos/ash/experiences/arc/net/arc_wifi_host_impl.h"
#include "chromeos/ash/experiences/arc/obb_mounter/arc_obb_mounter_bridge.h"
#include "chromeos/ash/experiences/arc/oemcrypto/arc_oemcrypto_bridge.h"
#include "chromeos/ash/experiences/arc/pay/arc_digital_goods_bridge.h"
#include "chromeos/ash/experiences/arc/pay/arc_payment_app_bridge.h"
#include "chromeos/ash/experiences/arc/power/arc_power_bridge.h"
#include "chromeos/ash/experiences/arc/process/arc_process_service.h"
#include "chromeos/ash/experiences/arc/safety/arc_safety_bridge.h"
#include "chromeos/ash/experiences/arc/sensor/arc_iio_sensor_bridge.h"
#include "chromeos/ash/experiences/arc/session/arc_service_manager.h"
#include "chromeos/ash/experiences/arc/session/arc_session.h"
#include "chromeos/ash/experiences/arc/session/arc_session_runner.h"
#include "chromeos/ash/experiences/arc/system_ui/arc_system_ui_bridge.h"
#include "chromeos/ash/experiences/arc/timer/arc_timer_bridge.h"
#include "chromeos/ash/experiences/arc/usb/usb_host_bridge.h"
#include "chromeos/ash/experiences/arc/video/gpu_arc_video_service_host.h"
#include "chromeos/ash/experiences/arc/volume_mounter/arc_volume_mounter_bridge.h"
#include "chromeos/ash/experiences/arc/wake_lock/arc_wake_lock_bridge.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_member.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

#if BUILDFLAG(USE_ARC_PROTECTED_MEDIA)
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/cdm_factory_daemon/cdm_factory_daemon_client.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#else
#include "ash/constants/ash_switches.h"
#include "base/files/file_path.h"
#endif

namespace arc {
namespace {

#if BUILDFLAG(USE_ARC_PROTECTED_MEDIA)
// Delay for repeatedly checking if the TPM is owned or not.
constexpr base::TimeDelta kTpmOwnershipCheckDelay = base::Seconds(5);
// Timeout for waiting for the daemon to become available after we have owned
// the TPM.
constexpr base::TimeDelta kDaemonWaitTimeoutSec = base::Seconds(30);
#endif  // BUILDFLAG(USE_ARC_PROTECTED_MEDIA)

// `ChromeBrowserMainPartsAsh` owns.
ArcServiceLauncher* g_arc_service_launcher = nullptr;
ArcSessionRunner* g_arc_session_runner_for_testing = nullptr;

std::unique_ptr<ArcSessionManager> CreateArcSessionManager(
    ArcBridgeService* arc_bridge_service,
    version_info::Channel channel,
    ash::SchedulerConfigurationManagerBase* scheduler_configuration_manager) {
  auto delegate = std::make_unique<AdbSideloadingAvailabilityDelegateImpl>();
  std::unique_ptr<ArcSessionRunner> runner(
      std::exchange(g_arc_session_runner_for_testing, nullptr));
  if (!runner) {
    runner = std::make_unique<ArcSessionRunner>(
        base::BindRepeating(ArcSession::Create, arc_bridge_service, channel,
                            scheduler_configuration_manager, delegate.get()));
  }
  return std::make_unique<ArcSessionManager>(std::move(runner),
                                             std::move(delegate));
}

#if !BUILDFLAG(USE_ARC_PROTECTED_MEDIA)
void CheckArcvmDlcImageStatus() {
  base::FilePath arc_vm_dlc_image_path(
      "/opt/google/vms/android/system.raw.img");
  // Check if the ARCVM DLC image exists before calling
  // GetArcStatusForProfile(). This blocks the main thread but is necessary to
  // ensure arc availability is consistent, especially during Ash Chrome
  // restarts. The check only occurs when the arcvm_dlc USE flag is enabled,
  // which is currently specific to the Reven board.
  bool is_arcvm_dlc_image_available = base::PathExists(arc_vm_dlc_image_path);
  arc::SetArcvmDlcImageStatus(is_arcvm_dlc_image_available);
}
#endif  //! BUILDFLAG(USE_ARC_PROTECTED_MEDIA)

}  // namespace

ArcServiceLauncher::ArcServiceLauncher(
    ash::SchedulerConfigurationManagerBase* scheduler_configuration_manager)
    : arc_service_manager_(std::make_unique<ArcServiceManager>()),
      arc_session_manager_(
          CreateArcSessionManager(arc_service_manager_->arc_bridge_service(),
                                  ash::GetChannel(),
                                  scheduler_configuration_manager)),
      scheduler_configuration_manager_(scheduler_configuration_manager),
      arc_dlc_installer_(std::make_unique<ArcDlcInstaller>(
          ash::CrosSettings::Get())) {
  DCHECK(g_arc_service_launcher == nullptr);
  g_arc_service_launcher = this;

  if (base::FeatureList::IsEnabled(kEnableVirtioBlkForData) ||
      base::FeatureList::IsEnabled(kEnableArcVmDataMigration)) {
    arc_disk_space_monitor_ = std::make_unique<ArcDiskSpaceMonitor>();
  }
}

ArcServiceLauncher::~ArcServiceLauncher() {
  DCHECK_EQ(g_arc_service_launcher, this);
  g_arc_service_launcher = nullptr;
}

// static
ArcServiceLauncher* ArcServiceLauncher::Get() {
  return g_arc_service_launcher;
}

void ArcServiceLauncher::Initialize() {
#if BUILDFLAG(USE_ARC_PROTECTED_MEDIA)
  // If we have ARC HWDRM then we need to wait for the CdmFactoryDaemon service
  // to advertise avalability so that arc-prepare-host-generated-dir can query
  // it via D-Bus for the CDM client information.
  ash::CdmFactoryDaemonClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&ArcServiceLauncher::OnCdmFactoryDaemonAvailable,
                     weak_factory_.GetWeakPtr(), /*from_timeout=*/false));

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ArcServiceLauncher::OnCheckTpmStatus,
                     weak_factory_.GetWeakPtr()),
      kTpmOwnershipCheckDelay);
#else
  if (arc::IsArcVmDlcEnabled() &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kLoginUser)) {
    CheckArcvmDlcImageStatus();
  }

  if (!arc::IsArcVmDlcEnabled()) {
    arc_session_manager_->ExpandPropertyFilesAndReadSalt();
    return;
  }

  // This should not be called for the board where ARC+DLC is not supported.
  arc_dlc_installer_->PrepareArc(
      base::BindOnce(&ArcServiceLauncher::OnDlcImageBindMountArcPath,
                     weak_factory_.GetWeakPtr()));

#endif  // BUILDFLAG(USE_ARC_PROTECTED_MEDIA)
}

void ArcServiceLauncher::OnDlcImageBindMountArcPath(bool result) {
  if (!result) {
    LOG(ERROR) << "Failed to bind mount the ARC DLC image.";
    return;
  }
  arc_session_manager_->ExpandPropertyFilesAndReadSalt();
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

  // Initialize the locked fullscreen manager with the primary user profile.
  arc_locked_fullscreen_manager_ =
      std::make_unique<ArcLockedFullscreenManager>(profile);

  // We usually want to configure swap exactly once for the session. We wait for
  // after the user profile is prepared to make sure policy has been loaded. The
  // function IsArcPlayStoreEnabledForProfile has many conditionals, but the
  // main one we're checking for is if ARC is disabled by policy (which is the
  // default). Note that there's a known edge case where during a session policy
  // changes from disabled to enabled. This is expected to be rare, and logging
  // out will update the swap configuration. Likewise, if a user enables or
  // disables ARC during a session, the swap configuration will also be updated.
  ash::ConfigureSwap(IsArcPlayStoreEnabledForProfile(profile));

  // Record metrics for ARC status based on device affiliation
  RecordArcStatusBasedOnDeviceAffiliationUMA(profile);

  if (arc_session_manager_->profile() != profile) {
    // Profile is not matched, so the given |profile| is not allowed to use
    // ARC.
    return;
  }

  std::string user_id_hash =
      ash::ProfileHelper::GetUserIdHashFromProfile(profile);

  // Instantiate ARC related BrowserContextKeyedService classes which need
  // to be running at the beginning of the container run.
  // Note that, to keep this list as small as possible, services which don't
  // need to be initialized at the beginning should not be listed here.
  // Those services will be initialized lazily.
  // List in lexicographical order.
  ArcAccessibilityHelperBridge::GetForBrowserContext(profile);
  ArcAdbdMonitorBridge::GetForBrowserContext(profile);
  ArcAppLaunchNotifier::GetForBrowserContext(profile);
  ArcAppPerformanceTracing::GetForBrowserContext(profile);
  ArcAudioBridge::GetForBrowserContext(profile);
  ArcAuthService::GetForBrowserContext(profile);
  ArcBluetoothBridge::GetForBrowserContext(profile);
  ArcBootErrorNotification::GetForBrowserContext(profile);
  ArcBootPhaseMonitorBridge::GetForBrowserContext(profile);
  ArcCameraBridge::GetForBrowserContext(profile);
  ArcCrashCollectorBridge::GetForBrowserContext(profile);
  ArcDigitalGoodsBridge::GetForBrowserContext(profile);
  ArcDiskSpaceBridge::GetForBrowserContext(profile);
  ArcEnterpriseReportingService::GetForBrowserContext(profile);
  ArcErrorNotificationBridge::GetForBrowserContext(profile);
  ArcFileSystemBridge::GetForBrowserContext(profile);
  ArcFileSystemMounter::GetForBrowserContext(profile);
  ArcFileSystemWatcherService::GetForBrowserContext(profile);
  ArcIioSensorBridge::GetForBrowserContext(profile);
  ArcImeService::GetForBrowserContext(profile);
  ArcInputMethodManagerService::GetForBrowserContext(profile);
  input_overlay::ArcInputOverlayManager::GetForBrowserContext(profile);
  ArcInstanceThrottle::GetForBrowserContext(profile);
  {
    auto* intent_helper = ArcIntentHelperBridge::GetForBrowserContext(profile);
    intent_helper->SetDelegate(
        std::make_unique<ChromeArcIntentHelperDelegate>(profile));
    arc_icon_cache_delegate_provider_ =
        std::make_unique<ArcIconCacheDelegateProvider>(intent_helper);
  }
  if (ShouldUseArcKeyMint()) {
    ArcKeyMintBridge::GetForBrowserContext(profile);
  } else {
    ArcKeymasterBridge::GetForBrowserContext(profile);
  }
  ArcMediaSessionBridge::GetForBrowserContext(profile);
  {
    auto* metrics_service = ArcMetricsService::GetForBrowserContext(profile);
    metrics_service->SetHistogramNamerCallback(
        base::BindRepeating([](const std::string& base_name) {
          return GetHistogramNameByUserTypeForPrimaryProfile(base_name);
        }));
    metrics_service->set_user_id_hash(user_id_hash);
  }
  ArcMetricsServiceProxy::GetForBrowserContext(profile);
  ArcMidisBridge::GetForBrowserContext(profile);
  ArcNearbyShareBridge::GetForBrowserContext(profile);
  {
    auto* arc_net_host_impl = ArcNetHostImpl::GetForBrowserContext(profile);
    arc_net_host_impl->SetPrefService(profile->GetPrefs());
    arc_net_host_impl->SetCertManager(
        std::make_unique<CertManagerImpl>(profile));
    arc_net_url_opener_ = std::make_unique<BrowserUrlOpenerImpl>();
  }
  ArcOemCryptoBridge::GetForBrowserContext(profile);
  ArcPaymentAppBridge::GetForBrowserContext(profile);
  ArcPipBridge::GetForBrowserContext(profile);
  ArcPolicyBridge::GetForBrowserContext(profile);
  ArcPowerBridge::GetForBrowserContext(profile)->SetUserIdHash(user_id_hash);
  ArcPrintSpoolerBridge::GetForBrowserContext(profile);
  ArcPrivacyItemsBridge::GetForBrowserContext(profile);
  ArcProcessService::GetForBrowserContext(profile);
  ArcProvisionNotificationService::GetForBrowserContext(profile);
  ArcResizeLockManager::GetForBrowserContext(profile);
  ArcScreenCaptureBridge::GetForBrowserContext(profile);
  ArcSettingsService::GetForBrowserContext(profile);
  ArcSharesheetBridge::GetForBrowserContext(profile);
  ArcSurveyService::GetForBrowserContext(profile);
  ArcSystemUIBridge::GetForBrowserContext(profile);
  ArcTracingBridge::GetForBrowserContext(profile);
  ArcTtsService::GetForBrowserContext(profile);
  ArcUsbHostBridge::GetForBrowserContext(profile);
  ArcUsbHostPermissionManager::GetForBrowserContext(profile);
  ArcUserSessionService::GetForBrowserContext(profile);
  ArcVolumeMounterBridge::GetForBrowserContext(profile);
  ArcWakeLockBridge::GetForBrowserContext(profile);
  ArcWallpaperService::GetForBrowserContext(profile);
  ArcWifiHostImpl::GetForBrowserContext(profile);
  GpuArcVideoKeyedService::GetForBrowserContext(profile);
  CertStoreServiceFactory::GetForBrowserContext(profile);
  apps::ArcAppsFactory::GetForProfile(profile);
  ash::ApkWebAppService::Get(profile);
  ash::app_restore::AppRestoreArcTaskHandlerFactory::GetForProfile(profile);
  ArcInitialOptInMetricsRecorderFactory::GetForBrowserContext(profile);
  ArcChromeFeatureFlagsBridge::GetForBrowserContext(profile);

  if (arc::IsArcVmEnabled()) {
    // ARCVM-only services.
    ArcVmmManager::GetForBrowserContext(profile)->set_user_id_hash(
        user_id_hash);
    ArcSafetyBridge::GetForBrowserContext(profile);
    ArcSystemStateBridge::GetForBrowserContext(profile);

    if (base::FeatureList::IsEnabled(kEnableArcVmDataMigration)) {
      arc_vm_data_migration_notifier_ =
          std::make_unique<ArcVmDataMigrationNotifier>(profile);
    }
    if (base::FeatureList::IsEnabled(kEnableArcIdleManager))
      ArcIdleManager::GetForBrowserContext(profile);
    if (ShouldUseArcKeyMint()) {
      auto serial_number = arc_session_manager_->GetSerialNumberForKeyMint();
      ArcKeyMintBridge::GetForBrowserContext(profile)->SetSerialNumberInKeyMint(
          serial_number);
    }
  } else {
    // ARC Container-only services.
    ArcTimerBridge::GetForBrowserContext(profile);
    ArcAppfuseBridge::GetForBrowserContext(profile);
    ArcObbMounterBridge::GetForBrowserContext(profile);
  }

  if (web_app::AreWebAppsEnabled(profile)) {
    web_apk_manager_ = std::make_unique<apps::WebApkManager>(profile);
  }

  arc_session_manager_->Initialize();
  arc_play_store_enabled_preference_handler_ =
      std::make_unique<ArcPlayStoreEnabledPreferenceHandler>(
          profile, arc_session_manager_.get());
  arc_play_store_enabled_preference_handler_->Start();
}

void ArcServiceLauncher::Shutdown() {
  // Reset browser context registered to ArcServiceManager before profile
  // destruction. This is required to avoid keeping the dangling pointer after
  // profile destruction.
  arc_service_manager_->set_browser_context(nullptr);

  arc_play_store_enabled_preference_handler_.reset();
  arc_session_manager_->Shutdown();
  web_apk_manager_.reset();
  arc_net_url_opener_.reset();
  arc_icon_cache_delegate_provider_.reset();
  arc_dlc_installer_.reset();
}

void ArcServiceLauncher::ResetForTesting() {
  // First destroy the internal states, then re-initialize them.
  // These are for workaround of singletonness DCHECK in their ctors/dtors.
  Shutdown();
  arc_session_manager_.reset();

  // No recreation of arc_service_manager. Pointers to its ArcBridgeService
  // may be referred from existing KeyedService, so destoying it would cause
  // unexpected behavior, specifically on test teardown.
  arc_session_manager_ = CreateArcSessionManager(
      arc_service_manager_->arc_bridge_service(), ash::GetChannel(),
      scheduler_configuration_manager_);

  // Recreate arc_dlc_installer_ after shutdown because browser_test will run
  // ResetForTesting and then do the OnPrimaryUserProfilePrepared.
  arc_dlc_installer_ = std::make_unique<ArcDlcInstaller>(
      ash::CrosSettings::Get());
}

#if BUILDFLAG(USE_ARC_PROTECTED_MEDIA)
void ArcServiceLauncher::OnCdmFactoryDaemonAvailable(
    bool from_timeout,
    bool is_service_available) {
  if (is_service_available && !expanded_property_files_) {
    if (from_timeout) {
      LOG(ERROR)
          << "Timed out waiting for CdmFactoryDaemon service in ARC startup";
    }
    expanded_property_files_ = true;
    arc_session_manager_->ExpandPropertyFilesAndReadSalt();
    weak_factory_.InvalidateWeakPtrs();
  }
}

void ArcServiceLauncher::OnCheckTpmStatus() {
  chromeos::TpmManagerClient::Get()->GetTpmNonsensitiveStatus(
      ::tpm_manager::GetTpmNonsensitiveStatusRequest(),
      base::BindOnce(&ArcServiceLauncher::OnGetTpmStatus,
                     weak_factory_.GetWeakPtr()));
}

void ArcServiceLauncher::OnGetTpmStatus(
    const ::tpm_manager::GetTpmNonsensitiveStatusReply& reply) {
  if (expanded_property_files_)
    return;

  if (reply.status() == ::tpm_manager::TpmManagerStatus::STATUS_SUCCESS &&
      reply.is_owned()) {
    // The TPM is owned, so we should invoke OnCdmFactoryDaemonAvailable() after
    // our timeout so that the property files get expanded even if the daemon
    // doesn't come online.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ArcServiceLauncher::OnCdmFactoryDaemonAvailable,
                       weak_factory_.GetWeakPtr(),
                       /*from_timeout=*/true,
                       /*is_service_available=*/true),
        kDaemonWaitTimeoutSec);
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ArcServiceLauncher::OnCheckTpmStatus,
                     weak_factory_.GetWeakPtr()),
      kTpmOwnershipCheckDelay);
}
#endif  // BUILDFLAG(USE_ARC_PROTECTED_MEDIA)

// static
void ArcServiceLauncher::EnsureFactoriesBuilt() {
  ArcAccessibilityHelperBridge::EnsureFactoryBuilt();
  ArcAdbdMonitorBridge::EnsureFactoryBuilt();
  ArcAppLaunchNotifier::EnsureFactoryBuilt();
  ArcAppPerformanceTracing::EnsureFactoryBuilt();
  ArcAppfuseBridge::EnsureFactoryBuilt();
  ArcAudioBridge::EnsureFactoryBuilt();
  ArcAuthService::EnsureFactoryBuilt();
  ArcBluetoothBridge::EnsureFactoryBuilt();
  ArcBootErrorNotification::EnsureFactoryBuilt();
  ArcBootPhaseMonitorBridgeFactory::GetInstance();
  ArcCameraBridge::EnsureFactoryBuilt();
  ArcCrashCollectorBridge::EnsureFactoryBuilt();
  ArcDigitalGoodsBridge::EnsureFactoryBuilt();
  ArcDiskSpaceBridge::EnsureFactoryBuilt();
  ArcDocumentsProviderRootMapFactory::GetInstance();
  ArcEnterpriseReportingService::EnsureFactoryBuilt();
  ArcErrorNotificationBridge::EnsureFactoryBuilt();
  ArcFileSystemMounter::EnsureFactoryBuilt();
  ArcFileSystemOperationRunner::EnsureFactoryBuilt();
  ArcFileSystemWatcherService::EnsureFactoryBuilt();
  ArcIdleManager::EnsureFactoryBuilt();
  ArcIioSensorBridge::EnsureFactoryBuilt();
  ArcImeService::EnsureFactoryBuilt();
  ArcInitialOptInMetricsRecorderFactory::GetInstance();
  ArcInstanceThrottle::EnsureFactoryBuilt();
  if (ShouldUseArcKeyMint()) {
    ArcKeyMintBridge::EnsureFactoryBuilt();
  } else {
    ArcKeymasterBridge::EnsureFactoryBuilt();
  }
  ArcMediaSessionBridge::EnsureFactoryBuilt();
  ArcMemoryBridge::EnsureFactoryBuilt();
  ArcMetricsServiceFactory::GetInstance();
  ArcMetricsServiceProxy::EnsureFactoryBuilt();
  ArcMidisBridge::EnsureFactoryBuilt();
  ArcNearbyShareBridge::EnsureFactoryBuilt();
  ArcNetHostImpl::EnsureFactoryBuilt();
  ArcObbMounterBridge::EnsureFactoryBuilt();
  ArcOemCryptoBridge::EnsureFactoryBuilt();
  ArcPackageSyncableServiceFactory::GetInstance();
  ArcPaymentAppBridge::EnsureFactoryBuilt();
  ArcPipBridge::EnsureFactoryBuilt();
  ArcPolicyBridge::EnsureFactoryBuilt();
  ArcPowerBridge::EnsureFactoryBuilt();
  ArcPrintSpoolerBridge::EnsureFactoryBuilt();
  ArcPrivacyItemsBridge::EnsureFactoryBuilt();
  ArcProcessService::EnsureFactoryBuilt();
  ArcProvisionNotificationService::EnsureFactoryBuilt();
  ArcResizeLockManager::EnsureFactoryBuilt();
  ArcSafetyBridge::EnsureFactoryBuilt();
  ArcScreenCaptureBridge::EnsureFactoryBuilt();
  ArcSettingsService::EnsureFactoryBuilt();
  ArcSharesheetBridge::EnsureFactoryBuilt();
  ArcSurveyService::EnsureFactoryBuilt();
  ArcSystemUIBridge::EnsureFactoryBuilt();
  ArcSystemStateBridge::EnsureFactoryBuilt();
  ArcTimerBridge::EnsureFactoryBuilt();
  ArcTracingBridge::EnsureFactoryBuilt();
  ArcTtsService::EnsureFactoryBuilt();
  ArcUsbHostBridge::EnsureFactoryBuilt();
  ArcUsbHostPermissionManagerFactory::GetInstance();
  ArcUserSessionService::EnsureFactoryBuilt();
  ArcVmmManager::EnsureFactoryBuilt();
  ArcVolumeMounterBridge::EnsureFactoryBuilt();
  ArcWakeLockBridge::EnsureFactoryBuilt();
  ArcWallpaperService::EnsureFactoryBuilt();
  ArcWifiHostImpl::EnsureFactoryBuilt();
  CertStoreServiceFactory::GetInstance();
  GpuArcVideoKeyedService::EnsureFactoryBuilt();
  input_overlay::ArcInputOverlayManager::EnsureFactoryBuilt();
  ArcChromeFeatureFlagsBridge::EnsureFactoryBuilt();
}

// static
void ArcServiceLauncher::SetArcSessionRunnerForTesting(
    std::unique_ptr<ArcSessionRunner> arc_session_runner) {
  CHECK(!g_arc_session_runner_for_testing);
  CHECK(!g_arc_service_launcher);
  g_arc_session_runner_for_testing = arc_session_runner.release();
}

}  // namespace arc
