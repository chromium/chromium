// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAIN_PARTS_CHROME_BROWSER_MAIN_PARTS_ASH_H_
#define CHROME_BROWSER_ASH_MAIN_PARTS_CHROME_BROWSER_MAIN_PARTS_ASH_H_

#include <memory>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/ash/external_metrics/external_metrics.h"
#include "chrome/browser/ash/pcie_peripheral/ash_usb_detector.h"
#include "chrome/browser/ash/performance/doze_mode_power_status_scheduler.h"
#include "chrome/browser/chrome_browser_main_linux.h"
#include "chrome/browser/memory/memory_kills_monitor.h"

class AmbientClientImpl;
class AssistantBrowserDelegateImpl;
class AssistantStateClient;
class ChromeKeyboardControllerClient;
class ImageDownloaderImpl;
class LobsterClientFactoryImpl;

namespace arc {
class ArcServiceLauncher;
class ContainerAppKiller;
}  // namespace arc

namespace chromeos {
class MahiWebContentsManager;

namespace default_app_order {
class ExternalLoader;
}  // namespace default_app_order

}  // namespace chromeos

namespace crosapi {
class BrowserManager;
class CrosapiManager;
class LacrosAvailabilityPolicyObserver;
}  // namespace crosapi

namespace crostini {
class CrostiniUnsupportedActionNotifier;
}  // namespace crostini

namespace lock_screen_apps {
class StateController;
}

namespace policy {
class LockToSingleUserManager;
}  // namespace policy

namespace video_conference {
class VideoConferenceManagerClientImpl;
}  // namespace video_conference

namespace ash {

class AccessibilityEventRewriterDelegateImpl;
class ApnMigrator;
class AudioSurveyHandler;
class BluetoothLogController;
class BluetoothPrefStateObserver;
class BulkPrintersCalculatorFactory;
class CameraGeneralSurveyHandler;
class ChromeAuthParts;
class CrosUsbDetector;
class DebugdNotificationHandler;
class EventRewriterDelegateImpl;
class FastTransitionObserver;
class FwupdDownloadClientImpl;
class GnubbyNotification;
class HatsBluetoothRevampTriggerImpl;
class IdleActionWarningObserver;
class KioskController;
class LoginScreenExtensionsStorageCleaner;
class LowDiskNotification;
class AuthEventsRecorder;
class MultiCaptureNotifications;
class NetworkChangeManagerClient;
class NetworkPrefStateObserver;
class NetworkThrottlingObserver;
class MisconfiguredUserCleaner;
class PowerMetricsReporter;
class RendererFreezer;
class ReportControllerInitializer;
class SessionTerminationManager;
class ShortcutMappingPrefService;
class ShutdownPolicyForwarder;
class SigninProfileHandler;
class SuspendPerfReporter;
class SystemTokenCertDBInitializer;
class VideoConferenceAppServiceClient;
class VideoConferenceAshFeatureClient;
class DozeModePowerStatusScheduler;

namespace carrier_lock {
class CarrierLockManager;
}

namespace cros_healthd::internal {
class DataCollector;
}

namespace file_manager {
class FileIndexServiceRegistry;
}

namespace internal {
class DBusServices;
}

namespace platform_keys {
class KeyPermissionsManager;
}

namespace power {
class SmartChargingManager;
namespace auto_screen_brightness {
class Controller;
}
namespace ml {
class AdaptiveScreenBrightnessManager;
}
}  // namespace power

namespace quick_pair {
class QuickPairBrowserDelegateImpl;
}

namespace system {
class DarkResumeController;
}  // namespace system

// ChromeBrowserMainParts implementation for chromeos specific code.
// NOTE: Chromeos UI (Ash) support should be added to
// ChromeBrowserMainExtraPartsAsh instead. This class should not depend on
// src/ash or chrome/browser/ui/ash.
class ChromeBrowserMainPartsAsh : public ChromeBrowserMainPartsLinux {
 public:
  ChromeBrowserMainPartsAsh(bool is_integration_test,
                            StartupData* startup_data);

  ChromeBrowserMainPartsAsh(const ChromeBrowserMainPartsAsh&) = delete;
  ChromeBrowserMainPartsAsh& operator=(const ChromeBrowserMainPartsAsh&) =
      delete;

  ~ChromeBrowserMainPartsAsh() override;

  // ChromeBrowserMainParts overrides.
  int PreEarlyInitialization() override;
  void PreCreateMainMessageLoop() override;
  void PostCreateMainMessageLoop() override;
  int PreMainMessageLoopRun() override;

  // Stages called from PreMainMessageLoopRun.
  void PreProfileInit() override;
  void PostProfileInit(Profile* profile, bool is_initial_profile) override;
  void PreBrowserStart() override;
  void PostBrowserStart() override;

  void PostMainMessageLoopRun() override;
  void PostDestroyThreads() override;

 private:
  std::unique_ptr<chromeos::default_app_order::ExternalLoader>
      app_order_loader_;
  std::unique_ptr<NetworkPrefStateObserver> network_pref_state_observer_;
  std::unique_ptr<BluetoothPrefStateObserver> bluetooth_pref_state_observer_;
  std::unique_ptr<BluetoothLogController> bluetooth_log_controller_;
  std::unique_ptr<IdleActionWarningObserver> idle_action_warning_observer_;
  std::unique_ptr<RendererFreezer> renderer_freezer_;
  std::unique_ptr<PowerMetricsReporter> power_metrics_reporter_;
  std::unique_ptr<SuspendPerfReporter> suspend_perf_reporter_;
  std::unique_ptr<FastTransitionObserver> fast_transition_observer_;
  std::unique_ptr<NetworkThrottlingObserver> network_throttling_observer_;
  std::unique_ptr<NetworkChangeManagerClient> network_change_manager_client_;
  std::unique_ptr<LobsterClientFactoryImpl> lobster_client_factory_;
  std::unique_ptr<DebugdNotificationHandler> debugd_notification_handler_;
  std::unique_ptr<HatsBluetoothRevampTriggerImpl>
      hats_bluetooth_revamp_trigger_;

  std::unique_ptr<::ash::file_manager::FileIndexServiceRegistry>
      file_index_service_registry_;

  std::unique_ptr<internal::DBusServices> dbus_services_;

  base::ScopedClosureRunner mojo_service_manager_closer_;

  std::unique_ptr<SystemTokenCertDBInitializer>
      system_token_certdb_initializer_;

  std::unique_ptr<ShutdownPolicyForwarder> shutdown_policy_forwarder_;

  std::unique_ptr<EventRewriterDelegateImpl> event_rewriter_delegate_;

  std::unique_ptr<carrier_lock::CarrierLockManager> carrier_lock_manager_;

  // Handles event dispatch to the accessibility component extensions.
  std::unique_ptr<AccessibilityEventRewriterDelegateImpl>
      accessibility_event_rewriter_delegate_;

  scoped_refptr<ExternalMetrics> external_metrics_;

  std::unique_ptr<DozeModePowerStatusScheduler>
      doze_mode_power_status_scheduler_;

  std::unique_ptr<arc::ArcServiceLauncher> arc_service_launcher_;

  std::unique_ptr<ImageDownloaderImpl> image_downloader_;

  std::unique_ptr<AssistantStateClient> assistant_state_client_;

  std::unique_ptr<AssistantBrowserDelegateImpl> assistant_delegate_;

  std::unique_ptr<LowDiskNotification> low_disk_notification_;
  std::unique_ptr<KioskController> kiosk_controller_;
  std::unique_ptr<AmbientClientImpl> ambient_client_;
  std::unique_ptr<MultiCaptureNotifications> multi_capture_notifications_;

  std::unique_ptr<ShortcutMappingPrefService> shortcut_mapping_pref_service_;
  std::unique_ptr<ChromeKeyboardControllerClient>
      chrome_keyboard_controller_client_;

  std::unique_ptr<lock_screen_apps::StateController>
      lock_screen_apps_state_controller_;
  std::unique_ptr<crosapi::CrosapiManager> crosapi_manager_;
  std::unique_ptr<crosapi::BrowserManager> browser_manager_;
  std::unique_ptr<crosapi::LacrosAvailabilityPolicyObserver>
      lacros_availability_policy_observer_;

  std::unique_ptr<VideoConferenceAppServiceClient> vc_app_service_client_;
  std::unique_ptr<VideoConferenceAshFeatureClient> vc_ash_feature_client_;

  std::unique_ptr<power::SmartChargingManager> smart_charging_manager_;

  std::unique_ptr<power::ml::AdaptiveScreenBrightnessManager>
      adaptive_screen_brightness_manager_;

  std::unique_ptr<power::auto_screen_brightness::Controller>
      auto_screen_brightness_controller_;

  std::unique_ptr<AshUsbDetector> ash_usb_detector_;
  std::unique_ptr<CrosUsbDetector> cros_usb_detector_;

  std::unique_ptr<ReportControllerInitializer> report_controller_initializer_;

  std::unique_ptr<crostini::CrostiniUnsupportedActionNotifier>
      crostini_unsupported_action_notifier_;

  std::unique_ptr<system::DarkResumeController> dark_resume_controller_;

  std::unique_ptr<BulkPrintersCalculatorFactory>
      bulk_printers_calculator_factory_;

  std::unique_ptr<FwupdDownloadClientImpl> fwupd_download_client_;

  std::unique_ptr<SessionTerminationManager> session_termination_manager_;

  std::unique_ptr<cros_healthd::internal::DataCollector>
      cros_healthd_data_collector_;

  std::unique_ptr<chromeos::MahiWebContentsManager> mahi_web_contents_manager_;

  // Set when PreProfileInit() is called. If PreMainMessageLoopRun() exits
  // early, this will be false during PostMainMessageLoopRun(), etc.
  // Used to prevent shutting down classes that were not initialized.
  bool pre_profile_init_called_ = false;
  std::unique_ptr<SigninProfileHandler> signin_profile_handler_;

  std::unique_ptr<policy::LockToSingleUserManager> lock_to_single_user_manager_;
  std::unique_ptr<LoginScreenExtensionsStorageCleaner>
      login_screen_extensions_storage_cleaner_;

  std::unique_ptr<GnubbyNotification> gnubby_notification_;

  std::unique_ptr<platform_keys::KeyPermissionsManager>
      system_token_key_permissions_manager_;

  std::unique_ptr<quick_pair::QuickPairBrowserDelegateImpl>
      quick_pair_delegate_;

  std::unique_ptr<AudioSurveyHandler> audio_survey_handler_;

  std::unique_ptr<CameraGeneralSurveyHandler> camera_general_survey_handler_;

  std::unique_ptr<ApnMigrator> apn_migrator_;

  std::unique_ptr<arc::ContainerAppKiller> arc_container_app_killer_;

  // Only temporarily owned, will be null after PostCreateMainMessageLoop().
  // The Accessor is constructed before initialization of FeatureList and should
  // only be used by ChromeFeaturesServiceProvider.
  std::unique_ptr<base::FeatureList::Accessor> feature_list_accessor_;

  std::unique_ptr<ash::AuthEventsRecorder> auth_events_recorder_;
  std::unique_ptr<ash::ChromeAuthParts> auth_parts_;

  std::unique_ptr<video_conference::VideoConferenceManagerClientImpl>
      video_conference_manager_client_;

  std::unique_ptr<MisconfiguredUserCleaner> misconfigured_user_cleaner_;

  base::WeakPtrFactory<ChromeBrowserMainPartsAsh> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAIN_PARTS_CHROME_BROWSER_MAIN_PARTS_ASH_H_
