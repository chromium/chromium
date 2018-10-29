// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_service_client.h"

#include <stddef.h>

#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_stability_metrics_provider.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_profile_session_durations_service_factory.h"
#include "chrome/browser/metrics/https_engagement_metrics_provider.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/metrics/network_quality_estimator_provider_impl.h"
#include "chrome/browser/metrics/persistent_histograms.h"
#include "chrome/browser/metrics/process_memory_metrics_emitter.h"
#include "chrome/browser/metrics/sampling_metrics_provider.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/certificate_reporting_metrics_provider.h"
#include "chrome/browser/sync/chrome_sync_client.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/translate/translate_ranker_metrics_provider.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/util_constants.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/browser_watcher/stability_paths.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/history/core/browser/history_service.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/metrics/component_metrics_provider.h"
#include "components/metrics/drive_metrics_provider.h"
#include "components/metrics/field_trials_provider.h"
#include "components/metrics/gpu/gpu_metrics_provider.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics/net/cellular_logic_helper.h"
#include "components/metrics/net/net_metrics_log_uploader.h"
#include "components/metrics/net/network_metrics_provider.h"
#include "components/metrics/stability_metrics_helper.h"
#include "components/metrics/ui/screen_info_metrics_provider.h"
#include "components/metrics/url_constants.h"
#include "components/metrics/version_utils.h"
#include "components/omnibox/browser/omnibox_metrics_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/sync/device_info/device_count_metrics_provider.h"
#include "components/ukm/ukm_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/histogram_fetcher.h"
#include "content/public/browser/notification_service.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "chrome/browser/metrics/android_metrics_provider.h"
#include "chrome/browser/metrics/page_load_metrics_provider.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#endif

#if defined(OS_POSIX)
#include <signal.h>
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && !defined(OS_CHROMEOS)
#include "chrome/browser/service_process/service_process_control.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/metrics/extensions_metrics_provider.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/metrics/plugin_metrics_provider.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/printing/printer_metrics_provider.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/metrics/chromeos_metrics_provider.h"
#include "chrome/browser/signin/signin_status_metrics_provider_chromeos.h"
#endif

#if defined(OS_WIN)
#include <windows.h>

#include "chrome/browser/metrics/antivirus_metrics_provider_win.h"
#include "chrome/browser/metrics/google_update_metrics_provider_win.h"
#include "chrome/common/metrics_constants_util_win.h"
#include "chrome/install_static/install_util.h"
#include "chrome/notification_helper/notification_helper_constants.h"
#include "components/browser_watcher/watcher_metrics_provider_win.h"
#endif

#if defined(OS_WIN) || defined(OS_MACOSX)
#include "third_party/crashpad/crashpad/client/crashpad_info.h"
#endif

#if !defined(OS_CHROMEOS)
#include "chrome/browser/signin/chrome_signin_status_metrics_provider_delegate.h"
#include "components/signin/core/browser/signin_status_metrics_provider.h"
#endif  // !defined(OS_CHROMEOS)

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
#include "chrome/browser/metrics/upgrade_metrics_provider.h"
#endif  //  !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
#include "chrome/browser/metrics/power_metrics_provider_mac.h"
#endif

namespace {

#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
const int kMaxHistogramStorageKiB = 100 << 10;  // 100 MiB
#else
const int kMaxHistogramStorageKiB = 500 << 10;  // 500 MiB
#endif

// This specifies the amount of time to wait for all renderers to send their
// data.
const int kMaxHistogramGatheringWaitDuration = 60000;  // 60 seconds.

// Needs to be kept in sync with the writer in
// third_party/crashpad/crashpad/handler/handler_main.cc.
const char kCrashpadHistogramAllocatorName[] = "CrashpadMetrics";

#if defined(OS_WIN) || defined(OS_MACOSX)
// The stream type assigned to the minidump stream that holds the serialized
// system profile proto.
const uint32_t kSystemProfileMinidumpStreamType = 0x4B6B0003;

// A serialized environment (SystemProfileProto) that was registered with the
// crash reporter, or the empty string if no environment was registered yet.
// Ownership must be maintained after registration as the crash reporter does
// not assume it.
// TODO(manzagop): revisit this if the Crashpad API evolves.
base::LazyInstance<std::string>::Leaky g_environment_for_crash_reporter;
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

void RegisterFileMetricsPreferences(PrefRegistrySimple* registry) {
  metrics::FileMetricsProvider::RegisterPrefs(registry, kBrowserMetricsName);

  metrics::FileMetricsProvider::RegisterPrefs(registry,
                                              kCrashpadHistogramAllocatorName);

#if defined(OS_WIN)
  metrics::FileMetricsProvider::RegisterPrefs(
      registry, installer::kSetupHistogramAllocatorName);

  metrics::FileMetricsProvider::RegisterPrefs(
      registry, notification_helper::kNotificationHelperHistogramAllocatorName);
#endif
}

// Constructs the name of a persistent metrics file from a directory and metrics
// name, and either registers that file as associated with a previous run if
// metrics reporting is enabled, or deletes it if not.
void RegisterOrRemovePreviousRunMetricsFile(
    bool metrics_reporting_enabled,
    const base::FilePath& dir,
    base::StringPiece metrics_name,
    metrics::FileMetricsProvider::SourceAssociation association,
    metrics::FileMetricsProvider* file_metrics_provider) {
  base::FilePath metrics_file;
  base::GlobalHistogramAllocator::ConstructFilePaths(
      dir, metrics_name, &metrics_file, nullptr, nullptr);

  if (metrics_reporting_enabled) {
    // Enable reading any existing saved metrics.
    file_metrics_provider->RegisterSource(metrics::FileMetricsProvider::Params(
        metrics_file,
        metrics::FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
        association, metrics_name));
  } else {
    // When metrics reporting is not enabled, any existing file should be
    // deleted in order to preserve user privacy.
    base::PostTaskWithTraits(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(base::IgnoreResult(&base::DeleteFile), metrics_file,
                       /*recursive=*/false));
  }
}

std::unique_ptr<metrics::FileMetricsProvider> CreateFileMetricsProvider(
    bool metrics_reporting_enabled) {
  // Create an object to monitor files of metrics and include them in reports.
  std::unique_ptr<metrics::FileMetricsProvider> file_metrics_provider(
      new metrics::FileMetricsProvider(g_browser_process->local_state()));

  base::FilePath user_data_dir;
  if (base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    // Register the Crashpad metrics files.
    // Register the data from the previous run if crashpad_handler didn't exit
    // cleanly.
    RegisterOrRemovePreviousRunMetricsFile(
        metrics_reporting_enabled, user_data_dir,
        kCrashpadHistogramAllocatorName,
        metrics::FileMetricsProvider::
            ASSOCIATE_INTERNAL_PROFILE_OR_PREVIOUS_RUN,
        file_metrics_provider.get());

    base::FilePath browser_metrics_upload_dir =
        user_data_dir.AppendASCII(kBrowserMetricsName);
    if (metrics_reporting_enabled) {
      metrics::FileMetricsProvider::Params browser_metrics_params(
          browser_metrics_upload_dir,
          metrics::FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_DIR,
          metrics::FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE,
          kBrowserMetricsName);
      browser_metrics_params.max_dir_kib = kMaxHistogramStorageKiB;
      browser_metrics_params.filter = base::BindRepeating(
          &ChromeMetricsServiceClient::FilterBrowserMetricsFiles);
      file_metrics_provider->RegisterSource(browser_metrics_params);

      base::FilePath active_path;
      base::GlobalHistogramAllocator::ConstructFilePaths(
          user_data_dir, kCrashpadHistogramAllocatorName, nullptr, &active_path,
          nullptr);
      // Register data that will be populated for the current run. "Active"
      // files need an empty "prefs_key" because they update the file itself.
      file_metrics_provider->RegisterSource(
          metrics::FileMetricsProvider::Params(
              active_path,
              metrics::FileMetricsProvider::SOURCE_HISTOGRAMS_ACTIVE_FILE,
              metrics::FileMetricsProvider::ASSOCIATE_CURRENT_RUN));
    } else {
      // When metrics reporting is not enabled, any existing files should be
      // deleted in order to preserve user privacy.
      base::PostTaskWithTraits(
          FROM_HERE,
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
          base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                         std::move(browser_metrics_upload_dir),
                         /*recursive=*/true));
    }
  }

#if defined(OS_WIN)
  // Read metrics file from setup.exe.
  base::FilePath program_dir;
  base::PathService::Get(base::DIR_EXE, &program_dir);
  file_metrics_provider->RegisterSource(metrics::FileMetricsProvider::Params(
      program_dir.AppendASCII(installer::kSetupHistogramAllocatorName),
      metrics::FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_DIR,
      metrics::FileMetricsProvider::ASSOCIATE_CURRENT_RUN,
      installer::kSetupHistogramAllocatorName));

  // When metrics reporting is enabled, register the notification_helper metrics
  // files; otherwise delete any existing files in order to preserve user
  // privacy.
  // TODO(chengx): Investigate if there is a need to update
  // RegisterOrRemovePreviousRunMetricsFile and apply it here to remove
  // potential duplicate code.
  if (!user_data_dir.empty()) {
    base::FilePath notification_helper_metrics_upload_dir =
        user_data_dir.AppendASCII(
            notification_helper::kNotificationHelperHistogramAllocatorName);

    if (metrics_reporting_enabled) {
      file_metrics_provider->RegisterSource(
          metrics::FileMetricsProvider::Params(
              notification_helper_metrics_upload_dir,
              metrics::FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_DIR,
              metrics::FileMetricsProvider::ASSOCIATE_CURRENT_RUN,
              notification_helper::kNotificationHelperHistogramAllocatorName));
    } else {
      base::PostTaskWithTraits(
          FROM_HERE,
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
          base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                         std::move(notification_helper_metrics_upload_dir),
                         /*recursive=*/true));
    }
  }
#endif

  return file_metrics_provider;
}

#if defined(OS_WIN)
void GetExecutableVersionDetails(base::string16* product_name,
                                 base::string16* version_number,
                                 base::string16* channel_name) {
  DCHECK_NE(nullptr, product_name);
  DCHECK_NE(nullptr, version_number);
  DCHECK_NE(nullptr, channel_name);

  wchar_t exe_file[MAX_PATH] = {};
  CHECK(::GetModuleFileName(nullptr, exe_file, arraysize(exe_file)));

  base::string16 unused_special_build;
  install_static::GetExecutableVersionDetails(
      exe_file, product_name, version_number, &unused_special_build,
      channel_name);
}
#endif  // OS_WIN

#if defined(OS_ANDROID)
class AndroidIncognitoObserver : public TabModelListObserver {
 public:
  explicit AndroidIncognitoObserver(ChromeMetricsServiceClient* parent)
      : parent_(parent) {
    TabModelList::AddObserver(this);
  }

  ~AndroidIncognitoObserver() override { TabModelList::RemoveObserver(this); }

  void OnTabModelAdded() override { parent_->UpdateRunningServices(); }

  void OnTabModelRemoved() override { parent_->UpdateRunningServices(); }

 private:
  ChromeMetricsServiceClient* parent_;
};
#endif

ChromeMetricsServiceClient::IsProcessRunningFunction g_is_process_running =
    nullptr;

bool IsProcessRunning(base::ProcessId pid) {
  // Use any "override" method if one is set (for testing).
  if (g_is_process_running)
    return g_is_process_running(pid);

#if defined(OS_WIN)
  HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
  if (process) {
    DWORD ret = WaitForSingleObject(process, 0);
    CloseHandle(process);
    if (ret == WAIT_TIMEOUT)
      return true;
  }
#elif defined(OS_POSIX)
  // Sending a signal value of 0 will cause error checking to be performed
  // with no signal being sent.
  if (kill(pid, 0) == 0 || errno != ESRCH)
    return true;
#else
#error Unsupported OS. Might be okay to just return false.
#endif

  return false;
}

}  // namespace

// UKM suffix for field trial recording.
const char kUKMFieldTrialSuffix[] = "UKM";

ChromeMetricsServiceClient::ChromeMetricsServiceClient(
    metrics::MetricsStateManager* state_manager)
    : metrics_state_manager_(state_manager),
      waiting_for_collect_final_metrics_step_(false),
      num_async_histogram_fetches_in_progress_(0),
#if BUILDFLAG(ENABLE_PLUGINS)
      plugin_metrics_provider_(nullptr),
#endif
      weak_ptr_factory_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordCommandLineMetrics();
  notification_listeners_active_ = RegisterForNotifications();
#if defined(OS_ANDROID)
  incognito_observer_ = std::make_unique<AndroidIncognitoObserver>(this);
  notification_listeners_active_ &= (incognito_observer_ != nullptr);
#endif
}

ChromeMetricsServiceClient::~ChromeMetricsServiceClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
std::unique_ptr<ChromeMetricsServiceClient> ChromeMetricsServiceClient::Create(
    metrics::MetricsStateManager* state_manager) {
  // Perform two-phase initialization so that |client->metrics_service_| only
  // receives pointers to fully constructed objects.
  std::unique_ptr<ChromeMetricsServiceClient> client(
      new ChromeMetricsServiceClient(state_manager));
  client->Initialize();

  return client;
}

// static
void ChromeMetricsServiceClient::RegisterPrefs(PrefRegistrySimple* registry) {
  metrics::MetricsService::RegisterPrefs(registry);
  ukm::UkmService::RegisterPrefs(registry);
  metrics::StabilityMetricsHelper::RegisterPrefs(registry);

  RegisterFileMetricsPreferences(registry);

  metrics::RegisterMetricsReportingStatePrefs(registry);

#if BUILDFLAG(ENABLE_PLUGINS)
  PluginMetricsProvider::RegisterPrefs(registry);
#endif  // BUILDFLAG(ENABLE_PLUGINS)
}

metrics::MetricsService* ChromeMetricsServiceClient::GetMetricsService() {
  return metrics_service_.get();
}

ukm::UkmService* ChromeMetricsServiceClient::GetUkmService() {
  return ukm_service_.get();
}

void ChromeMetricsServiceClient::SetMetricsClientId(
    const std::string& client_id) {
  crash_keys::SetMetricsClientIdFromGUID(client_id);
}

int32_t ChromeMetricsServiceClient::GetProduct() {
  return metrics::ChromeUserMetricsExtension::CHROME;
}

std::string ChromeMetricsServiceClient::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

bool ChromeMetricsServiceClient::GetBrand(std::string* brand_code) {
  return google_brand::GetBrand(brand_code);
}

metrics::SystemProfileProto::Channel ChromeMetricsServiceClient::GetChannel() {
  return metrics::AsProtobufChannel(chrome::GetChannel());
}

std::string ChromeMetricsServiceClient::GetVersionString() {
  return metrics::GetVersionString();
}

void ChromeMetricsServiceClient::OnEnvironmentUpdate(std::string* environment) {
#if defined(OS_WIN) || defined(OS_MACOSX)
  DCHECK(environment);

  // Register the environment with the crash reporter. Note this only registers
  // the first environment, meaning ulterior updates to the environment are not
  // reflected in crash report environments (e.g. fieldtrial information). This
  // approach is due to the Crashpad API at time of implementation (registered
  // data cannot be updated). It would however be unwise to rely on such a
  // mechanism to retrieve the value of the dynamic fields due to the
  // environment update lag. Also note there is a window from startup to this
  // point during which crash reports will not have an environment set.
  if (!g_environment_for_crash_reporter.Get().empty())
    return;

  g_environment_for_crash_reporter.Get() = std::move(*environment);

  crashpad::CrashpadInfo::GetCrashpadInfo()->AddUserDataMinidumpStream(
      kSystemProfileMinidumpStreamType,
      reinterpret_cast<const void*>(
          g_environment_for_crash_reporter.Get().data()),
      g_environment_for_crash_reporter.Get().size());
#endif  // OS_WIN || OS_MACOSX
}

void ChromeMetricsServiceClient::OnLogCleanShutdown() {
#if defined(OS_WIN)
  base::FilePath user_data_dir;
  if (base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    browser_watcher::MarkOwnStabilityFileDeleted(user_data_dir);
#endif  // OS_WIN
}

void ChromeMetricsServiceClient::CollectFinalMetricsForLog(
    const base::Closure& done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  collect_final_metrics_done_callback_ = done_callback;
  CollectFinalHistograms();
}

std::unique_ptr<metrics::MetricsLogUploader>
ChromeMetricsServiceClient::CreateUploader(
    base::StringPiece server_url,
    base::StringPiece insecure_server_url,
    base::StringPiece mime_type,
    metrics::MetricsLogUploader::MetricServiceType service_type,
    const metrics::MetricsLogUploader::UploadCallback& on_upload_complete) {
  return std::make_unique<metrics::NetMetricsLogUploader>(
      g_browser_process->shared_url_loader_factory(), server_url,
      insecure_server_url, mime_type, service_type, on_upload_complete);
}

base::TimeDelta ChromeMetricsServiceClient::GetStandardUploadInterval() {
  return metrics::GetUploadInterval();
}

void ChromeMetricsServiceClient::OnPluginLoadingError(
    const base::FilePath& plugin_path) {
#if BUILDFLAG(ENABLE_PLUGINS)
  plugin_metrics_provider_->LogPluginLoadingError(plugin_path);
#else
  NOTREACHED();
#endif  // BUILDFLAG(ENABLE_PLUGINS)
}

bool ChromeMetricsServiceClient::IsReportingPolicyManaged() {
  return IsMetricsReportingPolicyManaged();
}

metrics::EnableMetricsDefault
ChromeMetricsServiceClient::GetMetricsReportingDefaultState() {
  return metrics::GetMetricsReportingDefaultState(
      g_browser_process->local_state());
}

// static
bool ChromeMetricsServiceClient::IsMetricsReportingForceEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      metrics::switches::kForceEnableMetricsReporting);
}

void ChromeMetricsServiceClient::Initialize() {
  PrefService* local_state = g_browser_process->local_state();

  // Clear deprecated metrics preference for Android.
  // TODO(gayane): Cleanup this code after M60 when the pref would be cleared
  // from clients.
#if defined(OS_ANDROID)
  local_state->ClearPref(prefs::kCrashReportingEnabled);
#endif

  metrics_service_.reset(
      new metrics::MetricsService(metrics_state_manager_, this, local_state));

  RegisterMetricsServiceProviders();

  if (IsMetricsReportingForceEnabled() ||
      base::FeatureList::IsEnabled(ukm::kUkmFeature)) {
    // We only need to restrict to whitelisted Entries if metrics reporting
    // is not forced.
    bool restrict_to_whitelist_entries = !IsMetricsReportingForceEnabled();
    ukm_service_.reset(
        new ukm::UkmService(local_state, this, restrict_to_whitelist_entries));
    ukm_service_->SetIsWebstoreExtensionCallback(
        base::BindRepeating(&IsWebstoreExtension));
    RegisterUKMProviders();
  }
}

void ChromeMetricsServiceClient::RegisterMetricsServiceProviders() {
  PrefService* local_state = g_browser_process->local_state();

  // Gets access to persistent metrics shared by sub-processes.
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<SubprocessMetricsProvider>());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<ExtensionsMetricsProvider>(metrics_state_manager_));
#endif

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::NetworkMetricsProvider>(
          std::make_unique<metrics::NetworkQualityEstimatorProviderImpl>()));

  // Currently, we configure OmniboxMetricsProvider to not log events to UMA
  // if there is a single incognito session visible. In the future, it may
  // be worth revisiting this to still log events from non-incognito sessions.
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<OmniboxMetricsProvider>(
          base::Bind(&chrome::IsIncognitoSessionActive)));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<ChromeStabilityMetricsProvider>(local_state));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::GPUMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::ScreenInfoMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(CreateFileMetricsProvider(
      metrics_state_manager_->IsMetricsReportingEnabled()));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::DriveMetricsProvider>(
          chrome::FILE_LOCAL_STATE));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::CallStackProfileMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::SamplingMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<translate::TranslateRankerMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::ComponentMetricsProvider>(
          g_browser_process->component_updater()));

#if defined(OS_ANDROID)
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<AndroidMetricsProvider>());
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<PageLoadMetricsProvider>());
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<GoogleUpdateMetricsProviderWin>());

  base::FilePath user_data_dir;
  base::FilePath crash_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir) ||
      !base::PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dir)) {
    // If either call fails, then clear both.
    user_data_dir = base::FilePath();
    crash_dir = base::FilePath();
  }
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<browser_watcher::WatcherMetricsProviderWin>(
          chrome::GetBrowserExitCodesRegistryPath(), user_data_dir, crash_dir,
          base::Bind(&GetExecutableVersionDetails)));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<AntiVirusMetricsProvider>());
#endif  // defined(OS_WIN)

#if BUILDFLAG(ENABLE_PLUGINS)
  plugin_metrics_provider_ = new PluginMetricsProvider(local_state);
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(plugin_metrics_provider_));
#endif  // BUILDFLAG(ENABLE_PLUGINS)

#if defined(OS_CHROMEOS)
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<ChromeOSMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<SigninStatusMetricsProviderChromeOS>());

  // Record default UMA state as opt-out for all Chrome OS users, if not
  // recorded yet.
  if (metrics::GetMetricsReportingDefaultState(local_state) ==
      metrics::EnableMetricsDefault::DEFAULT_UNKNOWN) {
    metrics::RecordMetricsReportingDefaultState(
        local_state, metrics::EnableMetricsDefault::OPT_OUT);
  }

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<chromeos::PrinterMetricsProvider>());
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_CHROMEOS)
  metrics_service_->RegisterMetricsProvider(
      SigninStatusMetricsProvider::CreateInstance(
          std::make_unique<ChromeSigninStatusMetricsProviderDelegate>()));
#endif  // !defined(OS_CHROMEOS)

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<syncer::DeviceCountMetricsProvider>(
          base::Bind(&browser_sync::ChromeSyncClient::GetDeviceInfoTrackers)));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<HttpsEngagementMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<CertificateReportingMetricsProvider>());

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<UpgradeMetricsProvider>());
#endif  //! defined(OS_ANDROID) && !defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<PowerMetricsProvider>());
#endif
}

void ChromeMetricsServiceClient::RegisterUKMProviders() {
  ukm_service_->RegisterMetricsProvider(
      std::make_unique<metrics::NetworkMetricsProvider>(
          std::make_unique<metrics::NetworkQualityEstimatorProviderImpl>()));

#if defined(OS_CHROMEOS)
  ukm_service_->RegisterMetricsProvider(
      std::make_unique<ChromeOSMetricsProvider>());
#endif  // !defined(OS_CHROMEOS)

  // TODO(rkaplow): Support synthetic trials for UKM.
  ukm_service_->RegisterMetricsProvider(
      std::make_unique<variations::FieldTrialsProvider>(nullptr,
                                                        kUKMFieldTrialSuffix));
}

void ChromeMetricsServiceClient::CollectFinalHistograms() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Begin the multi-step process of collecting memory usage histograms:
  // First spawn a task to collect the memory details; when that task is
  // finished, it will call OnMemoryDetailCollectionDone. That will in turn
  // call HistogramSynchronization to collect histograms from all renderers and
  // then call OnHistogramSynchronizationDone to continue processing.
  DCHECK(!waiting_for_collect_final_metrics_step_);
  waiting_for_collect_final_metrics_step_ = true;

  base::Closure callback =
      base::Bind(&ChromeMetricsServiceClient::OnMemoryDetailCollectionDone,
                 weak_ptr_factory_.GetWeakPtr());

  scoped_refptr<MetricsMemoryDetails> details(
      new MetricsMemoryDetails(callback));
  details->StartFetch();

  scoped_refptr<ProcessMemoryMetricsEmitter> emitter(
      new ProcessMemoryMetricsEmitter);
  emitter->FetchAndEmitProcessMemoryMetrics();
}

void ChromeMetricsServiceClient::OnMemoryDetailCollectionDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This function should only be called as the callback from an ansynchronous
  // step.
  DCHECK(waiting_for_collect_final_metrics_step_);

  // Create a callback_task for OnHistogramSynchronizationDone.
  base::Closure callback = base::Bind(
      &ChromeMetricsServiceClient::OnHistogramSynchronizationDone,
      weak_ptr_factory_.GetWeakPtr());

  base::TimeDelta timeout =
      base::TimeDelta::FromMilliseconds(kMaxHistogramGatheringWaitDuration);

  DCHECK_EQ(num_async_histogram_fetches_in_progress_, 0);

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && !defined(OS_CHROMEOS)
  num_async_histogram_fetches_in_progress_ = 3;
  // Run requests to service and content in parallel.
  if (!ServiceProcessControl::GetInstance()->GetHistograms(callback, timeout)) {
    // Assume |num_async_histogram_fetches_in_progress_| is not changed by
    // |GetHistograms()|.
    DCHECK_EQ(num_async_histogram_fetches_in_progress_, 3);
    // Assign |num_async_histogram_fetches_in_progress_| above and decrement it
    // here to make code work even if |GetHistograms()| fired |callback|.
    --num_async_histogram_fetches_in_progress_;
  }
#else
  num_async_histogram_fetches_in_progress_ = 2;
#endif

  // Merge histograms from metrics providers into StatisticsRecorder.
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&base::StatisticsRecorder::ImportProvidedHistograms),
      callback);

  // Set up the callback task to call after we receive histograms from all
  // child processes. |timeout| specifies how long to wait before absolutely
  // calling us back on the task.
  content::FetchHistogramsAsynchronously(base::ThreadTaskRunnerHandle::Get(),
                                         callback, timeout);
}

void ChromeMetricsServiceClient::OnHistogramSynchronizationDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This function should only be called as the callback from an ansynchronous
  // step.
  DCHECK(waiting_for_collect_final_metrics_step_);
  DCHECK_GT(num_async_histogram_fetches_in_progress_, 0);

  // Check if all expected requests finished.
  if (--num_async_histogram_fetches_in_progress_ > 0)
    return;

  waiting_for_collect_final_metrics_step_ = false;
  collect_final_metrics_done_callback_.Run();
}

void ChromeMetricsServiceClient::RecordCommandLineMetrics() {
  // Get stats on use of command line.
  const base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());
  size_t common_commands = 0;
  if (command_line->HasSwitch(switches::kUserDataDir)) {
    ++common_commands;
    UMA_HISTOGRAM_COUNTS_100("Chrome.CommandLineDatDirCount", 1);
  }

  if (command_line->HasSwitch(switches::kApp)) {
    ++common_commands;
    UMA_HISTOGRAM_COUNTS_100("Chrome.CommandLineAppModeCount", 1);
  }

  // TODO(rohitrao): Should these be logged on iOS as well?
  // http://crbug.com/375794
  size_t switch_count = command_line->GetSwitches().size();
  UMA_HISTOGRAM_COUNTS_100("Chrome.CommandLineFlagCount", switch_count);
  UMA_HISTOGRAM_COUNTS_100("Chrome.CommandLineUncommonFlagCount",
                           switch_count - common_commands);
}

bool ChromeMetricsServiceClient::RegisterForNotifications() {
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_OPENED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_TAB_PARENTED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_TAB_CLOSING,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_HOST_HANG,
                 content::NotificationService::AllSources());

  omnibox_url_opened_subscription_ =
      OmniboxEventGlobalTracker::GetInstance()->RegisterCallback(
          base::Bind(&ChromeMetricsServiceClient::OnURLOpenedFromOmnibox,
                     base::Unretained(this)));

  // Observe history deletions for all profiles.
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_ADDED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());

  bool all_profiles_succeeded = true;
  for (Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    if (!RegisterForProfileEvents(profile)) {
      all_profiles_succeeded = false;
    }
  }
  return all_profiles_succeeded;
}

bool ChromeMetricsServiceClient::RegisterForProfileEvents(Profile* profile) {
#if defined(OS_CHROMEOS)
  // Ignore the signin and lock screen app profile for sync disables / history
  // deletion.
  if (chromeos::ProfileHelper::IsSigninProfile(profile) ||
      chromeos::ProfileHelper::IsLockScreenAppProfile(profile)) {
    // No listeners, but still a success case.
    return true;
  }
#endif
#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  // This creates the DesktopProfileSessionDurationsServices if it didn't exist
  // already.
  metrics::DesktopProfileSessionDurationsServiceFactory::GetForBrowserContext(
      profile);
#endif

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::IMPLICIT_ACCESS);
  if (!history_service) {
    return false;
  }

  ObserveServiceForDeletions(history_service);

  syncer::SyncService* sync =
      ProfileSyncServiceFactory::GetSyncServiceForBrowserContext(profile);
  if (!sync) {
    return false;
  }
  ObserveServiceForSyncDisables(sync, profile->GetPrefs());
  return true;
}

void ChromeMetricsServiceClient::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (type) {
    case chrome::NOTIFICATION_BROWSER_OPENED:
      // May have opened an incognito window.
      UpdateRunningServices();
      metrics_service_->OnApplicationNotIdle();
      break;
    case chrome::NOTIFICATION_BROWSER_CLOSED:
    case chrome::NOTIFICATION_TAB_PARENTED:
    case chrome::NOTIFICATION_TAB_CLOSING:
    case content::NOTIFICATION_LOAD_STOP:
    case content::NOTIFICATION_LOAD_START:
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED:
    case content::NOTIFICATION_RENDER_WIDGET_HOST_HANG:
      metrics_service_->OnApplicationNotIdle();
      break;

    case chrome::NOTIFICATION_PROFILE_ADDED: {
      bool success =
          RegisterForProfileEvents(content::Source<Profile>(source).ptr());
      if (!success && notification_listeners_active_) {
        notification_listeners_active_ = false;
        UpdateRunningServices();
      }
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTROYED:
      // May have closed last incognito window.
      UpdateRunningServices();
      break;

    default:
      NOTREACHED();
  }
}

void ChromeMetricsServiceClient::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  metrics_service_->OnApplicationNotIdle();
}

bool ChromeMetricsServiceClient::IsUMACellularUploadLogicEnabled() {
  return metrics::IsCellularLogicEnabled();
}

void ChromeMetricsServiceClient::OnHistoryDeleted() {
  if (ukm_service_)
    ukm_service_->Purge();
}

void ChromeMetricsServiceClient::OnSyncPrefsChanged(bool must_purge) {
  if (!ukm_service_)
    return;
  if (must_purge) {
    ukm_service_->Purge();
    ukm_service_->ResetClientState(ukm::ResetReason::kOnSyncPrefsChanged);
  }
  // Signal service manager to enable/disable UKM based on new state.
  UpdateRunningServices();
}

// static
bool ChromeMetricsServiceClient::IsWebstoreExtension(base::StringPiece id) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Only acceptable if at least one profile knows the extension and all
  // profiles that know the extension say it was from the web-store.
  bool matched = false;
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  DCHECK(profile_manager);
  auto profiles = profile_manager->GetLoadedProfiles();
  for (Profile* profile : profiles) {
    DCHECK(profile);
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile);
    if (!registry)
      continue;
    const extensions::Extension* extension = registry->GetExtensionById(
        id.as_string(), extensions::ExtensionRegistry::ENABLED);
    if (!extension)
      continue;
    if (!extension->from_webstore())
      return false;
    matched = true;
  }
  return matched;
#else
  return false;
#endif
}

// static
metrics::FileMetricsProvider::FilterAction
ChromeMetricsServiceClient::FilterBrowserMetricsFiles(
    const base::FilePath& path) {
  base::ProcessId pid;
  if (!base::GlobalHistogramAllocator::ParseFilePath(path, nullptr, nullptr,
                                                     &pid)) {
    return metrics::FileMetricsProvider::FILTER_PROCESS_FILE;
  }

  if (pid == base::GetCurrentProcId())
    return metrics::FileMetricsProvider::FILTER_ACTIVE_THIS_PID;

  if (IsProcessRunning(pid))
    return metrics::FileMetricsProvider::FILTER_TRY_LATER;

  return metrics::FileMetricsProvider::FILTER_PROCESS_FILE;
}

// static
void ChromeMetricsServiceClient::SetIsProcessRunningForTesting(
    ChromeMetricsServiceClient::IsProcessRunningFunction func) {
  g_is_process_running = func;
}

bool ChromeMetricsServiceClient::SyncStateAllowsUkm() {
  return SyncDisableObserver::SyncStateAllowsUkm();
}

bool ChromeMetricsServiceClient::SyncStateAllowsExtensionUkm() {
  return SyncDisableObserver::SyncStateAllowsExtensionUkm();
}

bool g_notification_listeners_failed = false;
void ChromeMetricsServiceClient::SetNotificationListenerSetupFailedForTesting(
    bool simulate_failure) {
  g_notification_listeners_failed = simulate_failure;
}

bool ChromeMetricsServiceClient::
    AreNotificationListenersEnabledOnAllProfiles() {
  // For testing
  if (g_notification_listeners_failed)
    return false;
  return notification_listeners_active_;
}

std::string ChromeMetricsServiceClient::GetAppPackageName() {
#if defined(OS_ANDROID)
  return base::android::BuildInfo::GetInstance()->package_name();
#endif
  return std::string();
}
