// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// When each service is created, we set a flag indicating this. At this point,
// the service initialization could fail or succeed. This allows us to remember
// if we tried to create a service, and not try creating it over and over if
// the creation failed.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_IMPL_H_
#define CHROME_BROWSER_BROWSER_PROCESS_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/debug/stack_trace.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/buildflags.h"
#include "components/keep_alive_registry/keep_alive_state_observer.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/nacl/common/buildflags.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "services/network/public/mojom/network_service.mojom-forward.h"

class BatteryMetrics;
class ChromeFeatureListCreator;
class ChromeMetricsServicesManagerClient;
class DevToolsAutoOpener;
class RemoteDebuggingServer;
class PrefRegistrySimple;
class SecureOriginPrefsObserver;
class SiteIsolationPrefsObserver;
class SystemNotificationHelper;
class StartupData;

#if BUILDFLAG(ENABLE_PLUGINS)
class PluginsResourceService;
#endif

namespace base {
class CommandLine;
}

namespace extensions {
class ExtensionsBrowserClient;
}

namespace gcm {
class GCMDriver;
}

namespace policy {
class ChromeBrowserPolicyConnector;
class PolicyService;
}  // namespace policy

namespace webrtc_event_logging {
class WebRtcEventLogManager;
}  // namespace webrtc_event_logging

// Real implementation of BrowserProcess that creates and returns the services.
class BrowserProcessImpl : public BrowserProcess,
                           public KeepAliveStateObserver {
 public:
  // |startup_data| should not be null. The BrowserProcessImpl
  // will take the PrefService owned by the creator as the Local State instead
  // of loading the JSON file from disk.
  explicit BrowserProcessImpl(StartupData* startup_data);
  ~BrowserProcessImpl() override;

  // Called to complete initialization.
  void Init();

#if !defined(OS_ANDROID)
  // Sets a closure to be run to break out of a run loop on browser shutdown
  // (when the KeepAlive count reaches zero).
  // TODO(https://crbug.com/845966): This is also used on macOS for the Cocoa
  // first run dialog so that shutdown can be initiated via a signal while the
  // first run dialog is showing.
  void SetQuitClosure(base::OnceClosure quit_closure);
#endif

#if defined(OS_MACOSX)
  // Clears the quit closure. Shutdown will not be initiated should the
  // KeepAlive count reach zero. This function may be called more than once.
  // TODO(https://crbug.com/845966): Remove this once the Cocoa first run
  // dialog no longer needs it.
  void ClearQuitClosure();
#endif

  // Called before the browser threads are created.
  void PreCreateThreads(const base::CommandLine& command_line);

  // Called after the threads have been created but before the message loops
  // starts running. Allows the browser process to do any initialization that
  // requires all threads running.
  void PreMainMessageLoopRun();

  // Most cleanup is done by these functions, driven from
  // ChromeBrowserMain based on notifications from the content
  // framework, rather than in the destructor, so that we can
  // interleave cleanup with threads being stopped.
#if !defined(OS_ANDROID)
  void StartTearDown();
  void PostDestroyThreads();
#endif

  // Sets |metrics_services_manager_| and |metrics_services_manager_client_|
  // which is owned by it.
  void SetMetricsServices(
      std::unique_ptr<metrics_services_manager::MetricsServicesManager> manager,
      metrics_services_manager::MetricsServicesManagerClient* client);

  // BrowserProcess implementation.
  void EndSession() override;
  void FlushLocalStateAndReply(base::OnceClosure reply) override;
  metrics_services_manager::MetricsServicesManager* GetMetricsServicesManager()
      override;
  metrics::MetricsService* metrics_service() override;
  rappor::RapporServiceImpl* rappor_service() override;
  // TODO(qinmin): Remove this method as callers can retrieve the global
  // instance from SystemNetworkContextManager directly.
  SystemNetworkContextManager* system_network_context_manager() override;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory()
      override;
  network::NetworkQualityTracker* network_quality_tracker() override;
  WatchDogThread* watchdog_thread() override;
  ProfileManager* profile_manager() override;
  PrefService* local_state() override;
  variations::VariationsService* variations_service() override;
  BrowserProcessPlatformPart* platform_part() override;
  extensions::EventRouterForwarder* extension_event_router_forwarder() override;
  NotificationUIManager* notification_ui_manager() override;
  NotificationPlatformBridge* notification_platform_bridge() override;
  policy::ChromeBrowserPolicyConnector* browser_policy_connector() override;
  policy::PolicyService* policy_service() override;
  IconManager* icon_manager() override;
  GpuModeManager* gpu_mode_manager() override;
  void CreateDevToolsProtocolHandler() override;
  void CreateDevToolsAutoOpener() override;
  bool IsShuttingDown() override;
  printing::PrintJobManager* print_job_manager() override;
  printing::PrintPreviewDialogController* print_preview_dialog_controller()
      override;
  printing::BackgroundPrintingManager* background_printing_manager() override;
  IntranetRedirectDetector* intranet_redirect_detector() override;
  const std::string& GetApplicationLocale() override;
  void SetApplicationLocale(const std::string& actual_locale) override;
  DownloadStatusUpdater* download_status_updater() override;
  DownloadRequestLimiter* download_request_limiter() override;
  BackgroundModeManager* background_mode_manager() override;
  void set_background_mode_manager_for_test(
      std::unique_ptr<BackgroundModeManager> manager) override;
  StatusTray* status_tray() override;
  safe_browsing::SafeBrowsingService* safe_browsing_service() override;
  safe_browsing::ClientSideDetectionService* safe_browsing_detection_service()
      override;
  subresource_filter::RulesetService* subresource_filter_ruleset_service()
      override;
  optimization_guide::OptimizationGuideService* optimization_guide_service()
      override;

  StartupData* startup_data() override;

#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  void StartAutoupdateTimer() override;
#endif

  component_updater::ComponentUpdateService* component_updater() override;
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  component_updater::SupervisedUserWhitelistInstaller*
  supervised_user_whitelist_installer() override;
#endif
  MediaFileSystemRegistry* media_file_system_registry() override;
  WebRtcLogUploader* webrtc_log_uploader() override;
  network_time::NetworkTimeTracker* network_time_tracker() override;
  gcm::GCMDriver* gcm_driver() override;
  resource_coordinator::TabManager* GetTabManager() override;
  resource_coordinator::ResourceCoordinatorParts* resource_coordinator_parts()
      override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  using WebRtcEventLogManager = webrtc_event_logging::WebRtcEventLogManager;

  // KeepAliveStateObserver implementation
  void OnKeepAliveStateChanged(bool is_keeping_alive) override;
  void OnKeepAliveRestartStateChanged(bool can_restart) override;

  // Create network quality observer so that it can propagate network quality
  // changes to the render process.
  void CreateNetworkQualityObserver();

  void CreateWatchdogThread();
  void CreateProfileManager();
  void CreateLocalState();
  void CreateIconManager();
  void CreateIntranetRedirectDetector();
  void CreateNotificationPlatformBridge();
  void CreateNotificationUIManager();
  void CreatePrintPreviewDialogController();
  void CreateBackgroundPrintingManager();
  void CreateSafeBrowsingService();
  void CreateSubresourceFilterRulesetService();
  void CreateOptimizationGuideService();
  void CreateStatusTray();
  void CreateBackgroundModeManager();
  void CreateGCMDriver();
  void CreatePhysicalWebDataSource();

  void ApplyDefaultBrowserPolicy();

  // Methods called to control our lifetime. The browser process can be "pinned"
  // to make sure it keeps running.
  void Pin();
  void Unpin();

  // |metrics_services_manager_| owns this.
  ChromeMetricsServicesManagerClient* metrics_services_manager_client_ =
      nullptr;

  std::unique_ptr<metrics_services_manager::MetricsServicesManager>
      metrics_services_manager_;

  bool created_watchdog_thread_ = false;
  std::unique_ptr<WatchDogThread> watchdog_thread_;

  bool created_browser_policy_connector_ = false;
  // Must be destroyed after |local_state_|.
  std::unique_ptr<policy::ChromeBrowserPolicyConnector>
      browser_policy_connector_;

  bool created_profile_manager_ = false;
  std::unique_ptr<ProfileManager> profile_manager_;

  std::unique_ptr<PrefService> local_state_;

  std::unique_ptr<network::NetworkQualityTracker> network_quality_tracker_;

  // Listens to NetworkQualityTracker and sends network quality updates to the
  // renderer.
  std::unique_ptr<
      network::NetworkQualityTracker::RTTAndThroughputEstimatesObserver>
      network_quality_observer_;

  bool created_icon_manager_ = false;
  std::unique_ptr<IconManager> icon_manager_;

  std::unique_ptr<GpuModeManager> gpu_mode_manager_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::unique_ptr<extensions::ExtensionsBrowserClient>
      extensions_browser_client_;

  scoped_refptr<extensions::EventRouterForwarder>
      extension_event_router_forwarder_;

  std::unique_ptr<MediaFileSystemRegistry> media_file_system_registry_;
#endif

#if !defined(OS_ANDROID)
  std::unique_ptr<RemoteDebuggingServer> remote_debugging_server_;
  std::unique_ptr<DevToolsAutoOpener> devtools_auto_opener_;
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  scoped_refptr<printing::PrintPreviewDialogController>
      print_preview_dialog_controller_;

  std::unique_ptr<printing::BackgroundPrintingManager>
      background_printing_manager_;
#endif

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  // Manager for desktop notification UI.
  bool created_notification_ui_manager_ = false;
  std::unique_ptr<NotificationUIManager> notification_ui_manager_;
#endif

  std::unique_ptr<IntranetRedirectDetector> intranet_redirect_detector_;

  std::unique_ptr<StatusTray> status_tray_;

#if BUILDFLAG(ENABLE_NATIVE_NOTIFICATIONS)
  bool created_notification_bridge_ = false;
#endif

  std::unique_ptr<NotificationPlatformBridge> notification_bridge_;

  // Use SystemNotificationHelper::GetInstance to get this instance.
  std::unique_ptr<SystemNotificationHelper> system_notification_helper_;

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  std::unique_ptr<BackgroundModeManager> background_mode_manager_;
#endif

  bool created_safe_browsing_service_ = false;
  scoped_refptr<safe_browsing::SafeBrowsingService> safe_browsing_service_;

  bool created_subresource_filter_ruleset_service_ = false;
  std::unique_ptr<subresource_filter::RulesetService>
      subresource_filter_ruleset_service_;

  bool created_optimization_guide_service_ = false;
  std::unique_ptr<optimization_guide::OptimizationGuideService>
      optimization_guide_service_;

  bool shutting_down_ = false;

  bool tearing_down_ = false;

  // Ensures that all the print jobs are finished before closing the browser.
  std::unique_ptr<printing::PrintJobManager> print_job_manager_;

  std::string locale_;

  // Download status updates (like a changing application icon on dock/taskbar)
  // are global per-application. DownloadStatusUpdater does no work in the ctor
  // so we don't have to worry about lazy initialization.
  std::unique_ptr<DownloadStatusUpdater> download_status_updater_;

  scoped_refptr<DownloadRequestLimiter> download_request_limiter_;

  // If non-null, this object holds a pref store that will be taken by
  // BrowserProcessImpl to create the |local_state_|.
  ChromeFeatureListCreator* chrome_feature_list_creator_;

  StartupData* startup_data_;

  // Ensures that the observers of plugin/print disable/enable state
  // notifications are properly added and removed.
  PrefChangeRegistrar pref_change_registrar_;

  std::unique_ptr<BatteryMetrics> battery_metrics_;

#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  base::RepeatingTimer autoupdate_timer_;

  // Gets called by autoupdate timer to see if browser needs restart and can be
  // restarted, and if that's the case, restarts the browser.
  void OnAutoupdateTimer();
  bool IsRunningInBackground() const;
  void OnPendingRestartResult(bool is_update_pending_restart);
  void RestartBackgroundInstance();
#endif  // defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))

  // component updater is normally not used under ChromeOS due
  // to concerns over integrity of data shared between profiles,
  // but some users of component updater only install per-user.
  std::unique_ptr<component_updater::ComponentUpdateService> component_updater_;

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  std::unique_ptr<component_updater::SupervisedUserWhitelistInstaller>
      supervised_user_whitelist_installer_;
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  std::unique_ptr<PluginsResourceService> plugins_resource_service_;
#endif

  std::unique_ptr<BrowserProcessPlatformPart> platform_part_;

  // TODO(eroman): Remove this when done debugging 113031. This tracks
  // the callstack which released the final module reference count.
  base::debug::StackTrace release_last_reference_callstack_;

  // Lazily initialized.
  std::unique_ptr<WebRtcLogUploader> webrtc_log_uploader_;

  // WebRtcEventLogManager is a singleton which is instaniated before anything
  // that needs it, and lives until ~BrowserProcessImpl(). This allows it to
  // safely post base::Unretained(this) references to an internally owned task
  // queue, since after ~BrowserProcessImpl(), those tasks would no longer run.
  std::unique_ptr<WebRtcEventLogManager> webrtc_event_log_manager_;

  std::unique_ptr<network_time::NetworkTimeTracker> network_time_tracker_;

  std::unique_ptr<gcm::GCMDriver> gcm_driver_;

  std::unique_ptr<resource_coordinator::ResourceCoordinatorParts>
      resource_coordinator_parts_;

  std::unique_ptr<SecureOriginPrefsObserver> secure_origin_prefs_observer_;
  std::unique_ptr<SiteIsolationPrefsObserver> site_isolation_prefs_observer_;

#if !defined(OS_ANDROID)
  // Called to signal the process' main message loop to exit.
  base::OnceClosure quit_closure_;
#endif

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(BrowserProcessImpl);
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_IMPL_H_
