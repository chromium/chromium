// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_

#include <memory>

#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/chrome_browser_main_linux.h"
#include "chrome/browser/chromeos/external_metrics.h"
#include "chrome/browser/memory/memory_kills_monitor.h"

class AssistantClient;
class AssistantStateClient;
class ChromeKeyboardControllerClient;
class SpokenFeedbackEventRewriterDelegate;

namespace lock_screen_apps {
class StateController;
}

namespace arc {
class ArcServiceLauncher;
}  // namespace arc

namespace policy {
class LockToSingleUserManager;
}  // namespace policy


namespace crostini {
class CrostiniUnsupportedActionNotifier;
class CrosvmMetrics;
}  // namespace crostini

namespace chromeos {

class ArcKioskAppManager;
class CrosUsbDetector;
class DemoModeResourcesRemover;
class DiscoverManager;
class EventRewriterDelegateImpl;
class FastTransitionObserver;
class GnubbyNotification;
class IdleActionWarningObserver;
class LoginScreenExtensionsLifetimeManager;
class LoginScreenExtensionsStorageCleaner;
class LowDiskNotification;
class NetworkChangeManagerClient;
class NetworkPrefStateObserver;
class NetworkThrottlingObserver;
class PowerMetricsReporter;
class RendererFreezer;
class SchedulerConfigurationManager;
class SessionTerminationManager;
class ShutdownPolicyForwarder;
class SystemTokenCertDBInitializer;
class WakeOnWifiManager;
class WebKioskAppManager;
class WilcoDtcSupportdManager;

namespace default_app_order {
class ExternalLoader;
}

namespace internal {
class DBusServices;
}  // namespace internal

namespace power {
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
  void PreMainMessageLoopRun() override;

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
  std::unique_ptr<WakeOnWifiManager> wake_on_wifi_manager_;
  std::unique_ptr<FastTransitionObserver> fast_transition_observer_;
  std::unique_ptr<NetworkThrottlingObserver> network_throttling_observer_;
  std::unique_ptr<NetworkChangeManagerClient> network_change_manager_client_;

  std::unique_ptr<internal::DBusServices> dbus_services_;

  std::unique_ptr<SystemTokenCertDBInitializer>
      system_token_certdb_initializer_;

  std::unique_ptr<ShutdownPolicyForwarder> shutdown_policy_forwarder_;

  std::unique_ptr<EventRewriterDelegateImpl> event_rewriter_delegate_;

  // Handles event dispatch to the spoken feedback extension (ChromeVox).
  std::unique_ptr<SpokenFeedbackEventRewriterDelegate>
      spoken_feedback_event_rewriter_delegate_;

  scoped_refptr<chromeos::ExternalMetrics> external_metrics_;

  std::unique_ptr<arc::ArcServiceLauncher> arc_service_launcher_;

  std::unique_ptr<AssistantStateClient> assistant_state_client_;

  std::unique_ptr<AssistantClient> assistant_client_;

  std::unique_ptr<LowDiskNotification> low_disk_notification_;
  std::unique_ptr<ArcKioskAppManager> arc_kiosk_app_manager_;
  std::unique_ptr<WebKioskAppManager> web_kiosk_app_manager_;

  std::unique_ptr<memory::MemoryKillsMonitor::Handle> memory_kills_monitor_;

  std::unique_ptr<ChromeKeyboardControllerClient>
      chrome_keyboard_controller_client_;

  std::unique_ptr<lock_screen_apps::StateController>
      lock_screen_apps_state_controller_;

  std::unique_ptr<power::ml::AdaptiveScreenBrightnessManager>
      adaptive_screen_brightness_manager_;

  std::unique_ptr<power::auto_screen_brightness::Controller>
      auto_screen_brightness_controller_;

  std::unique_ptr<DemoModeResourcesRemover> demo_mode_resources_remover_;
  std::unique_ptr<crostini::CrosvmMetrics> crosvm_metrics_;
  std::unique_ptr<DiscoverManager> discover_manager_;
  std::unique_ptr<SchedulerConfigurationManager>
      scheduler_configuration_manager_;

  std::unique_ptr<CrosUsbDetector> cros_usb_detector_;

  std::unique_ptr<crostini::CrostiniUnsupportedActionNotifier>
      crostini_unsupported_action_notifier_;

  std::unique_ptr<chromeos::system::DarkResumeController>
      dark_resume_controller_;

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

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainPartsChromeos);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_
