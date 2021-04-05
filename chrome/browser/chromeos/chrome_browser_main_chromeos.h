// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_

#include <memory>

#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"
// TODO(https://crbug.com/1164001): forward declare when moved to
// chrome/browser/ash/.
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
// TODO(https://crbug.com/1164001): forward declare when moved to
// chrome/browser/ash/.
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
// TODO(https://crbug.com/1164001): forward declare when moved to
// chrome/browser/ash/.
#include "chrome/browser/ash/notifications/low_disk_notification.h"
// TODO(https://crbug.com/1164001): forward declare when moved to
// chrome/browser/ash/.
#include "chrome/browser/ash/notifications/gnubby_notification.h"
// TODO(https://crbug.com/1164001): forward declare when moved to
// chrome/browser/ash/.
#include "chrome/browser/ash/settings/shutdown_policy_forwarder.h"
// TODO(https://crbug.com/1164001): forward declare when moved to
// chrome/browser/ash/.
#include "chrome/browser/ash/system/breakpad_consent_watcher.h"
#include "chrome/browser/chrome_browser_main_linux.h"
#include "chrome/browser/chromeos/external_metrics.h"
#include "chrome/browser/memory/memory_kills_monitor.h"

class AssistantClientImpl;
class AssistantStateClient;
class ChromeKeyboardControllerClient;
class ImageDownloaderImpl;

namespace arc {
namespace data_snapshotd {
class ArcDataSnapshotdManager;
}  // namespace data_snapshotd

class ArcServiceLauncher;
}  // namespace arc

namespace ash {
class AccessibilityEventRewriterDelegateImpl;
}

namespace crosapi {
class BrowserManager;
class CrosapiManager;
}  // namespace crosapi

namespace crostini {
class CrostiniUnsupportedActionNotifier;
class CrosvmMetrics;
}  // namespace crostini

namespace lock_screen_apps {
class StateController;
}

namespace policy {
class LockToSingleUserManager;
}  // namespace policy

namespace chromeos {

class BulkPrintersCalculatorFactory;
class CrosUsbDetector;
class DemoModeResourcesRemover;
class EventRewriterDelegateImpl;
class FastTransitionObserver;
class IdleActionWarningObserver;
class LoginScreenExtensionsLifetimeManager;
class LoginScreenExtensionsStorageCleaner;
class MemoryAblationStudy;
class NetworkChangeManagerClient;
class NetworkPrefStateObserver;
class NetworkThrottlingObserver;
class PowerMetricsReporter;
class RendererFreezer;
class SessionTerminationManager;
class SystemTokenCertDBInitializer;
class WilcoDtcSupportdManager;

namespace default_app_order {
class ExternalLoader;
}

namespace internal {
class DBusServices;
}  // namespace internal

namespace platform_keys {
class KeyPermissionsManager;
}

namespace power {
class SmartChargingManager;
namespace ml {
class AdaptiveScreenBrightnessManager;
}  // namespace ml

namespace auto_screen_brightness {
class Controller;
}  // namespace auto_screen_brightness
}  // namespace power

namespace system {
class DarkResumeController;
}  // namespace system

// ChromeBrowserMainParts implementation for chromeos specific code.
// NOTE: Chromeos UI (Ash) support should be added to
// ChromeBrowserMainExtraPartsAsh instead. This class should not depend on
// src/ash or chrome/browser/ui/ash.
class ChromeBrowserMainPartsChromeos : public ChromeBrowserMainPartsLinux {
 public:
  ChromeBrowserMainPartsChromeos(const content::MainFunctionParams& parameters,
                                 StartupData* startup_data);
  ~ChromeBrowserMainPartsChromeos() override;

  // ChromeBrowserMainParts overrides.
  int PreEarlyInitialization() override;
  void PreMainMessageLoopStart() override;
  void PostMainMessageLoopStart() override;
  int PreMainMessageLoopRun() override;

  // Stages called from PreMainMessageLoopRun.
  void PreProfileInit() override;
  void PostProfileInit() override;
  void PreBrowserStart() override;
  void PostBrowserStart() override;

  void PostMainMessageLoopRun() override;
  void PostDestroyThreads() override;

 private:
  std::unique_ptr<default_app_order::ExternalLoader> app_order_loader_;
  std::unique_ptr<NetworkPrefStateObserver> network_pref_state_observer_;
  std::unique_ptr<IdleActionWarningObserver> idle_action_warning_observer_;
  std::unique_ptr<RendererFreezer> renderer_freezer_;
  std::unique_ptr<PowerMetricsReporter> power_metrics_reporter_;
  std::unique_ptr<FastTransitionObserver> fast_transition_observer_;
  std::unique_ptr<NetworkThrottlingObserver> network_throttling_observer_;
  std::unique_ptr<NetworkChangeManagerClient> network_change_manager_client_;

  std::unique_ptr<internal::DBusServices> dbus_services_;

  std::unique_ptr<SystemTokenCertDBInitializer>
      system_token_certdb_initializer_;

  std::unique_ptr<ShutdownPolicyForwarder> shutdown_policy_forwarder_;

  std::unique_ptr<EventRewriterDelegateImpl> event_rewriter_delegate_;

  // Handles event dispatch to the accessibility component extensions.
  std::unique_ptr<ash::AccessibilityEventRewriterDelegateImpl>
      accessibility_event_rewriter_delegate_;

  scoped_refptr<chromeos::ExternalMetrics> external_metrics_;

  std::unique_ptr<arc::ArcServiceLauncher> arc_service_launcher_;

  std::unique_ptr<ImageDownloaderImpl> image_downloader_;

  std::unique_ptr<AssistantStateClient> assistant_state_client_;

  std::unique_ptr<AssistantClientImpl> assistant_client_;

  std::unique_ptr<LowDiskNotification> low_disk_notification_;
  std::unique_ptr<ArcKioskAppManager> arc_kiosk_app_manager_;
  std::unique_ptr<WebKioskAppManager> web_kiosk_app_manager_;

  std::unique_ptr<ChromeKeyboardControllerClient>
      chrome_keyboard_controller_client_;

  std::unique_ptr<lock_screen_apps::StateController>
      lock_screen_apps_state_controller_;
  std::unique_ptr<crosapi::CrosapiManager> crosapi_manager_;
  std::unique_ptr<crosapi::BrowserManager> browser_manager_;

  std::unique_ptr<power::SmartChargingManager> smart_charging_manager_;

  std::unique_ptr<power::ml::AdaptiveScreenBrightnessManager>
      adaptive_screen_brightness_manager_;

  std::unique_ptr<power::auto_screen_brightness::Controller>
      auto_screen_brightness_controller_;

  std::unique_ptr<DemoModeResourcesRemover> demo_mode_resources_remover_;
  std::unique_ptr<crostini::CrosvmMetrics> crosvm_metrics_;

  std::unique_ptr<CrosUsbDetector> cros_usb_detector_;

  std::unique_ptr<crostini::CrostiniUnsupportedActionNotifier>
      crostini_unsupported_action_notifier_;

  std::unique_ptr<chromeos::system::DarkResumeController>
      dark_resume_controller_;

  std::unique_ptr<chromeos::BulkPrintersCalculatorFactory>
      bulk_printers_calculator_factory_;

  std::unique_ptr<SessionTerminationManager> session_termination_manager_;

  // Set when PreProfileInit() is called. If PreMainMessageLoopRun() exits
  // early, this will be false during PostMainMessageLoopRun(), etc.
  // Used to prevent shutting down classes that were not initialized.
  bool pre_profile_init_called_ = false;

  std::unique_ptr<policy::LockToSingleUserManager> lock_to_single_user_manager_;
  std::unique_ptr<WilcoDtcSupportdManager> wilco_dtc_supportd_manager_;
  std::unique_ptr<LoginScreenExtensionsLifetimeManager>
      login_screen_extensions_lifetime_manager_;
  std::unique_ptr<LoginScreenExtensionsStorageCleaner>
      login_screen_extensions_storage_cleaner_;

  std::unique_ptr<GnubbyNotification> gnubby_notification_;
  std::unique_ptr<system::BreakpadConsentWatcher> breakpad_consent_watcher_;

  std::unique_ptr<arc::data_snapshotd::ArcDataSnapshotdManager>
      arc_data_snapshotd_manager_;

  std::unique_ptr<platform_keys::KeyPermissionsManager>
      system_token_key_permissions_manager_;
  std::unique_ptr<MemoryAblationStudy> memory_ablation_study_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainPartsChromeos);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_
