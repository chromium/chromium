// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_impl.h"

#include <stddef.h>
#include <stdio.h>

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "base/atomic_ref_count.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/accessibility/soda_installer_impl.h"
#include "chrome/browser/battery/battery_metrics.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/component_updater/chrome_component_updater_configurator.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/devtools/remote_debugging_server.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/download/download_status_updater.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/gpu/gpu_mode_manager.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/lifetime/switch_utils.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager.h"
#include "chrome/browser/media/webrtc/webrtc_log_uploader.h"
#include "chrome/browser/metrics/chrome_feature_list_creator.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/permissions/chrome_permissions_client.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/resource_coordinator/resource_coordinator_parts.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/site_isolation/prefs_observer.h"
#include "chrome/browser/ssl/secure_origin_prefs_observer.h"
#include "chrome/browser/startup_data.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/update_client/chrome_update_query_params_delegate.h"
#include "chrome/browser/webapps/chrome_webapps_client.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/breadcrumbs/core/application_breadcrumbs_logger.h"
#include "components/breadcrumbs/core/breadcrumb_persistent_storage_manager.h"
#include "components/breadcrumbs/core/breadcrumb_persistent_storage_util.h"
#include "components/breadcrumbs/core/breadcrumbs_status.h"
#include "components/breadcrumbs/core/crash_reporter_breadcrumb_observer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/timer_update_scheduler.h"
#include "components/crash/core/common/crash_key.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/language/core/browser/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/metrics_services_manager/metrics_services_manager_client.h"
#include "components/network_time/network_time_tracker.h"
#include "components/permissions/permissions_client.h"
#include "components/policy/core/common/policy_service.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/safe_browsing_service_interface.h"
#include "components/sessions/core/session_id_generator.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/ukm/ukm_service.h"
#include "components/update_client/update_query_params.h"
#include "components/variations/service/variations_service.h"
#include "components/web_resource/web_resource_pref_names.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/network_quality_observer_factory.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/process_visibility_util.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/network_service_util.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/log/net_log.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "ui/base/idle/idle.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#elif BUILDFLAG(IS_MAC)
#include "chrome/browser/chrome_browser_main_mac.h"
#include "chrome/browser/media/webrtc/system_media_capture_permissions_stats_mac.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/message_center/message_center.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/soda/soda_installer_impl_chromeos.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ssl/chrome_security_state_client.h"
#include "chrome/browser/webauthn/android/chrome_webauthn_client_android.h"
#include "components/webauthn/android/webauthn_client_android.h"
#else
#include "chrome/browser/devtools/devtools_auto_opener.h"
#include "chrome/browser/gcm/gcm_product_util.h"
#include "chrome/browser/hid/hid_policy_allowed_devices.h"
#include "chrome/browser/hid/hid_system_tray_icon.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/serial/serial_policy_allowed_ports.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_desktop_utils.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#endif

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
#include "chrome/browser/background/background_mode_manager.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/apps/platform_apps/chrome_apps_browser_api_provider.h"
#include "chrome/browser/extensions/chrome_extensions_browser_client.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/ui/apps/chrome_app_window_client.h"
#include "chrome/common/extensions/chrome_extensions_client.h"
#include "chrome/common/initialize_extensions_client.h"
#include "components/storage_monitor/storage_monitor.h"
#include "extensions/common/extension_l10n_util.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "content/public/browser/plugin_service.h"
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/exit_type_service.h"
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/first_run/upgrade_util.h"
#include "chrome/browser/ui/profile_picker.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/error_reporting/chrome_js_error_report_processor.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_CHROME_NOTIFICATIONS)
#include "chrome/browser/notifications/notification_ui_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/extensions/telemetry/chromeos_telemetry_extensions_browser_api_provider.h"
#include "chrome/browser/hid/hid_pinned_notification.h"
#elif !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/hid/hid_status_icon.h"
#endif

#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
// How often to check if the persistent instance of Chrome needs to restart
// to install an update.
static const int kUpdateCheckIntervalHours = 6;
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_OZONE)
// How long to wait for the File thread to complete during EndSession, on Linux
// and Windows. We have a timeout here because we're unable to run the UI
// messageloop and there's some deadlock risk. Our only option is to exit
// anyway.
static constexpr base::TimeDelta kEndSessionTimeout = base::Seconds(10);
#endif

using content::BrowserThread;
using content::ChildProcessSecurityPolicy;

BrowserProcessImpl::BrowserProcessImpl(StartupData* startup_data)
    : startup_data_(startup_data),
      browser_policy_connector_(startup_data->chrome_feature_list_creator()
                                    ->TakeChromeBrowserPolicyConnector()),
      local_state_(
          startup_data->chrome_feature_list_creator()->TakePrefService()),
      platform_part_(std::make_unique<BrowserProcessPlatformPart>()) {
  g_browser_process = this;

  // Initialize the SessionIdGenerator instance, providing a PrefService to
  // ensure the persistent storage of current max SessionId.
  sessions::SessionIdGenerator::GetInstance()->Init(local_state_.get());

  DCHECK(browser_policy_connector_);
  DCHECK(local_state_);
  DCHECK(startup_data);
  // Most work should be done in Init().
}

void BrowserProcessImpl::Init() {
  if (content::IsOutOfProcessNetworkService()) {
    // Initialize NetLog source IDs to use an alternate starting value for
    // the browser process. This needs to be done early in process startup
    // before any NetLogSource objects might get created.
    net::NetLog::Get()->InitializeSourceIdPartition();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Forces creation of |metrics_services_manager_client_| if necessary
  // (typically this call is a no-op as MetricsServicesManager has already been
  // created).
  GetMetricsServicesManager();
  DCHECK(metrics_services_manager_client_);
  metrics_services_manager_client_->OnCrosSettingsCreated();
#endif

  download_status_updater_ = std::make_unique<DownloadStatusUpdater>();

#if BUILDFLAG(ENABLE_PRINTING)
  // Must be created after the NotificationService.
  print_job_manager_ = std::make_unique<printing::PrintJobManager>();
#endif

  ChildProcessSecurityPolicy::GetInstance()->RegisterWebSafeScheme(
      chrome::kChromeSearchScheme);

#if BUILDFLAG(IS_MAC)
  ui::InitIdleMonitor();
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::AppWindowClient::Set(ChromeAppWindowClient::GetInstance());

  extension_event_router_forwarder_ =
      base::MakeRefCounted<extensions::EventRouterForwarder>();

  EnsureExtensionsClientInitialized();

  extensions_browser_client_ =
      std::make_unique<extensions::ChromeExtensionsBrowserClient>();
  extensions_browser_client_->AddAPIProvider(
      std::make_unique<chrome_apps::ChromeAppsBrowserAPIProvider>());
  extensions::ExtensionsBrowserClient::Set(extensions_browser_client_.get());

#if BUILDFLAG(IS_CHROMEOS)
  extensions_browser_client_->AddAPIProvider(
      std::make_unique<
          chromeos::ChromeOSTelemetryExtensionsBrowserAPIProvider>());
#endif  // BUILDFLAG(IS_CHROMEOS)

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_CHROME_NOTIFICATIONS)
  message_center::MessageCenter::Initialize();
  // Set the system notification source display name ("Google Chrome" or
  // "Chromium").
  if (message_center::MessageCenter::Get()) {
    message_center::MessageCenter::Get()->SetSystemNotificationAppName(
        l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  }
#endif

  system_notification_helper_ = std::make_unique<SystemNotificationHelper>();

  update_client::UpdateQueryParams::SetDelegate(
      ChromeUpdateQueryParamsDelegate::GetInstance());

  // Make sure permissions client has been set.
  ChromePermissionsClient::GetInstance();

  // Make sure webapps client has been set.
  webapps::ChromeWebappsClient::GetInstance();

#if !BUILDFLAG(IS_ANDROID)
  KeepAliveRegistry::GetInstance()->SetIsShuttingDown(false);
  KeepAliveRegistry::GetInstance()->AddObserver(this);
#endif  // !BUILDFLAG(IS_ANDROID)

  MigrateObsoleteLocalStatePrefs(local_state());
  pref_change_registrar_.Init(local_state());

  // Initialize the notification for the default browser setting policy.
  pref_change_registrar_.Add(
      prefs::kDefaultBrowserSettingEnabled,
      base::BindRepeating(&BrowserProcessImpl::ApplyDefaultBrowserPolicy,
                          base::Unretained(this)));

  // This preference must be kept in sync with external values; update them
  // whenever the preference or its controlling policy changes.
  pref_change_registrar_.Add(metrics::prefs::kMetricsReportingEnabled,
                             base::BindRepeating(&ApplyMetricsReportingPolicy));

  DCHECK(!webrtc_event_log_manager_);
  webrtc_event_log_manager_ = WebRtcEventLogManager::CreateSingletonInstance();

#if BUILDFLAG(IS_MAC)
  system_media_permissions::LogSystemMediaPermissionsStartupStats();
#endif

#if BUILDFLAG(IS_ANDROID)
  components::WebAuthnClientAndroid::SetClient(
      std::make_unique<ChromeWebAuthnClientAndroid>());
#endif

#if !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_CHROMEOS)
  hid_system_tray_icon_ = std::make_unique<HidPinnedNotification>();
#else
  hid_system_tray_icon_ = std::make_unique<HidStatusIcon>();
#endif  // BUILDFLAG(IS_CHROMEOS)
#endif  // !BUILDFLAG(IS_ANDROID)
}

#if !BUILDFLAG(IS_ANDROID)
void BrowserProcessImpl::SetQuitClosure(base::OnceClosure quit_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(quit_closure);
  DCHECK(!quit_closure_);
  quit_closure_ = std::move(quit_closure);
}
#endif

#if BUILDFLAG(IS_MAC)
void BrowserProcessImpl::ClearQuitClosure() {
  quit_closure_.Reset();
}
#endif

BrowserProcessImpl::~BrowserProcessImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ExtensionsBrowserClient::Set(nullptr);
  extensions::AppWindowClient::Set(nullptr);
#endif

#if !BUILDFLAG(IS_ANDROID)
  KeepAliveRegistry::GetInstance()->RemoveObserver(this);
#endif

  g_browser_process = nullptr;
}

#if !BUILDFLAG(IS_ANDROID)
void BrowserProcessImpl::StartTearDown() {
  TRACE_EVENT0("shutdown", "BrowserProcessImpl::StartTearDown");
  // TODO(crbug.com/560486): Fix the tests that make the check of
  // |tearing_down_| necessary in IsShuttingDown().
  tearing_down_ = true;
  DCHECK(IsShuttingDown());

  platform_part()->BeginStartTearDown();

  metrics_services_manager_.reset();
  intranet_redirect_detector_.reset();
  if (safe_browsing_service_.get())
    safe_browsing_service()->ShutDown();
  network_time_tracker_.reset();

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // Initial cleanup for ChromeBrowserCloudManagement, shutdown components that
  // depend on profile and notification system. For example, ProfileManager
  // observer and KeyServices observer need to be removed before profiles.
  auto* cloud_management_controller =
      browser_policy_connector_->chrome_browser_cloud_management_controller();
  if (cloud_management_controller)
    cloud_management_controller->ShutDown();
#endif

#if !BUILDFLAG(IS_ANDROID)
  // |hid_system_tray_icon_| must be destroyed before
  // |system_notification_helper_| for ChromeOS and |status_tray_| for
  // non-ChromeOS.
  hid_system_tray_icon_.reset();
#endif

  system_notification_helper_.reset();

#if BUILDFLAG(ENABLE_CHROME_NOTIFICATIONS)
  // Need to clear the desktop notification balloons before the IO thread and
  // before the profiles, since if there are any still showing we will access
  // those things during teardown.
  notification_ui_manager_.reset();
#endif

  // Debugger must be cleaned up before ProfileManager.
  remote_debugging_server_.reset();
  devtools_auto_opener_.reset();

  battery_metrics_.reset();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // The Extensions Browser Client needs to teardown some members while the
  // profile manager is still alive.
  extensions_browser_client_->StartTearDown();
#endif

  // Need to clear profiles (download managers) before the IO thread.
  {
    TRACE_EVENT0("shutdown",
                 "BrowserProcessImpl::StartTearDown:ProfileManager");
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    // The desktop profile picker needs to be closed before the guest profile
    // can be destroyed.
    ProfilePicker::Hide();
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
    // `profile_manager_` must be destroyed before `background_mode_manager_`,
    // because the background mode manager does not stop observing profile
    // changes at destruction (notifying the observers would cause a use-after-
    // free).
    profile_manager_.reset();
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // The `media_file_system_registry_` cannot be reset until the
  // `profile_manager_` has been.
  media_file_system_registry_.reset();
  // Remove the global instance of the Storage Monitor now. Otherwise the
  // FILE thread would be gone when we try to release it in the dtor and
  // Valgrind would report a leak on almost every single browser_test.
  // TODO(gbillock): Make this unnecessary.
  storage_monitor::StorageMonitor::Destroy();
#endif

#if BUILDFLAG(ENABLE_CHROME_NOTIFICATIONS)
  if (message_center::MessageCenter::Get())
    message_center::MessageCenter::Shutdown();
#endif

  // The policy providers managed by |browser_policy_connector_| need to shut
  // down while the IO and FILE threads are still alive. The monitoring
  // framework owned by |browser_policy_connector_| relies on |gcm_driver_|, so
  // this must be shutdown before |gcm_driver_| below.
  browser_policy_connector_->Shutdown();

  // The |gcm_driver_| must shut down while the IO thread is still alive.
  if (gcm_driver_)
    gcm_driver_->Shutdown();

  platform_part()->StartTearDown();

  // Cancel any uploads to release the system url request context references.
  if (webrtc_log_uploader_)
    webrtc_log_uploader_->Shutdown();

  sessions::SessionIdGenerator::GetInstance()->Shutdown();

  // Resetting the status tray will result in calls to
  // |g_browser_process->local_state()|. See crbug.com/1187418
  status_tray_.reset();

  local_state_->CommitPendingWrite();

  // This expects to be destroyed before the task scheduler is torn down.
  SystemNetworkContextManager::DeleteInstance();

  // The ApplicationBreadcrumbsLogger logs a shutdown event via a task when it
  // is destroyed, so it should be destroyed before the task scheduler is torn
  // down.
  application_breadcrumbs_logger_.reset();
}

void BrowserProcessImpl::PostDestroyThreads() {
  // With the file_thread_ flushed, we can release any icon resources.
  icon_manager_.reset();

  // Must outlive the worker threads.
  webrtc_log_uploader_.reset();
}
#endif  // !BUILDFLAG(IS_ANDROID)

void BrowserProcessImpl::SetMetricsServices(
    std::unique_ptr<metrics_services_manager::MetricsServicesManager> manager,
    metrics_services_manager::MetricsServicesManagerClient* client) {
  metrics_services_manager_ = std::move(manager);
  metrics_services_manager_client_ =
      static_cast<ChromeMetricsServicesManagerClient*>(client);
}

namespace {

// Used at the end of session to block the UI thread for completion of sentinel
// tasks on the set of threads used to persist profile data and local state.
// This is done to ensure that the data has been persisted to disk before
// continuing.
class RundownTaskCounter :
    public base::RefCountedThreadSafe<RundownTaskCounter> {
 public:
  RundownTaskCounter();

  RundownTaskCounter(const RundownTaskCounter&) = delete;
  RundownTaskCounter& operator=(const RundownTaskCounter&) = delete;

  // Increments |count_| and returns a closure bound to Decrement(). All
  // closures returned by this RundownTaskCounter's GetRundownClosure() method
  // must be invoked for TimedWait() to complete its wait without timing
  // out.
  base::OnceClosure GetRundownClosure();

  // Waits until the count is zero or |timeout| expires.
  // This can only be called once per instance.
  void TimedWait(base::TimeDelta timeout);

 private:
  friend class base::RefCountedThreadSafe<RundownTaskCounter>;
  ~RundownTaskCounter() {}

  // Decrements the counter and releases the waitable event on transition to
  // zero.
  void Decrement();

  // The count starts at one to defer the possibility of one->zero transitions
  // until TimedWait is called.
  base::AtomicRefCount count_{1};
  base::WaitableEvent waitable_event_;
};

RundownTaskCounter::RundownTaskCounter() = default;

base::OnceClosure RundownTaskCounter::GetRundownClosure() {
  // As the count starts off at one, it should never get to zero unless
  // TimedWait has been called.
  DCHECK(!count_.IsZero());

  count_.Increment();

  return base::BindOnce(&RundownTaskCounter::Decrement, this);
}

void RundownTaskCounter::Decrement() {
  if (!count_.Decrement())
    waitable_event_.Signal();
}

void RundownTaskCounter::TimedWait(base::TimeDelta timeout) {
  // Decrement the excess count from the constructor.
  Decrement();

  // RundownTaskCounter::TimedWait() could return
  // |waitable_event_.TimedWait()|'s result if any user ever cared about whether
  // it returned per success or timeout. Currently no user of this API cares and
  // as such this return value is ignored.
  waitable_event_.TimedWait(timeout);
}

#if !BUILDFLAG(IS_ANDROID)
void RequestProxyResolvingSocketFactoryOnUIThread(
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
        receiver) {
  network::mojom::NetworkContext* network_context =
      g_browser_process->system_network_context_manager()->GetContext();
  network_context->CreateProxyResolvingSocketFactory(std::move(receiver));
}

void RequestProxyResolvingSocketFactory(
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
        receiver) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&RequestProxyResolvingSocketFactoryOnUIThread,
                                std::move(receiver)));
}
#endif

}  // namespace

void BrowserProcessImpl::FlushLocalStateAndReply(base::OnceClosure reply) {
  local_state_->CommitPendingWrite(std::move(reply));
}

void BrowserProcessImpl::EndSession() {
  // Mark all the profiles as clean.
  ProfileManager* pm = profile_manager();
  scoped_refptr<RundownTaskCounter> rundown_counter =
      base::MakeRefCounted<RundownTaskCounter>();
  for (Profile* profile : pm->GetLoadedProfiles()) {
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
    ExitTypeService* exit_type_service =
        ExitTypeService::GetInstanceForProfile(profile);
    if (exit_type_service)
      exit_type_service->SetCurrentSessionExitType(ExitType::kForcedShutdown);
#endif
    if (profile->GetPrefs()) {
      profile->GetPrefs()->CommitPendingWrite(
          base::OnceClosure(), rundown_counter->GetRundownClosure());
    }
  }

  // Tell the metrics service it was cleanly shutdown.
  metrics::MetricsService* metrics = g_browser_process->metrics_service();
  if (metrics) {
    metrics->LogCleanShutdown();
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    // The MetricsService may update Local State prefs in memory without
    // writing the updated prefs to disk, so schedule a Local State write now.
    //
    // Do not schedule a write on ChromeOS because writing to disk multiple
    // times during shutdown was causing shutdown problems. See crbug/302578.
    local_state_->CommitPendingWrite(base::OnceClosure(),
                                     rundown_counter->GetRundownClosure());
#endif
  }

  // This wait is legitimate and necessary on Windows, since the process will
  // be terminated soon.
  // http://crbug.com/125207
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;

  // We must write that the profile and metrics service shutdown cleanly,
  // otherwise on startup we'll think we crashed. So we block until done and
  // then proceed with normal shutdown.
  //
  // If you change the condition here, be sure to also change
  // ProfileBrowserTests to match.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_OZONE)
  // Do a best-effort wait on the successful countdown of rundown tasks. Note
  // that if we don't complete "quickly enough", Windows will terminate our
  // process.
  //
  // On Windows, we previously posted a message to FILE and then ran a nested
  // message loop, waiting for that message to be processed until quitting.
  // However, doing so means that other messages will also be processed. In
  // particular, if the GPU process host notices that the GPU has been killed
  // during shutdown, it races exiting the nested loop with the process host
  // blocking the message loop attempting to re-establish a connection to the
  // GPU process synchronously. Because the system may not be allowing
  // processes to launch, this can result in a hang. See
  // http://crbug.com/318527.
  rundown_counter->TimedWait(kEndSessionTimeout);
#else
  NOTIMPLEMENTED();
#endif
}

metrics_services_manager::MetricsServicesManager*
BrowserProcessImpl::GetMetricsServicesManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!metrics_services_manager_) {
    auto client =
        std::make_unique<ChromeMetricsServicesManagerClient>(local_state());
    metrics_services_manager_client_ = client.get();
    metrics_services_manager_ =
        std::make_unique<metrics_services_manager::MetricsServicesManager>(
            std::move(client));
  }
  return metrics_services_manager_.get();
}

metrics::MetricsService* BrowserProcessImpl::metrics_service() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetMetricsServicesManager()->GetMetricsService();
}

SystemNetworkContextManager*
BrowserProcessImpl::system_network_context_manager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(SystemNetworkContextManager::GetInstance());
  return SystemNetworkContextManager::GetInstance();
}

scoped_refptr<network::SharedURLLoaderFactory>
BrowserProcessImpl::shared_url_loader_factory() {
  return system_network_context_manager()->GetSharedURLLoaderFactory();
}

network::NetworkQualityTracker* BrowserProcessImpl::network_quality_tracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!network_quality_tracker_) {
    network_quality_tracker_ = std::make_unique<network::NetworkQualityTracker>(
        base::BindRepeating(&content::GetNetworkService));
  }
  return network_quality_tracker_.get();
}

ProfileManager* BrowserProcessImpl::profile_manager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!created_profile_manager_)
    CreateProfileManager();
  return profile_manager_.get();
}

PrefService* BrowserProcessImpl::local_state() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return local_state_.get();
}

variations::VariationsService* BrowserProcessImpl::variations_service() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetMetricsServicesManager()->GetVariationsService();
}

BrowserProcessPlatformPart* BrowserProcessImpl::platform_part() {
  return platform_part_.get();
}

extensions::EventRouterForwarder*
BrowserProcessImpl::extension_event_router_forwarder() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return extension_event_router_forwarder_.get();
#else
  return NULL;
#endif
}

NotificationUIManager* BrowserProcessImpl::notification_ui_manager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(ENABLE_CHROME_NOTIFICATIONS)
  if (!created_notification_ui_manager_)
    CreateNotificationUIManager();
  return notification_ui_manager_.get();
#else
  return nullptr;
#endif
}

NotificationPlatformBridge* BrowserProcessImpl::notification_platform_bridge() {
#if BUILDFLAG(ENABLE_SYSTEM_NOTIFICATIONS)
  if (!created_notification_bridge_)
    CreateNotificationPlatformBridge();
  return notification_bridge_.get();
#else
  return nullptr;
#endif
}

policy::ChromeBrowserPolicyConnector*
BrowserProcessImpl::browser_policy_connector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return browser_policy_connector_.get();
}

policy::PolicyService* BrowserProcessImpl::policy_service() {
  return browser_policy_connector()->GetPolicyService();
}

IconManager* BrowserProcessImpl::icon_manager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!created_icon_manager_)
    CreateIconManager();
  return icon_manager_.get();
}

GpuModeManager* BrowserProcessImpl::gpu_mode_manager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!gpu_mode_manager_)
    gpu_mode_manager_ = std::make_unique<GpuModeManager>();
  return gpu_mode_manager_.get();
}

void BrowserProcessImpl::CreateDevToolsProtocolHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if !BUILDFLAG(IS_ANDROID)
  // StartupBrowserCreator::LaunchBrowser can be run multiple times when browser
  // is started with several profiles or existing browser process is reused.
  if (!remote_debugging_server_) {
    if (!local_state_->GetBoolean(prefs::kDevToolsRemoteDebuggingAllowed)) {
      // Follow content/browser/devtools/devtools_http_handler.cc that reports
      // its remote debugging port on stderr for symmetry.
      fputs("\nDevTools remote debugging is disallowed by the system admin.\n",
            stderr);
      fflush(stderr);
      return;
    }
    remote_debugging_server_ = std::make_unique<RemoteDebuggingServer>();
  }
#endif
}

void BrowserProcessImpl::CreateDevToolsAutoOpener() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if !BUILDFLAG(IS_ANDROID)
  // StartupBrowserCreator::LaunchBrowser can be run multiple times when browser
  // is started with several profiles or existing browser process is reused.
  if (!devtools_auto_opener_)
    devtools_auto_opener_ = std::make_unique<DevToolsAutoOpener>();
#endif
}

bool BrowserProcessImpl::IsShuttingDown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO (crbug.com/560486): Fix the tests that make the check of
  // |tearing_down_| necessary here.
  // TODO (crbug/1155597): Maybe use browser_shutdown::HasShutdownStarted here.
  return shutting_down_ || tearing_down_;
}

printing::PrintJobManager* BrowserProcessImpl::print_job_manager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(ENABLE_PRINTING)
  return print_job_manager_.get();
#else
  NOTREACHED();
  return nullptr;
#endif
}

printing::PrintPreviewDialogController*
    BrowserProcessImpl::print_preview_dialog_controller() {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!print_preview_dialog_controller_.get())
    CreatePrintPreviewDialogController();
  return print_preview_dialog_controller_.get();
#else
  NOTIMPLEMENTED();
  return NULL;
#endif
}

printing::BackgroundPrintingManager*
    BrowserProcessImpl::background_printing_manager() {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!background_printing_manager_)
    CreateBackgroundPrintingManager();
  return background_printing_manager_.get();
#else
  NOTIMPLEMENTED();
  return NULL;
#endif
}

#if !BUILDFLAG(IS_ANDROID)
IntranetRedirectDetector* BrowserProcessImpl::intranet_redirect_detector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!intranet_redirect_detector_)
    intranet_redirect_detector_ = std::make_unique<IntranetRedirectDetector>();

  return intranet_redirect_detector_.get();
}
#endif

const std::string& BrowserProcessImpl::GetApplicationLocale() {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/1033644): Remove #if.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif
  DCHECK(!locale_.empty());
  return locale_;
}

void BrowserProcessImpl::SetApplicationLocale(
    const std::string& actual_locale) {
  // NOTE: this is called before any threads have been created in non-test
  // environments.
  locale_ = actual_locale;
  ChromeContentBrowserClient::SetApplicationLocale(actual_locale);
  translate::TranslateDownloadManager::GetInstance()->set_application_locale(
      actual_locale);
}

DownloadStatusUpdater* BrowserProcessImpl::download_status_updater() {
  return download_status_updater_.get();
}

MediaFileSystemRegistry* BrowserProcessImpl::media_file_system_registry() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!media_file_system_registry_)
    media_file_system_registry_ = std::make_unique<MediaFileSystemRegistry>();
  return media_file_system_registry_.get();
#else
  return NULL;
#endif
}

WebRtcLogUploader* BrowserProcessImpl::webrtc_log_uploader() {
  if (!webrtc_log_uploader_)
    webrtc_log_uploader_ = std::make_unique<WebRtcLogUploader>();
  return webrtc_log_uploader_.get();
}

network_time::NetworkTimeTracker* BrowserProcessImpl::network_time_tracker() {
  CreateNetworkTimeTracker();
  return network_time_tracker_.get();
}

#if !BUILDFLAG(IS_ANDROID)
gcm::GCMDriver* BrowserProcessImpl::gcm_driver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!gcm_driver_)
    CreateGCMDriver();
  return gcm_driver_.get();
}
#endif

resource_coordinator::TabManager* BrowserProcessImpl::GetTabManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return resource_coordinator_parts()->tab_manager();
}

resource_coordinator::ResourceCoordinatorParts*
BrowserProcessImpl::resource_coordinator_parts() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!resource_coordinator_parts_) {
    resource_coordinator_parts_ =
        std::make_unique<resource_coordinator::ResourceCoordinatorParts>();
  }
  return resource_coordinator_parts_.get();
}

#if !BUILDFLAG(IS_ANDROID)
SerialPolicyAllowedPorts* BrowserProcessImpl::serial_policy_allowed_ports() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!serial_policy_allowed_ports_) {
    serial_policy_allowed_ports_ =
        std::make_unique<SerialPolicyAllowedPorts>(local_state());
  }
  return serial_policy_allowed_ports_.get();
}

HidPolicyAllowedDevices* BrowserProcessImpl::hid_policy_allowed_devices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!hid_policy_allowed_devices_) {
    hid_policy_allowed_devices_ =
        std::make_unique<HidPolicyAllowedDevices>(local_state());
  }
  return hid_policy_allowed_devices_.get();
}

HidSystemTrayIcon* BrowserProcessImpl::hid_system_tray_icon() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return hid_system_tray_icon_.get();
}
#endif

BuildState* BrowserProcessImpl::GetBuildState() {
#if !BUILDFLAG(IS_ANDROID)
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return &build_state_;
#else
  NOTIMPLEMENTED();
  return nullptr;
#endif
}

breadcrumbs::BreadcrumbPersistentStorageManager*
BrowserProcessImpl::GetBreadcrumbPersistentStorageManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return application_breadcrumbs_logger_
             ? application_breadcrumbs_logger_->GetPersistentStorageManager()
             : nullptr;
}

// static
void BrowserProcessImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDefaultBrowserSettingEnabled,
                                false);

  registry->RegisterBooleanPref(prefs::kAllowCrossOriginAuthPrompt, false);

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(prefs::kEulaAccepted, false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)

  registry->RegisterStringPref(language::prefs::kApplicationLocale,
                               std::string());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterStringPref(prefs::kOwnerLocale, std::string());
  registry->RegisterStringPref(prefs::kHardwareKeyboardLayout,
                               std::string());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  registry->RegisterBooleanPref(metrics::prefs::kMetricsReportingEnabled,
                                GoogleUpdateSettings::GetCollectStatsConsent());
  registry->RegisterBooleanPref(prefs::kDevToolsRemoteDebuggingAllowed, true);
}

DownloadRequestLimiter* BrowserProcessImpl::download_request_limiter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!download_request_limiter_.get()) {
    download_request_limiter_ = base::MakeRefCounted<DownloadRequestLimiter>();
  }
  return download_request_limiter_.get();
}

BackgroundModeManager* BrowserProcessImpl::background_mode_manager() {
#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!background_mode_manager_)
    CreateBackgroundModeManager();
  return background_mode_manager_.get();
#else
  return nullptr;
#endif
}

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
void BrowserProcessImpl::set_background_mode_manager_for_test(
    std::unique_ptr<BackgroundModeManager> manager) {
  background_mode_manager_ = std::move(manager);
}
#endif

StatusTray* BrowserProcessImpl::status_tray() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!status_tray_)
    CreateStatusTray();
  return status_tray_.get();
}

safe_browsing::SafeBrowsingService*
BrowserProcessImpl::safe_browsing_service() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!created_safe_browsing_service_)
    CreateSafeBrowsingService();
  return safe_browsing_service_.get();
}

subresource_filter::RulesetService*
BrowserProcessImpl::subresource_filter_ruleset_service() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!created_subresource_filter_ruleset_service_)
    CreateSubresourceFilterRulesetService();
  return subresource_filter_ruleset_service_.get();
}

StartupData* BrowserProcessImpl::startup_data() {
  return startup_data_;
}

// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
void BrowserProcessImpl::StartAutoupdateTimer() {
  autoupdate_timer_.Start(FROM_HERE, base::Hours(kUpdateCheckIntervalHours),
                          this, &BrowserProcessImpl::OnAutoupdateTimer);
}
#endif

component_updater::ComponentUpdateService*
BrowserProcessImpl::component_updater() {
  if (component_updater_)
    return component_updater_.get();

  if (!BrowserThread::CurrentlyOn(BrowserThread::UI))
    return nullptr;

  std::unique_ptr<component_updater::UpdateScheduler> scheduler =
      std::make_unique<component_updater::TimerUpdateScheduler>();

  std::string brand;
  google_brand::GetBrand(&brand);
  component_updater_ = component_updater::ComponentUpdateServiceFactory(
      component_updater::MakeChromeComponentUpdaterConfigurator(
          base::CommandLine::ForCurrentProcess(),
          g_browser_process->local_state()),
      std::move(scheduler), brand);

  return component_updater_.get();
}

void BrowserProcessImpl::OnKeepAliveStateChanged(bool is_keeping_alive) {
  if (is_keeping_alive)
    Pin();
  else
    Unpin();
}

void BrowserProcessImpl::CreateNetworkQualityObserver() {
  DCHECK(!network_quality_observer_);
  network_quality_observer_ =
      content::CreateNetworkQualityObserver(network_quality_tracker());
  DCHECK(network_quality_observer_);
}

void BrowserProcessImpl::OnKeepAliveRestartStateChanged(bool can_restart) {}

void BrowserProcessImpl::CreateProfileManager() {
  DCHECK(!created_profile_manager_ && !profile_manager_);
  created_profile_manager_ = true;

  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  profile_manager_ = std::make_unique<ProfileManager>(user_data_dir);
}

void BrowserProcessImpl::PreCreateThreads() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // chrome-extension:// URLs are safe to request anywhere, but may only
  // commit (including in iframes) in extension processes.
  ChildProcessSecurityPolicy::GetInstance()->RegisterWebSafeIsolatedScheme(
      extensions::kExtensionScheme, true);
#endif

  battery_metrics_ = std::make_unique<BatteryMetrics>();

#if BUILDFLAG(IS_ANDROID)
  app_state_listener_ = base::android::ApplicationStatusListener::New(
      base::BindRepeating([](base::android::ApplicationState state) {
        content::OnBrowserVisibilityChanged(
            state == base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES ||
            state == base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES);
      }));
  content::OnBrowserVisibilityChanged(
      base::android::ApplicationStatusListener::HasVisibleActivities());
#endif  // BUILDFLAG(IS_ANDROID)

  secure_origin_prefs_observer_ =
      std::make_unique<SecureOriginPrefsObserver>(local_state());
  site_isolation_prefs_observer_ =
      std::make_unique<SiteIsolationPrefsObserver>(local_state());

  // Create SystemNetworkContextManager without a NetworkService if it has not
  // been requested yet.
  if (!SystemNetworkContextManager::HasInstance())
    SystemNetworkContextManager::CreateInstance(local_state());
}

void BrowserProcessImpl::PreMainMessageLoopRun() {
  TRACE_EVENT0("startup", "BrowserProcessImpl::PreMainMessageLoopRun");
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Startup.BrowserProcessImpl_PreMainMessageLoopRunTime");

  // browser_policy_connector() is created very early because local_state()
  // needs policy to be initialized with the managed preference values.
  // However, policy fetches from the network and loading of disk caches
  // requires that threads are running; this Init() call lets the connector
  // resume its initialization now that the loops are spinning and the
  // system request context is available for the fetchers.
  browser_policy_connector()->Init(
      local_state(),
      system_network_context_manager()->GetSharedURLLoaderFactory());

  if (local_state_->IsManagedPreference(prefs::kDefaultBrowserSettingEnabled))
    ApplyDefaultBrowserPolicy();

  ApplyMetricsReportingPolicy();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  ChromeJsErrorReportProcessor::Create();
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  content::PluginService::GetInstance()->SetFilter(
      ChromePluginServiceFilter::GetInstance());
#endif  // BUILDFLAG(ENABLE_PLUGINS)

#if !BUILDFLAG(IS_ANDROID)
  storage_monitor::StorageMonitor::Create();
#endif

  platform_part_->PreMainMessageLoopRun();

  if (base::FeatureList::IsEnabled(network_time::kNetworkTimeServiceQuerying)) {
    CreateNetworkTimeTracker();
  }

  CreateNetworkQualityObserver();

#if BUILDFLAG(IS_ANDROID)
  // This needs to be here so that SecurityStateClient is non-null when
  // SecurityStateModel code is called.
  security_state::SetSecurityStateClient(new ChromeSecurityStateClient());
#endif

// Create the global SodaInstaller instance.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  soda_installer_impl_ = std::make_unique<speech::SodaInstallerImpl>();
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  soda_installer_impl_ = std::make_unique<speech::SodaInstallerImplChromeOS>();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  base::FilePath user_data_dir;
  bool result = base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(result);
  if (breadcrumbs::IsEnabled()) {
    // Start crash reporter listening for breadcrumb events. Collected
    // breadcrumbs will be attached to crash reports.
    breadcrumbs::CrashReporterBreadcrumbObserver::GetInstance();

    application_breadcrumbs_logger_ =
        std::make_unique<breadcrumbs::ApplicationBreadcrumbsLogger>(
            user_data_dir, base::BindRepeating([] {
              return ChromeMetricsServiceAccessor::
                  IsMetricsAndCrashReportingEnabled();
            }));

    // Get stored persistent breadcrumbs from last run to set on crash reports.
    GetBreadcrumbPersistentStorageManager()->GetStoredEvents(
        base::BindOnce([](std::vector<std::string> events) {
          breadcrumbs::BreadcrumbManager::GetInstance()
              .SetPreviousSessionEvents(events);
        }));
  } else {
    breadcrumbs::DeleteBreadcrumbFiles(user_data_dir);
  }
}

void BrowserProcessImpl::CreateIconManager() {
  DCHECK(!created_icon_manager_ && !icon_manager_);
  created_icon_manager_ = true;
  icon_manager_ = std::make_unique<IconManager>();
}

void BrowserProcessImpl::CreateNotificationPlatformBridge() {
#if BUILDFLAG(ENABLE_SYSTEM_NOTIFICATIONS)
  DCHECK(!notification_bridge_);
  notification_bridge_ = NotificationPlatformBridge::Create();
  created_notification_bridge_ = true;
#endif
}

void BrowserProcessImpl::CreateNotificationUIManager() {
#if BUILDFLAG(ENABLE_CHROME_NOTIFICATIONS)
  DCHECK(!notification_ui_manager_);
  notification_ui_manager_ = NotificationUIManager::Create();
  created_notification_ui_manager_ = !!notification_ui_manager_;
#endif
}

void BrowserProcessImpl::CreateBackgroundModeManager() {
#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  DCHECK(!background_mode_manager_);
  background_mode_manager_ = std::make_unique<BackgroundModeManager>(
      *base::CommandLine::ForCurrentProcess(),
      &profile_manager()->GetProfileAttributesStorage());
#endif
}

void BrowserProcessImpl::CreateStatusTray() {
  DCHECK(!status_tray_);
  status_tray_ = StatusTray::Create();
}

void BrowserProcessImpl::CreatePrintPreviewDialogController() {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  DCHECK(!print_preview_dialog_controller_);
  print_preview_dialog_controller_ =
      base::MakeRefCounted<printing::PrintPreviewDialogController>();
#else
  NOTIMPLEMENTED();
#endif
}

void BrowserProcessImpl::CreateBackgroundPrintingManager() {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  DCHECK(!background_printing_manager_);
  background_printing_manager_ =
      std::make_unique<printing::BackgroundPrintingManager>();
#else
  NOTIMPLEMENTED();
#endif
}

void BrowserProcessImpl::CreateSafeBrowsingService() {
  DCHECK(!safe_browsing_service_);
  // Set this flag to true so that we don't retry indefinitely to
  // create the service class if there was an error.
  created_safe_browsing_service_ = true;

  // The factory can be overridden in tests.
  if (!safe_browsing::SafeBrowsingServiceInterface::HasFactory()) {
    safe_browsing::SafeBrowsingServiceInterface::RegisterFactory(
        safe_browsing::GetSafeBrowsingServiceFactory());
  }

  // TODO(crbug/925153): Port consumers of the |safe_browsing_service_| to use
  // the interface in components/safe_browsing, and remove this cast.
  safe_browsing_service_ = static_cast<safe_browsing::SafeBrowsingService*>(
      safe_browsing::SafeBrowsingServiceInterface::CreateSafeBrowsingService());
  if (safe_browsing_service_)
    safe_browsing_service_->Initialize();
}

void BrowserProcessImpl::CreateSubresourceFilterRulesetService() {
  DCHECK(!subresource_filter_ruleset_service_);
  created_subresource_filter_ruleset_service_ = true;

  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  subresource_filter_ruleset_service_ =
      subresource_filter::RulesetService::Create(local_state(), user_data_dir);
}

#if !BUILDFLAG(IS_ANDROID)
// Android's GCMDriver currently makes the assumption that it's a singleton.
// Until this gets fixed, instantiating multiple Java GCMDrivers will throw an
// exception, but because they're only initialized on demand these crashes
// would be very difficult to triage. See http://crbug.com/437827.
void BrowserProcessImpl::CreateGCMDriver() {
  DCHECK(!gcm_driver_);

  base::FilePath store_path;
  CHECK(base::PathService::Get(chrome::DIR_GLOBAL_GCM_STORE, &store_path));
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));

  gcm_driver_ = gcm::CreateGCMDriverDesktop(
      base::WrapUnique(new gcm::GCMClientFactory), local_state(), store_path,
      /*remove_account_mappings_with_email_key=*/false,
      base::BindRepeating(&RequestProxyResolvingSocketFactory),
      system_network_context_manager()->GetSharedURLLoaderFactory(),
      content::GetNetworkConnectionTracker(), chrome::GetChannel(),
      gcm::GetProductCategoryForSubtypes(local_state()),
      content::GetUIThreadTaskRunner({}), content::GetIOThreadTaskRunner({}),
      blocking_task_runner);
}
#endif  // !BUILDFLAG(IS_ANDROID)

void BrowserProcessImpl::CreateNetworkTimeTracker() {
  if (network_time_tracker_) {
    return;
  }
  network_time_tracker_ = std::make_unique<network_time::NetworkTimeTracker>(
      base::WrapUnique(new base::DefaultClock()),
      base::WrapUnique(new base::DefaultTickClock()), local_state(),
      system_network_context_manager()->GetSharedURLLoaderFactory());
}

void BrowserProcessImpl::ApplyDefaultBrowserPolicy() {
  if (local_state()->GetBoolean(prefs::kDefaultBrowserSettingEnabled)) {
    // The worker pointer is reference counted. While it is running, the
    // message loops of the FILE and UI thread will hold references to it
    // and it will be automatically freed once all its tasks have finished.
    auto set_browser_worker =
        base::MakeRefCounted<shell_integration::DefaultBrowserWorker>();
    // The user interaction must always be disabled when applying the default
    // browser policy since it is done at each browser startup and the result
    // of the interaction cannot be forced.
    set_browser_worker->set_interactive_permitted(false);
    set_browser_worker->StartSetAsDefault(base::NullCallback());
  }
}

void BrowserProcessImpl::Pin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!IsShuttingDown());
}

void BrowserProcessImpl::Unpin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if !BUILDFLAG(IS_ANDROID)
  // The quit closure is set by ChromeBrowserMainParts to transfer ownership of
  // the browser's lifetime to the BrowserProcess. Any KeepAlives registered and
  // unregistered prior to setting the quit closure are ignored. Only once the
  // quit closure is set should unpinning start process shutdown.
  if (!quit_closure_)
    return;
#endif

  DCHECK(!shutting_down_);
  shutting_down_ = true;

#if !BUILDFLAG(IS_ANDROID)
  KeepAliveRegistry::GetInstance()->SetIsShuttingDown();
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_PRINTING)
  // Wait for the pending print jobs to finish. Don't do this later, since
  // this might cause a nested run loop to run, and we don't want pending
  // tasks to run once teardown has started.
  print_job_manager_->Shutdown();
#endif

#if defined(LEAK_SANITIZER)
  // Check for memory leaks now, before we start shutting down threads. Doing
  // this early means we won't report any shutdown-only leaks (as they have
  // not yet happened at this point).
  // If leaks are found, this will make the process exit immediately.
  __lsan_do_leak_check();
#endif

  CHECK(base::RunLoop::IsRunningOnCurrentThread());

#if BUILDFLAG(IS_MAC)
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(ChromeBrowserMainPartsMac::DidEndMainMessageLoop));
#endif

#if !BUILDFLAG(IS_ANDROID)
  std::move(quit_closure_).Run();

  chrome::ShutdownIfNeeded();

  // TODO(crbug.com/967603): remove when root cause is found.
  CHECK_EQ(BrowserList::GetInstance()->size(), 0u);
#endif  // !BUILDFLAG(IS_ANDROID)
}

// Mac is currently not supported.
// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))

bool BrowserProcessImpl::IsRunningInBackground() const {
  // Check if browser is in the background.
  return chrome::GetTotalBrowserCount() == 0 &&
         KeepAliveRegistry::GetInstance()->IsKeepingAlive();
}

void BrowserProcessImpl::RestartBackgroundInstance() {
  base::CommandLine* old_cl = base::CommandLine::ForCurrentProcess();
  auto new_cl = std::make_unique<base::CommandLine>(old_cl->GetProgram());

  base::CommandLine::SwitchMap switches = old_cl->GetSwitches();
  switches::RemoveSwitchesForAutostart(&switches);

  // Append the rest of the switches (along with their values, if any)
  // to the new command line
  for (const auto& it : switches) {
    const auto& switch_name = it.first;
    const auto& switch_value = it.second;
    if (switch_value.empty())
      new_cl->AppendSwitch(switch_name);
    else
      new_cl->AppendSwitchNative(switch_name, switch_value);
  }

  // Switches to add when auto-restarting Chrome.
  static constexpr const char* kSwitchesToAddOnAutorestart[] = {
      switches::kNoStartupWindow};

  // Ensure that our desired switches are set on the new process.
  for (const char* switch_to_add : kSwitchesToAddOnAutorestart) {
    if (!new_cl->HasSwitch(switch_to_add))
      new_cl->AppendSwitch(switch_to_add);
  }

#if BUILDFLAG(IS_WIN)
  new_cl->AppendArg(switches::kPrefetchArgumentBrowserBackground);
#endif  // BUILDFLAG(IS_WIN)

  DLOG(WARNING) << "Shutting down current instance of the browser.";
  chrome::AttemptExit();

  upgrade_util::SetNewCommandLine(std::move(new_cl));
}

void BrowserProcessImpl::OnAutoupdateTimer() {
  if (IsRunningInBackground()) {
    // upgrade_util::IsUpdatePendingRestart touches the disk, so do it on a
    // suitable thread.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()},
        base::BindOnce(&upgrade_util::IsUpdatePendingRestart),
        base::BindOnce(&BrowserProcessImpl::OnPendingRestartResult,
                       base::Unretained(this)));
  }
}

void BrowserProcessImpl::OnPendingRestartResult(
    bool is_update_pending_restart) {
  // Make sure that the browser is still in the background after returning from
  // the check.
  if (is_update_pending_restart && IsRunningInBackground()) {
    DLOG(WARNING) << "Detected update.  Restarting browser.";
    RestartBackgroundInstance();
  }
}
#endif  // BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_LACROS))
