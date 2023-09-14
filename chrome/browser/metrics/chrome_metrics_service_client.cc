// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_service_client.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/metrics/statistics_recorder.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/string_piece.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/metrics/cached_metrics_profile.h"
#include "chrome/browser/metrics/chrome_browser_main_extra_parts_metrics.h"
#include "chrome/browser/metrics/chrome_metrics_extensions_helper.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/browser/metrics/desktop_platform_features_metrics_provider.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_profile_session_durations_service_factory.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_metrics_provider.h"
#include "chrome/browser/metrics/https_engagement_metrics_provider.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/metrics/network_quality_estimator_provider_impl.h"
#include "chrome/browser/metrics/usertype_by_devicetype_metrics_provider.h"
#include "chrome/browser/performance_manager/metrics/metrics_provider_common.h"
#include "chrome/browser/privacy_budget/privacy_budget_metrics_provider.h"
#include "chrome/browser/privacy_budget/privacy_budget_prefs.h"
#include "chrome/browser/privacy_budget/privacy_budget_ukm_entry_filter.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/certificate_reporting_metrics_provider.h"
#include "chrome/browser/safe_browsing/metrics/safe_browsing_metrics_provider.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/tracing/chrome_background_tracing_metrics_provider.h"
#include "chrome/browser/translate/translate_ranker_metrics_provider.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/history/core/browser/history_service.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/metrics/component_metrics_provider.h"
#include "components/metrics/content/content_stability_metrics_provider.h"
#include "components/metrics/content/gpu_metrics_provider.h"
#include "components/metrics/content/rendering_perf_metrics_provider.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/metrics/cpu_metrics_provider.h"
#include "components/metrics/demographics/demographic_metrics_provider.h"
#include "components/metrics/drive_metrics_provider.h"
#include "components/metrics/entropy_state_provider.h"
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
#include "components/metrics/persistent_histograms.h"
#include "components/metrics/persistent_synthetic_trial_observer.h"
#include "components/metrics/sampling_metrics_provider.h"
#include "components/metrics/stability_metrics_helper.h"
#include "components/metrics/ui/form_factor_metrics_provider.h"
#include "components/metrics/ui/screen_info_metrics_provider.h"
#include "components/metrics/url_constants.h"
#include "components/metrics/version_utils.h"
#include "components/network_time/network_time_tracker.h"
#include "components/omnibox/browser/omnibox_metrics_provider.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/sync/service/passphrase_type_metrics_provider.h"
#include "components/sync/service/sync_service.h"
#include "components/sync_device_info/device_count_metrics_provider.h"
#include "components/ukm/field_trials_provider_helper.h"
#include "components/ukm/ukm_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/histogram_fetcher.h"
#include "content/public/browser/network_service_instance.h"
#include "google_apis/google_api_keys.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/metrics/chrome_android_metrics_provider.h"
#include "chrome/browser/metrics/page_load_metrics_provider.h"
#include "components/metrics/android_metrics_provider.h"
#else
#include "chrome/browser/metrics/browser_activity_watcher.h"
#include "chrome/browser/performance_manager/metrics/metrics_provider_desktop.h"
#endif

#if BUILDFLAG(IS_POSIX)
#include <signal.h>
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/metrics/extensions_metrics_provider.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/metrics/lacros_metrics_provider.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/printing/printer_metrics_provider.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/keyboard_backlight_color_metrics_provider.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_theme_metrics_provider.h"
#include "chrome/browser/metrics/ambient_mode_metrics_provider.h"
#include "chrome/browser/metrics/assistant_service_metrics_provider.h"
#include "chrome/browser/metrics/chromeos_family_link_user_metrics_provider.h"
#include "chrome/browser/metrics/chromeos_metrics_provider.h"
#include "chrome/browser/metrics/chromeos_system_profile_provider.h"
#include "chrome/browser/metrics/cros_healthd_metrics_provider.h"
#include "chrome/browser/metrics/family_user_metrics_provider.h"
#include "chrome/browser/metrics/per_user_state_manager_chromeos.h"
#include "chrome/browser/metrics/update_engine_metrics_provider.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_metrics_provider.h"
#include "components/metrics/structured/structured_metrics_features.h"  // nogncheck
#include "components/metrics/structured/structured_metrics_provider.h"  // nogncheck
#include "components/metrics/structured/structured_metrics_service.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "chrome/browser/metrics/antivirus_metrics_provider_win.h"
#include "chrome/browser/metrics/google_update_metrics_provider_win.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/notification_helper/notification_helper_constants.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
#include "components/metrics/motherboard_metrics_provider.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#include "third_party/crashpad/crashpad/client/crashpad_info.h"  // nogncheck
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/signin/chrome_signin_and_sync_status_metrics_provider.h"
#include "components/metrics/content/accessibility_metrics_provider.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/metrics/upgrade_metrics_provider.h"
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/metrics/power/power_metrics_provider_mac.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/metrics/bluetooth_metrics_provider.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_ANDROID)
#include "chrome/browser/metrics/family_link_user_metrics_provider.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_LACROS))||BUILDFLAG(IS_ANDROID))

namespace {

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
// The stream type assigned to the minidump stream that holds the serialized
// system profile proto.
const uint32_t kSystemProfileMinidumpStreamType = 0x4B6B0003;

// A serialized environment (SystemProfileProto) that was registered with the
// crash reporter, or the empty string if no environment was registered yet.
// Ownership must be maintained after registration as the crash reporter does
// not assume it.
// TODO(manzagop): revisit this if the Crashpad API evolves.
base::LazyInstance<std::string>::Leaky g_environment_for_crash_reporter =
    LAZY_INSTANCE_INITIALIZER;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)

void RegisterFileMetricsPreferences(PrefRegistrySimple* registry) {
  metrics::FileMetricsProvider::RegisterSourcePrefs(registry,
                                                    kBrowserMetricsName);

  metrics::FileMetricsProvider::RegisterSourcePrefs(
      registry, kDeferredBrowserMetricsName);

  metrics::FileMetricsProvider::RegisterSourcePrefs(
      registry, kCrashpadHistogramAllocatorName);

#if BUILDFLAG(IS_WIN)
  metrics::FileMetricsProvider::RegisterSourcePrefs(
      registry, installer::kSetupHistogramAllocatorName);

  metrics::FileMetricsProvider::RegisterSourcePrefs(
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
  base::FilePath metrics_file =
      base::GlobalHistogramAllocator::ConstructFilePath(dir, metrics_name);

  if (metrics_reporting_enabled) {
    // Enable reading any existing saved metrics.
    file_metrics_provider->RegisterSource(metrics::FileMetricsProvider::Params(
        metrics_file,
        metrics::FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
        association, metrics_name));
  } else {
    // When metrics reporting is not enabled, any existing file should be
    // deleted in order to preserve user privacy.
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::GetDeleteFileCallback(metrics_file));
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
    base::FilePath deferred_browser_metrics_upload_dir =
        user_data_dir.AppendASCII(kDeferredBrowserMetricsName);
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

      metrics::FileMetricsProvider::Params deferred_browser_metrics_params(
          deferred_browser_metrics_upload_dir,
          metrics::FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_DIR,
          metrics::FileMetricsProvider::ASSOCIATE_CURRENT_RUN,
          kDeferredBrowserMetricsName);
      deferred_browser_metrics_params.max_dir_kib = kMaxHistogramStorageKiB;
      file_metrics_provider->RegisterSource(deferred_browser_metrics_params);

      base::FilePath crashpad_active_path =
          base::GlobalHistogramAllocator::ConstructFilePathForActiveFile(
              user_data_dir, kCrashpadHistogramAllocatorName);
      // Register data that will be populated for the current run. "Active"
      // files need an empty "prefs_key" because they update the file itself.
      file_metrics_provider->RegisterSource(
          metrics::FileMetricsProvider::Params(
              crashpad_active_path,
              metrics::FileMetricsProvider::SOURCE_HISTOGRAMS_ACTIVE_FILE,
              metrics::FileMetricsProvider::ASSOCIATE_CURRENT_RUN));
    } else {
      // When metrics reporting is not enabled, any existing files should be
      // deleted in order to preserve user privacy.
      base::ThreadPool::PostTask(
          FROM_HERE,
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
          base::GetDeletePathRecursivelyCallback(
              std::move(browser_metrics_upload_dir)));
      base::ThreadPool::PostTask(
          FROM_HERE,
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
          base::GetDeletePathRecursivelyCallback(
              std::move(deferred_browser_metrics_upload_dir)));
    }
  }

#if BUILDFLAG(IS_WIN)
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
      base::ThreadPool::PostTask(
          FROM_HERE,
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
          base::GetDeletePathRecursivelyCallback(
              std::move(notification_helper_metrics_upload_dir)));
    }
  }
#endif

  return file_metrics_provider;
}

ChromeMetricsServiceClient::IsProcessRunningFunction g_is_process_running =
    nullptr;

bool IsProcessRunning(base::ProcessId pid) {
  // Use any "override" method if one is set (for testing).
  if (g_is_process_running) {
    return g_is_process_running(pid);
  }

#if BUILDFLAG(IS_WIN)
  HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
  if (process) {
    DWORD ret = WaitForSingleObject(process, 0);
    CloseHandle(process);
    if (ret == WAIT_TIMEOUT) {
      return true;
    }
  }
#elif BUILDFLAG(IS_POSIX)
  // Sending a signal value of 0 will cause error checking to be performed
  // with no signal being sent.
  if (kill(pid, 0) == 0 || errno != ESRCH) {
    return true;
  }
#elif BUILDFLAG(IS_FUCHSIA)
  // TODO(crbug.com/967028): Implement along with metrics support.
  NOTIMPLEMENTED_LOG_ONCE();
#else
#error Unsupported OS. Might be okay to just return false.
#endif

  return false;
}

// Client used by DemographicMetricsProvider to retrieve Profile information.
class ProfileClientImpl
    : public metrics::DemographicMetricsProvider::ProfileClient {
 public:
  ProfileClientImpl() = default;
  ProfileClientImpl(const ProfileClientImpl&) = delete;
  ProfileClientImpl& operator=(const ProfileClientImpl&) = delete;
  ~ProfileClientImpl() override = default;

  int GetNumberOfProfilesOnDisk() override {
    return g_browser_process->profile_manager()->GetNumberOfProfiles();
  }

  PrefService* GetLocalState() override {
    return g_browser_process->local_state();
  }

  PrefService* GetProfilePrefs() override {
    Profile* profile = cached_metrics_profile_.GetMetricsProfile();
    if (!profile) {
      return nullptr;
    }

    return profile->GetPrefs();
  }

  syncer::SyncService* GetSyncService() override {
    Profile* profile = cached_metrics_profile_.GetMetricsProfile();
    if (!profile) {
      return nullptr;
    }

    return SyncServiceFactory::GetForProfile(profile);
  }

  base::Time GetNetworkTime() const override {
    base::Time time;
    if (g_browser_process->network_time_tracker()->GetNetworkTime(&time,
                                                                  nullptr) !=
        network_time::NetworkTimeTracker::NETWORK_TIME_AVAILABLE) {
      // Return null time to indicate that it could not get the network time. It
      // is the responsibility of the client to have the strategy to deal with
      // the absence of network time.
      return base::Time();
    }
    return time;
  }

 private:
  // Provides the same cached Profile each time.
  metrics::CachedMetricsProfile cached_metrics_profile_;
};

std::unique_ptr<metrics::DemographicMetricsProvider>
MakeDemographicMetricsProvider(
    metrics::MetricsLogUploader::MetricServiceType metrics_service_type) {
  return std::make_unique<metrics::DemographicMetricsProvider>(
      std::make_unique<ProfileClientImpl>(), metrics_service_type);
}

class ChromeComponentMetricsProviderDelegate
    : public metrics::ComponentMetricsProviderDelegate {
 public:
  explicit ChromeComponentMetricsProviderDelegate(
      component_updater::ComponentUpdateService* component_updater_service)
      : component_updater_service_(component_updater_service) {}
  ~ChromeComponentMetricsProviderDelegate() override = default;

  std::vector<component_updater::ComponentInfo> GetComponents() override {
    return component_updater_service_->GetComponents();
  }

 private:
  raw_ptr<component_updater::ComponentUpdateService> component_updater_service_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// TODO(crbug/1295789): Remove this and use ChangeMetricsReportingState() once
// crash no longer depends on GoogleUpdateSettings and per-user is available
// outside of Ash.
void UpdateMetricsServicesForPerUser(bool enabled) {
  // Set the local state pref because a lot of services read directly from this
  // pref to obtain metrics consent.
  //
  // This is OK on Chrome OS because this pref is set on every startup with the
  // device policy value. The previous user consent will get overwritten by
  // the correct device policy value on startup.
  //
  // TODO(crbug/1297765): Once a proper API is established and services no
  // longer read the pref value directly, this can be removed.
  g_browser_process->local_state()->SetBoolean(
      metrics::prefs::kMetricsReportingEnabled, enabled);

  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(true);
}
#endif

}  // namespace

ChromeMetricsServiceClient::ChromeMetricsServiceClient(
    metrics::MetricsStateManager* state_manager)
    : metrics_state_manager_(state_manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  incognito_observer_ = IncognitoObserver::Create(
      base::BindRepeating(&ChromeMetricsServiceClient::UpdateRunningServices,
                          weak_ptr_factory_.GetWeakPtr()));
}

ChromeMetricsServiceClient::~ChromeMetricsServiceClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
std::unique_ptr<ChromeMetricsServiceClient> ChromeMetricsServiceClient::Create(
    metrics::MetricsStateManager* state_manager) {
  // Perform two-phase initialization so that `client->metrics_service_` only
  // receives pointers to fully constructed objects.
  std::unique_ptr<ChromeMetricsServiceClient> client(
      new ChromeMetricsServiceClient(state_manager));
  client->Initialize();

  return client;
}

// static
void ChromeMetricsServiceClient::RegisterPrefs(PrefRegistrySimple* registry) {
  ChromeBrowserMainExtraPartsMetrics::RegisterPrefs(registry);
  metrics::MetricsService::RegisterPrefs(registry);
  ukm::UkmService::RegisterPrefs(registry);
  metrics::StabilityMetricsHelper::RegisterPrefs(registry);
  prefs::RegisterPrivacyBudgetPrefs(registry);

  RegisterFileMetricsPreferences(registry);

  metrics::RegisterMetricsReportingStatePrefs(registry);

#if BUILDFLAG(IS_ANDROID)
  ChromeAndroidMetricsProvider::RegisterPrefs(registry);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  metrics::PerUserStateManagerChromeOS::RegisterPrefs(registry);
  metrics::structured::StructuredMetricsService::RegisterPrefs(registry);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ChromeMetricsServiceClient::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  metrics::PerUserStateManagerChromeOS::RegisterProfilePrefs(registry);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

variations::SyntheticTrialRegistry*
ChromeMetricsServiceClient::GetSyntheticTrialRegistry() {
  return synthetic_trial_registry_.get();
}

metrics::MetricsService* ChromeMetricsServiceClient::GetMetricsService() {
  return metrics_service_.get();
}

ukm::UkmService* ChromeMetricsServiceClient::GetUkmService() {
  return ukm_service_.get();
}

metrics::structured::StructuredMetricsService*
ChromeMetricsServiceClient::GetStructuredMetricsService() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return structured_metrics_service_.get();
#else
  return nullptr;
#endif
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

const network_time::NetworkTimeTracker*
ChromeMetricsServiceClient::GetNetworkTimeTracker() {
  return g_browser_process->network_time_tracker();
}

bool ChromeMetricsServiceClient::GetBrand(std::string* brand_code) {
  return google_brand::GetBrand(brand_code);
}

metrics::SystemProfileProto::Channel ChromeMetricsServiceClient::GetChannel() {
  return metrics::AsProtobufChannel(chrome::GetChannel());
}

bool ChromeMetricsServiceClient::IsExtendedStableChannel() {
  return chrome::IsExtendedStableChannel();
}

std::string ChromeMetricsServiceClient::GetVersionString() {
  return metrics::GetVersionString();
}

void ChromeMetricsServiceClient::OnEnvironmentUpdate(std::string* environment) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  DCHECK(environment);

  // Register the environment with the crash reporter. Note this only registers
  // the first environment, meaning ulterior updates to the environment are not
  // reflected in crash report environments (e.g. fieldtrial information). This
  // approach is due to the Crashpad API at time of implementation (registered
  // data cannot be updated). It would however be unwise to rely on such a
  // mechanism to retrieve the value of the dynamic fields due to the
  // environment update lag. Also note there is a window from startup to this
  // point during which crash reports will not have an environment set.
  if (!g_environment_for_crash_reporter.Get().empty()) {
    return;
  }

  g_environment_for_crash_reporter.Get() = std::move(*environment);

  crashpad::CrashpadInfo::GetCrashpadInfo()->AddUserDataMinidumpStream(
      kSystemProfileMinidumpStreamType,
      reinterpret_cast<const void*>(
          g_environment_for_crash_reporter.Get().data()),
      g_environment_for_crash_reporter.Get().size());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
}

void ChromeMetricsServiceClient::MergeSubprocessHistograms() {
  // TODO(crbug.com/1293026): Move this to a shared place to not have to
  // duplicate the code across different `MetricsServiceClient`s.

  // Synchronously fetch subprocess histograms that live in shared memory.
  base::StatisticsRecorder::ImportProvidedHistogramsSync();

  // Asynchronously fetch subprocess histograms that do not live in shared
  // memory (e.g., they were emitted before the shared memory was set up).
  content::FetchHistogramsAsynchronously(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      /*callback=*/base::DoNothing(),
      /*wait_time=*/base::Milliseconds(kMaxHistogramGatheringWaitDuration));
}

void ChromeMetricsServiceClient::CollectFinalMetricsForLog(
    base::OnceClosure done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  collect_final_metrics_done_callback_ = std::move(done_callback);
  CollectFinalHistograms();
}

std::unique_ptr<metrics::MetricsLogUploader>
ChromeMetricsServiceClient::CreateUploader(
    const GURL& server_url,
    const GURL& insecure_server_url,
    base::StringPiece mime_type,
    metrics::MetricsLogUploader::MetricServiceType service_type,
    const metrics::MetricsLogUploader::UploadCallback& on_upload_complete) {
  return std::make_unique<metrics::NetMetricsLogUploader>(
      g_browser_process->shared_url_loader_factory(), server_url,
      insecure_server_url, mime_type, service_type, on_upload_complete);
}

base::TimeDelta ChromeMetricsServiceClient::GetStandardUploadInterval() {
  return metrics::GetUploadInterval(metrics::ShouldUseCellularUploadInterval());
}

bool ChromeMetricsServiceClient::IsReportingPolicyManaged() {
  return IsMetricsReportingPolicyManaged();
}

metrics::EnableMetricsDefault
ChromeMetricsServiceClient::GetMetricsReportingDefaultState() {
  return metrics::GetMetricsReportingDefaultState(
      g_browser_process->local_state());
}

void ChromeMetricsServiceClient::Initialize() {
  PrefService* local_state = g_browser_process->local_state();

  synthetic_trial_registry_ =
      std::make_unique<variations::SyntheticTrialRegistry>(
          IsExternalExperimentAllowlistEnabled());

  metrics_service_ = std::make_unique<metrics::MetricsService>(
      metrics_state_manager_, this, local_state);

  observers_active_ = RegisterObservers();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  cros_system_profile_provider_ =
      std::make_unique<ChromeOSSystemProfileProvider>();
  structured_metrics_service_ =
      std::make_unique<metrics::structured::StructuredMetricsService>(
          cros_system_profile_provider_.get(), this, local_state);
#endif

  RegisterMetricsServiceProviders();

  if (IsMetricsReportingForceEnabled() ||
      base::FeatureList::IsEnabled(ukm::kUkmFeature)) {
    identifiability_study_state_ =
        std::make_unique<IdentifiabilityStudyState>(local_state);

    uint64_t client_id = 0;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Read metrics service client id from ash chrome if it's present.
    client_id = chromeos::BrowserParamsProxy::Get()->UkmClientId();
#endif

    ukm_service_ = std::make_unique<ukm::UkmService>(
        local_state, this,
        MakeDemographicMetricsProvider(
            metrics::MetricsLogUploader::MetricServiceType::UKM),
        client_id);
    ukm_service_->SetIsWebstoreExtensionCallback(
        base::BindRepeating(&IsWebstoreExtension));
    ukm_service_->RegisterEventFilter(
        std::make_unique<PrivacyBudgetUkmEntryFilter>(
            identifiability_study_state_.get()));

    RegisterUKMProviders();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  metrics::structured::Recorder::GetInstance()->SetUiTaskRunner(
      base::SequencedTaskRunner::GetCurrentDefault());

  AsyncInitSystemProfileProvider();

  // Set is_demo_mode_ to true in ukm_consent_state_observer if the device is
  // currently in Demo Mode.
  SetIsDemoMode(ash::DemoSession::IsDeviceInDemoMode());
#endif
}

void ChromeMetricsServiceClient::RegisterMetricsServiceProviders() {
  PrefService* local_state = g_browser_process->local_state();

  // Gets access to persistent metrics shared by sub-processes.
  CHECK(metrics::SubprocessMetricsProvider::GetInstance());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<ExtensionsMetricsProvider>(metrics_state_manager_));
#endif

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::NetworkMetricsProvider>(
          content::CreateNetworkConnectionTrackerAsyncGetter(),
          std::make_unique<metrics::NetworkQualityEstimatorProviderImpl>()));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<OmniboxMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::ContentStabilityMetricsProvider>(
          local_state, std::make_unique<ChromeMetricsExtensionsHelper>()));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::GPUMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::RenderingPerfMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::CPUMetricsProvider>());

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::MotherboardMetricsProvider>());
#endif

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::EntropyStateProvider>(local_state));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::ScreenInfoMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::FormFactorMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(CreateFileMetricsProvider(
      metrics_state_manager_->IsMetricsReportingEnabled()));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::DriveMetricsProvider>(
          chrome::FILE_LOCAL_STATE));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::CallStackProfileMetricsProvider>());

  int sample_rate;
  // Only log the sample rate if it's defined.
  if (ChromeMetricsServicesManagerClient::GetSamplingRatePerMille(
          &sample_rate)) {
    metrics_service_->RegisterMetricsProvider(
        std::make_unique<metrics::SamplingMetricsProvider>(sample_rate));
  }

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<translate::TranslateRankerMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::ComponentMetricsProvider>(
          std::make_unique<ChromeComponentMetricsProviderDelegate>(
              g_browser_process->component_updater())));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<tracing::ChromeBackgroundTracingMetricsProvider>(
          reinterpret_cast<ChromeOSSystemProfileProvider*>(
              cros_system_profile_provider_.get())));

  metrics_service_->RegisterMetricsProvider(MakeDemographicMetricsProvider(
      metrics::MetricsLogUploader::MetricServiceType::UMA));

  // TODO(crbug.com/1207574): Add metrics registration for WebView, iOS and
  // WebLayer.
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<safe_browsing::SafeBrowsingMetricsProvider>());

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::BluetoothMetricsProvider>());
#endif

#if BUILDFLAG(IS_ANDROID)
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::AndroidMetricsProvider>());
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<ChromeAndroidMetricsProvider>(local_state));
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<PageLoadMetricsProvider>());
#else
  metrics_service_->RegisterMetricsProvider(base::WrapUnique(
      new performance_manager::MetricsProviderDesktop(local_state)));
#endif  // BUILDFLAG(IS_ANDROID)

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<performance_manager::MetricsProviderCommon>());

#if BUILDFLAG(IS_WIN)
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<GoogleUpdateMetricsProviderWin>());
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<AntiVirusMetricsProvider>());
#endif  // BUILDFLAG(IS_WIN)

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<DesktopPlatformFeaturesMetricsProvider>());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_LACROS))

#if BUILDFLAG(ENABLE_SUPERVISED_USERS) && !BUILDFLAG(IS_CHROMEOS_ASH)
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<FamilyLinkUserMetricsProvider>());
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS) && !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<LacrosMetricsProvider>());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<ChromeOSMetricsProvider>(
          metrics::MetricsLogUploader::UMA,
          reinterpret_cast<ChromeOSSystemProfileProvider*>(
              cros_system_profile_provider_.get())));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<ChromeOSHistogramMetricsProvider>());

  if (base::FeatureList::IsEnabled(::features::kUmaStorageDimensions)) {
    metrics_service_->RegisterMetricsProvider(
        std::make_unique<CrosHealthdMetricsProvider>());
  }

  // Record default UMA state as opt-out for all Chrome OS users, if not
  // recorded yet.
  if (metrics::GetMetricsReportingDefaultState(local_state) ==
      metrics::EnableMetricsDefault::DEFAULT_UNKNOWN) {
    metrics::RecordMetricsReportingDefaultState(
        local_state, metrics::EnableMetricsDefault::OPT_OUT);
  }

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<ash::PrinterMetricsProvider>());

  // TODO(b/282057109) The StructuredMetricsService owns the recorder. While we
  // are transitioning from the provider to the service, the provider will be
  // given a pointer to the recorder. Will be removed when the provider is
  // deprecated.
  if (!base::FeatureList::IsEnabled(
          metrics::structured::kEnabledStructuredMetricsService)) {
    metrics_service_->RegisterMetricsProvider(
        std::make_unique<metrics::structured::StructuredMetricsProvider>(
            structured_metrics_service_->recorder()));
  }

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<AssistantServiceMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<AmbientModeMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<FamilyUserMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<ChromeOSFamilyLinkUserMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<UpdateEngineMetricsProvider>());

  if (base::FeatureList::IsEnabled(
          ::features::kUserTypeByDeviceTypeMetricsProvider)) {
    metrics_service_->RegisterMetricsProvider(
        std::make_unique<UserTypeByDeviceTypeMetricsProvider>());
  }

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<KeyboardBacklightColorMetricsProvider>());
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<PersonalizationAppThemeMetricsProvider>());
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<ash::settings::OsSettingsMetricsProvider>());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<ChromeSigninAndSyncStatusMetricsProvider>());
  // ChromeOS uses ChromeOSMetricsProvider for accessibility metrics provider.
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::AccessibilityMetricsProvider>());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<syncer::DeviceCountMetricsProvider>(base::BindRepeating(
          &DeviceInfoSyncServiceFactory::GetAllDeviceInfoTrackers)));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<syncer::PassphraseTypeMetricsProvider>(
          base::BindRepeating(&SyncServiceFactory::GetAllSyncServices)));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<HttpsEngagementMetricsProvider>());

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<CertificateReportingMetricsProvider>());
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<UpgradeMetricsProvider>());
#endif  //! BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<PowerMetricsProvider>());
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  metrics_service_->RegisterMetricsProvider(
      metrics::CreateDesktopSessionMetricsProvider());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_LINUX)
}

void ChromeMetricsServiceClient::RegisterUKMProviders() {
  // Note: if you make changes here please also consider whether they should go
  // in AndroidMetricsServiceClient::CreateUkmService().
  ukm_service_->RegisterMetricsProvider(
      std::make_unique<metrics::NetworkMetricsProvider>(
          content::CreateNetworkConnectionTrackerAsyncGetter(),
          std::make_unique<metrics::NetworkQualityEstimatorProviderImpl>()));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ukm_service_->RegisterMetricsProvider(
      std::make_unique<ChromeOSMetricsProvider>(
          metrics::MetricsLogUploader::UKM,
          reinterpret_cast<ChromeOSSystemProfileProvider*>(
              cros_system_profile_provider_.get())));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  ukm_service_->RegisterMetricsProvider(
      std::make_unique<LacrosMetricsProvider>());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  ukm_service_->RegisterMetricsProvider(
      std::make_unique<metrics::GPUMetricsProvider>());

  ukm_service_->RegisterMetricsProvider(
      std::make_unique<metrics::CPUMetricsProvider>());

  ukm_service_->RegisterMetricsProvider(
      std::make_unique<metrics::ScreenInfoMetricsProvider>());

  ukm_service_->RegisterMetricsProvider(
      std::make_unique<metrics::FormFactorMetricsProvider>());

  ukm_service_->RegisterMetricsProvider(
      ukm::CreateFieldTrialsProviderForUkm(synthetic_trial_registry_.get()));

  ukm_service_->RegisterMetricsProvider(
      std::make_unique<PrivacyBudgetMetricsProvider>(
          identifiability_study_state_.get()));

  ukm_service_->RegisterMetricsProvider(
      std::make_unique<metrics::ComponentMetricsProvider>(
          std::make_unique<ChromeComponentMetricsProviderDelegate>(
              g_browser_process->component_updater())));
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

  base::OnceClosure callback =
      base::BindOnce(&ChromeMetricsServiceClient::OnMemoryDetailCollectionDone,
                     weak_ptr_factory_.GetWeakPtr());

  auto details =
      base::MakeRefCounted<MetricsMemoryDetails>(std::move(callback));
  details->StartFetch();
}

void ChromeMetricsServiceClient::OnMemoryDetailCollectionDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This function should only be called as the callback from an ansynchronous
  // step.
  DCHECK(waiting_for_collect_final_metrics_step_);

  // Create a callback_task for OnHistogramSynchronizationDone.
  base::RepeatingClosure callback = base::BindRepeating(
      &ChromeMetricsServiceClient::OnHistogramSynchronizationDone,
      weak_ptr_factory_.GetWeakPtr());

  base::TimeDelta timeout =
      base::Milliseconds(kMaxHistogramGatheringWaitDuration);

  DCHECK_EQ(num_async_histogram_fetches_in_progress_, 0);
  // `callback` is used 2 times below.
  num_async_histogram_fetches_in_progress_ = 2;

  // Merge histograms from metrics providers into StatisticsRecorder.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&base::StatisticsRecorder::ImportProvidedHistograms,
                     /*async=*/true, callback));

  // Set up the callback task to call after we receive histograms from all
  // child processes. `timeout` specifies how long to wait before absolutely
  // calling us back on the task.
  content::FetchHistogramsAsynchronously(
      base::SingleThreadTaskRunner::GetCurrentDefault(), std::move(callback),
      timeout);
}

void ChromeMetricsServiceClient::OnHistogramSynchronizationDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This function should only be called as the callback from an ansynchronous
  // step.
  DCHECK(waiting_for_collect_final_metrics_step_);
  DCHECK_GT(num_async_histogram_fetches_in_progress_, 0);

  // Check if all expected requests finished.
  if (--num_async_histogram_fetches_in_progress_ > 0) {
    return;
  }

  waiting_for_collect_final_metrics_step_ = false;
  std::move(collect_final_metrics_done_callback_).Run();
}

bool ChromeMetricsServiceClient::RegisterObservers() {
  omnibox_url_opened_subscription_ =
      OmniboxEventGlobalTracker::GetInstance()->RegisterCallback(
          base::BindRepeating(
              &ChromeMetricsServiceClient::OnURLOpenedFromOmnibox,
              base::Unretained(this)));

#if !BUILDFLAG(IS_ANDROID)
  browser_activity_watcher_ = std::make_unique<BrowserActivityWatcher>(
      base::BindRepeating(&metrics::MetricsService::OnApplicationNotIdle,
                          base::Unretained(metrics_service_.get())));
#endif

  bool all_profiles_succeeded = true;
  for (Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    if (!RegisterForProfileEvents(profile)) {
      all_profiles_succeeded = false;
    }
  }
  profile_manager_observer_.Observe(g_browser_process->profile_manager());

  synthetic_trial_observation_.Observe(synthetic_trial_registry_.get());

  return all_profiles_succeeded;
}

bool ChromeMetricsServiceClient::RegisterForProfileEvents(Profile* profile) {
  // Only Original Profiles are checked in this call, meaning Incognito and
  // Guest Off the record status are not checked in this method for UKM consent.
  // The equivalent check is done in this method
  // `MetricsServicesManager::UpdateUkmService()`
  DCHECK(!profile->IsOffTheRecord());

  // Non-Regular Profiles consent information are not expected to be
  // observed or checked, therefore they are whitelisted for the UKM
  // validation that checks consent on all profiles.
  // E.g System Profile consent should be always true.
  if (!profile->IsRegularProfile()) {
    return true;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Ignore the signin, lock screen app and lock screen profile for sync
  // disables / history deletion.
  if (!ash::ProfileHelper::IsUserProfile(profile)) {
    // No listeners, but still a success case.
    return true;
  }

  // Begin initializing the structured metrics system, which is currently
  // only implemented for Chrome OS. Initialization must wait until a
  // profile is added, because it reads keys stored within the user's
  // cryptohome. We only initialize for profiles that are valid candidates
  // for metrics collection, ignoring the sign-in profile, lock screen app
  // profile, and guest sessions.
  //
  // TODO(crbug.com/1016655): This call would be better placed in
  // metrics::structured::Recorder, but can't be because it depends on Chrome
  // code. Investigate whether there's a way of checking this from the
  // component.
  metrics::structured::Recorder::GetInstance()->ProfileAdded(
      profile->GetPath());

  // If the device is in Demo Mode, observe the sync service to enable UKM to
  // collect app data and return true.
  if (IsDeviceInDemoMode()) {
    syncer::SyncService* sync = SyncServiceFactory::GetForProfile(profile);
    if (!sync) {
      return false;
    }
    StartObserving(sync, profile->GetPrefs());
    return true;
  }
#endif
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
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

  syncer::SyncService* sync = SyncServiceFactory::GetForProfile(profile);
  if (!sync) {
    return false;
  }
  StartObserving(sync, profile->GetPrefs());
  return true;
}

void ChromeMetricsServiceClient::OnProfileAdded(Profile* profile) {
  bool success = RegisterForProfileEvents(profile);
  // On failure, set `observers_active_` to false which will
  // disable UKM reporting via UpdateRunningServices().
  if (!success && observers_active_) {
    observers_active_ = false;
    UpdateRunningServices();
  }
}

void ChromeMetricsServiceClient::OnProfileManagerDestroying() {
  profile_manager_observer_.Reset();
}

void ChromeMetricsServiceClient::LoadingStateChanged(bool /*is_loading*/) {
  metrics_service_->OnApplicationNotIdle();
}

void ChromeMetricsServiceClient::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  metrics_service_->OnApplicationNotIdle();
}

bool ChromeMetricsServiceClient::IsOnCellularConnection() {
  return metrics::ShouldUseCellularUploadInterval();
}

void ChromeMetricsServiceClient::OnHistoryDeleted() {
  if (ukm_service_) {
    ukm_service_->Purge();
  }
}

void ChromeMetricsServiceClient::OnUkmAllowedStateChanged(
    bool total_purge,
    ukm::UkmConsentState previous_consent_state) {
  if (!ukm_service_) {
    return;
  }

  const ukm::UkmConsentState consent_state = GetUkmConsentState();

  // Manages purging of events and sources.
  if (total_purge) {
    ukm_service_->Purge();
    ukm_service_->ResetClientState(ukm::ResetReason::kOnUkmAllowedStateChanged);
  } else {
    // Purge recording if required consent has been revoked.
    if (!consent_state.Has(ukm::MSBB)) {
      ukm_service_->PurgeMsbbData();
    }
    if (!consent_state.Has(ukm::EXTENSIONS)) {
      ukm_service_->PurgeExtensionsData();
    }
    if (!consent_state.Has(ukm::APPS)) {
      ukm_service_->PurgeAppsData();
    }

    // If MSBB or App-sync consent changed from on to off then, the client id,
    // or client state, must be reset. When not ChromeOS Ash, function
    // will be a no-op.
    //
    // On non-ChromeOS platforms, client reset is handled above because
    // |total_purge| will be true. MSBB is used to determine if UKM is enabled
    // or disabled. When the consent is revoked UkmService will be disabled,
    // triggering |total_purge| to be true. At which point the client state will
    // be reset.
    //
    // On ChromeOS, disabling MSBB or App-Sync will not trigger a total purge.
    // Resetting the client state has to be handled specifically for this case.
    ResetClientStateWhenMsbbOrAppConsentIsRevoked(previous_consent_state);
  }

  // Notify the recording service of changed metrics consent.
  ukm_service_->UpdateRecording(consent_state);

  // Broadcast UKM consent state change.
  ukm_service_->OnUkmAllowedStateChanged(consent_state);

  // Signal service manager to enable/disable UKM based on new states.
  UpdateRunningServices();
}

void ChromeMetricsServiceClient::OnRenderProcessHostCreated(
    content::RenderProcessHost* host) {
  if (!scoped_observations_.IsObservingSource(host)) {
    scoped_observations_.AddObservation(host);
  }
}

void ChromeMetricsServiceClient::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  metrics_service_->OnApplicationNotIdle();
}

void ChromeMetricsServiceClient::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  scoped_observations_.RemoveObservation(host);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ChromeMetricsServiceClient::AsyncInitSystemProfileProvider() {
  DCHECK(cros_system_profile_provider_);
  cros_system_profile_provider_->AsyncInit(base::BindOnce([]() {
    // Structured metrics needs to know when the SystemProfile is
    // available since events should have SystemProfile populated.
    // Notify structured metrics recorder that SystemProfile is available to
    // start sending events.
    metrics::structured::Recorder::GetInstance()->OnSystemProfileInitialized();
  }));
}
#endif

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
    if (!registry) {
      continue;
    }
    const extensions::Extension* extension = registry->GetExtensionById(
        std::string(id), extensions::ExtensionRegistry::ENABLED);
    if (!extension) {
      continue;
    }
    if (!extension->from_webstore()) {
      return false;
    }
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

  if (pid == base::GetCurrentProcId()) {
    return metrics::FileMetricsProvider::FILTER_ACTIVE_THIS_PID;
  }

  if (IsProcessRunning(pid)) {
    return metrics::FileMetricsProvider::FILTER_TRY_LATER;
  }

  return metrics::FileMetricsProvider::FILTER_PROCESS_FILE;
}

// static
void ChromeMetricsServiceClient::SetIsProcessRunningForTesting(
    ChromeMetricsServiceClient::IsProcessRunningFunction func) {
  g_is_process_running = func;
}

bool ChromeMetricsServiceClient::IsUkmAllowedForAllProfiles() {
  return UkmConsentStateObserver::IsUkmAllowedForAllProfiles();
}

bool g_observer_registration_failed = false;
void ChromeMetricsServiceClient::SetNotificationListenerSetupFailedForTesting(
    bool simulate_failure) {
  g_observer_registration_failed = simulate_failure;
}

bool ChromeMetricsServiceClient::
    AreNotificationListenersEnabledOnAllProfiles() {
  // For testing
  if (g_observer_registration_failed) {
    return false;
  }
  return observers_active_;
}

std::string ChromeMetricsServiceClient::GetAppPackageNameIfLoggable() {
  return metrics::GetAppPackageName();
}

std::string ChromeMetricsServiceClient::GetUploadSigningKey() {
  std::string decoded_key;
  base::Base64Decode(google_apis::GetMetricsKey(), &decoded_key);
  return decoded_key;
}

bool ChromeMetricsServiceClient::ShouldResetClientIdsOnClonedInstall() {
  return metrics_service_->ShouldResetClientIdsOnClonedInstall();
}

base::CallbackListSubscription
ChromeMetricsServiceClient::AddOnClonedInstallDetectedCallback(
    base::OnceClosure callback) {
  return metrics_state_manager_->AddOnClonedInstallDetectedCallback(
      std::move(callback));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

bool ChromeMetricsServiceClient::ShouldUploadMetricsForUserId(
    const uint64_t user_id) {
  if (base::FeatureList::IsEnabled(ash::features::kPerUserMetrics)) {
    // Metrics logs with user ids should be stored in a user cryptohome so this
    // function should only be called after a user logins.
    // |per_user_state_manager_| is initialized before a user can login.
    DCHECK(per_user_state_manager_);

    // This function should only be called if reporting is enabled.
    DCHECK(ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());

    auto current_user_id = per_user_state_manager_->GetCurrentUserId();

    // Current session is an ephemeral session with metrics reporting enabled.
    // All logs generated during the session will not have a user id associated.
    // Do not upload log with |user_id| during this session.
    if (!current_user_id.has_value()) {
      return false;
    }

    // If |user_id| is different from the currently logged in user, log
    // associated with different |user_id| should not be uploaded. This can
    // happen if a user goes from enable->disable->enable state as user ID is
    // reset going from enable->disable state.
    //
    // The log will be dropped since it may contain data collected during a
    // point in which metrics reporting consent was disabled.
    return user_id == metrics::MetricsLog::Hash(current_user_id.value());
  }

  return true;
}

void ChromeMetricsServiceClient::UpdateCurrentUserMetricsConsent(
    bool user_metrics_consent) {
  if (base::FeatureList::IsEnabled(ash::features::kPerUserMetrics)) {
    DCHECK(per_user_state_manager_);
    per_user_state_manager_->SetCurrentUserMetricsConsent(user_metrics_consent);
  }
}

void ChromeMetricsServiceClient::InitPerUserMetrics() {
  if (base::FeatureList::IsEnabled(ash::features::kPerUserMetrics)) {
    per_user_state_manager_ =
        std::make_unique<metrics::PerUserStateManagerChromeOS>(
            this, g_browser_process->local_state());
    per_user_consent_change_subscription_ =
        per_user_state_manager_->AddObserver(
            base::BindRepeating(&UpdateMetricsServicesForPerUser));
  }
}

absl::optional<bool> ChromeMetricsServiceClient::GetCurrentUserMetricsConsent()
    const {
  if (per_user_state_manager_) {
    DCHECK(base::FeatureList::IsEnabled(ash::features::kPerUserMetrics));
    return per_user_state_manager_
        ->GetCurrentUserReportingConsentIfApplicable();
  }

  return absl::nullopt;
}

absl::optional<std::string> ChromeMetricsServiceClient::GetCurrentUserId()
    const {
  if (per_user_state_manager_) {
    DCHECK(base::FeatureList::IsEnabled(ash::features::kPerUserMetrics));
    return per_user_state_manager_->GetCurrentUserId();
  }

  return absl::nullopt;
}

#endif  //  BUILDFLAG(IS_CHROMEOS_ASH)

void ChromeMetricsServiceClient::ResetClientStateWhenMsbbOrAppConsentIsRevoked(
    ukm::UkmConsentState previous_consent_state) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const auto ukm_consent_state = GetUkmConsentState();

  // True if MSBB consent change from on to off, False otherwise.
  const bool msbb_revoked = previous_consent_state.Has(ukm::MSBB) &&
                            !ukm_consent_state.Has(ukm::MSBB);

  // True if APPS consent change from on to off, False otherwise.
  const bool apps_revoked = previous_consent_state.Has(ukm::APPS) &&
                            !ukm_consent_state.Has(ukm::APPS);

  // If either condition is true, then reset client state.
  if (msbb_revoked || apps_revoked) {
    ukm_service_->ResetClientState(ukm::ResetReason::kOnUkmAllowedStateChanged);
  }
#endif
}
