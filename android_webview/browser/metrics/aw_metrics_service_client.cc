// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_metrics_service_client.h"

#include <jni.h>

#include <cstdint>
#include <string>
#include <string_view>

#include "android_webview/browser/metrics/android_metrics_log_uploader.h"
#include "android_webview/browser/metrics/android_metrics_provider.h"
#include "android_webview/common/aw_features.h"
#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/barrier_closure.h"
#include "base/base_paths_android.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/i18n/rtl.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/metrics/android_metrics_provider.h"
#include "components/metrics/call_stacks/call_stack_profile_metrics_provider.h"
#include "components/metrics/content/content_stability_metrics_provider.h"
#include "components/metrics/content/extensions_helper.h"
#include "components/metrics/content/gpu_metrics_provider.h"
#include "components/metrics/content/metrics_services_web_contents_observer.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/metrics/cpu_metrics_provider.h"
#include "components/metrics/drive_metrics_provider.h"
#include "components/metrics/entropy_state_provider.h"
#include "components/metrics/file_metrics_provider.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/net/cellular_logic_helper.h"
#include "components/metrics/net/network_metrics_provider.h"
#include "components/metrics/persistent_histograms.h"
#include "components/metrics/persistent_synthetic_trial_observer.h"
#include "components/metrics/sampling_metrics_provider.h"
#include "components/metrics/stability_metrics_helper.h"
#include "components/metrics/ui/form_factor_metrics_provider.h"
#include "components/metrics/ui/screen_info_metrics_provider.h"
#include "components/metrics/version_utils.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/android/channel_getter.h"
#include "content/public/browser/histogram_fetcher.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwMetricsServiceClient_jni.h"

namespace android_webview {

using InstallerPackageType = AwMetricsServiceClient::InstallerPackageType;

namespace {

// This specifies the amount of time to wait for all renderers to send their
// data.
const int kMaxHistogramGatheringWaitDuration = 60000;  // 60 seconds.

const int kMaxHistogramStorageKiB = 100 << 10;  // 100 MiB

// Divides the spectrum of uint32_t values into 1000 ~equal-sized buckets (range
// [0, 999] inclusive), and returns which bucket |value| falls into. Ex. given
// 2^30, this would return 250, because 25% of uint32_t values fall below the
// given value.
int UintToPerMille(uint32_t value) {
  // We need to divide by UINT32_MAX+1 (2^32), otherwise the fraction could
  // evaluate to 1000.
  uint64_t divisor = 1llu << 32;
  uint64_t value_per_mille = static_cast<uint64_t>(value) * 1000llu / divisor;
  DCHECK_GE(value_per_mille, 0llu);
  DCHECK_LE(value_per_mille, 999llu);
  return static_cast<int>(value_per_mille);
}

bool IsProcessRunning(base::ProcessId pid) {
  // Sending a signal value of 0 will cause error checking to be performed
  // with no signal being sent.
  return (kill(pid, 0) == 0 || errno != ESRCH);
}

metrics::FileMetricsProvider::FilterAction FilterBrowserMetricsFiles(
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

bool IsSamplesCounterEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kPersistentHistogramsFeature, "prev_run_metrics_count_only", false);
}

metrics::FileMetricsProvider::Params CreateBrowserMetricsParams(
    base::FilePath metrics_dir) {
  using metrics::FileMetricsProvider;

  FileMetricsProvider::Params browser_metrics_params(
      metrics_dir.AppendASCII(kBrowserMetricsName),
      FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_DIR,
      IsSamplesCounterEnabled()
          ? FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE_SAMPLES_COUNTER
          : FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE,
      kBrowserMetricsName);
  browser_metrics_params.max_dir_kib = kMaxHistogramStorageKiB;
  browser_metrics_params.filter =
      base::BindRepeating(FilterBrowserMetricsFiles);

  return browser_metrics_params;
}

// TODO(crbug.com/40158523): Unify this implementation with the one in
// ChromeMetricsServiceClient.
std::unique_ptr<metrics::FileMetricsProvider> CreateFileMetricsProvider(
    PrefService* pref_service,
    base::FilePath metrics_dir,
    base::FilePath old_metrics_dir,
    bool metrics_reporting_enabled) {
  using metrics::FileMetricsProvider;

  // Create an object to monitor files of metrics and include them in reports.
  // `is_fre` will always be false for Android WV because the concept of First
  // Run Experience is not applicable.
  std::unique_ptr<FileMetricsProvider> file_metrics_provider =
      std::make_unique<FileMetricsProvider>(pref_service,
                                            /*is_fre=*/false);

  file_metrics_provider->RegisterSource(CreateBrowserMetricsParams(metrics_dir),
                                        metrics_reporting_enabled);

  if (!old_metrics_dir.empty()) {
    file_metrics_provider->RegisterSource(
        CreateBrowserMetricsParams(old_metrics_dir), metrics_reporting_enabled);
  }

  // WebView never configured Crashpad to actually create these metrics files,
  // so it's not useful to try to upload them.
  // TODO(crbug.com/440359722): decide if we want these metrics and either
  // configure Crashpad appropriately or clean up this code.

  // Register the Crashpad metrics files:
  // 1. Data from the previous run if crashpad_handler didn't exit cleanly.
  // base::FilePath crashpad_metrics_file =
  //     base::GlobalHistogramAllocator::ConstructFilePath(
  //         metrics_dir, kCrashpadHistogramAllocatorName);
  // file_metrics_provider->RegisterSource(
  //     FileMetricsProvider::Params(
  //         crashpad_metrics_file,
  //         FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
  //         FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE_OR_PREVIOUS_RUN,
  //         kCrashpadHistogramAllocatorName),
  //     metrics_reporting_enabled);

  // 2. Data from the current run. Note: "Active" files don't set "prefs_key"
  // because they update the file itself.
  // base::FilePath crashpad_active_path =
  //     base::GlobalHistogramAllocator::ConstructFilePathForActiveFile(
  //         metrics_dir, kCrashpadHistogramAllocatorName);
  // file_metrics_provider->RegisterSource(
  //     FileMetricsProvider::Params(
  //         crashpad_active_path,
  //         FileMetricsProvider::SOURCE_HISTOGRAMS_ACTIVE_FILE,
  //         FileMetricsProvider::ASSOCIATE_CURRENT_RUN),
  //     metrics_reporting_enabled);
  return file_metrics_provider;
}

base::OnceClosure CreateChainedClosure(base::OnceClosure cb1,
                                       base::OnceClosure cb2) {
  return base::BindOnce(
      [](base::OnceClosure cb1, base::OnceClosure cb2) {
        if (cb1) {
          std::move(cb1).Run();
        }
        if (cb2) {
          std::move(cb2).Run();
        }
      },
      std::move(cb1), std::move(cb2));
}

// IMPORTANT: DO NOT CHANGE sample rates without first ensuring the Chrome
// Metrics team has the appropriate backend bandwidth and storage.

// Sample at 2%, based on storage concerns. We sample at a different rate than
// Chrome because we have more metrics "clients" (each app on the device counts
// as a separate client).
const int kStableUnfilteredSampledInRatePerMille = 20;

// Sample non-stable channels at 99%, to boost volume for pre-stable
// experiments. We choose 99% instead of 100% for consistency with Chrome and to
// exercise the out-of-sample code path.
const int kBetaDevCanaryUnfilteredSampledInRatePerMille = 990;

AwMetricsServiceClient* g_aw_metrics_service_client = nullptr;

}  // namespace

// Needs to be kept in sync with the writer in
// third_party/crashpad/crashpad/handler/handler_main.cc.
const char kCrashpadHistogramAllocatorName[] = "CrashpadMetrics";

AwMetricsServiceClient::Delegate::Delegate() = default;
AwMetricsServiceClient::Delegate::~Delegate() = default;

// static
AwMetricsServiceClient* AwMetricsServiceClient::GetInstance() {
  DCHECK(g_aw_metrics_service_client);
  DCHECK_CALLED_ON_VALID_SEQUENCE(
      g_aw_metrics_service_client->sequence_checker_);
  return g_aw_metrics_service_client;
}

// static
void AwMetricsServiceClient::SetInstance(
    std::unique_ptr<AwMetricsServiceClient> aw_metrics_service_client) {
  DCHECK(!g_aw_metrics_service_client);
  DCHECK(aw_metrics_service_client);
  g_aw_metrics_service_client = aw_metrics_service_client.release();
  DCHECK_CALLED_ON_VALID_SEQUENCE(
      g_aw_metrics_service_client->sequence_checker_);
}

AwMetricsServiceClient::AwMetricsServiceClient(
    std::unique_ptr<Delegate> delegate)
    : time_created_(base::Time::Now()), delegate_(std::move(delegate)) {}

AwMetricsServiceClient::~AwMetricsServiceClient() = default;

void AwMetricsServiceClient::Initialize(PrefService* pref_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!init_finished_);

  pref_service_ = pref_service;

  metrics_state_manager_ = metrics::MetricsStateManager::Create(
      pref_service_, this,
      // Pass an empty file path since the path is for Extended Variations Safe
      // Mode, which is N/A to Android embedders.
      std::wstring(), base::FilePath(), metrics::StartupVisibility::kUnknown,
      {
          // The low entropy provider is used instead of the default provider
          // because the default provider needs to know if UMA is enabled and
          // querying GMS to determine whether UMA is enabled is slow.
          // The low entropy provider has fewer unique experiment combinations,
          // which is better for privacy, but can have crosstalk issues between
          // experiments.
          .default_entropy_provider_type = metrics::EntropyProviderType::kLow,
          .force_benchmarking_mode =
              base::CommandLine::ForCurrentProcess()->HasSwitch(
                  switches::kEnableGpuBenchmarking),
      });

  metrics_state_manager_->InstantiateFieldTrialList();

  init_finished_ = true;

  synthetic_trial_registry_ =
      std::make_unique<variations::SyntheticTrialRegistry>();
  synthetic_trial_observation_.Observe(synthetic_trial_registry_.get());

  // Create the MetricsService immediately so that other code can make use of
  // it. Chrome always creates the MetricsService as well.
  metrics_service_ = std::make_unique<metrics::MetricsService>(
      metrics_state_manager_.get(), this, pref_service_);

  // Registration of providers has to wait until consent is determined. To
  // do otherwise means the providers would always be configured with reporting
  // disabled (because when this is called in production consent hasn't been
  // determined).
  // We also need the metrics directory to have been set up by calling
  // SetUpMetricsDir().
  // If consent has not been determined or the metrics directory not set, this
  // does nothing.
  MaybeStartMetrics();
}

void AwMetricsServiceClient::MaybeStartMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsReadyToStart()) {
    return;
  }

#if DCHECK_IS_ON()
  // This function should be called only once after consent has been determined.
  DCHECK(!did_start_metrics_with_consent_);
  did_start_metrics_with_consent_ = true;
#endif

  // Treat the debugging flag the same as user consent because the user set it,
  // but keep app_consent_ separate so we never persist data from an opted-out
  // app.
  bool user_consent_or_flag = user_consent_ || IsMetricsReportingForceEnabled();
  if (app_consent_ && user_consent_or_flag) {
    did_start_metrics_ = true;
    // Make GetSampleBucketValue() work properly.
    metrics_state_manager_->ForceClientIdCreation();
    is_client_id_forced_ = true;
    RegisterMetricsProvidersAndInitState();
    // Register for notifications so we can detect when the user or app are
    // interacting with the embedder. We use these as signals to wake up the
    // MetricsService.
    delegate_->AddWebViewAppStateObserver(this);

    if (IsReportingEnabled()) {
      // We assume the embedder has no shutdown sequence, so there's no need
      // for a matching Stop() call.
      metrics_service_->Start();
    }
  } else {
    // Even though reporting is not enabled, CreateFileMetricsProvider() is
    // called. This ensures on disk state is removed.
    metrics_service_->RegisterMetricsProvider(
        CreateFileMetricsProvider(pref_service_, metrics_dir_, old_metrics_dir_,
                                  /* metrics_reporting_enabled */ false));
    pref_service_->ClearPref(metrics::prefs::kMetricsClientID);
    pref_service_->ClearPref(metrics::prefs::kMetricsProvisionalClientID);
    pref_service_->ClearPref(metrics::prefs::kMetricsLogRecordId);
  }
}

void AwMetricsServiceClient::RegisterMetricsProvidersAndInitState() {
  CHECK(metrics::SubprocessMetricsProvider::GetInstance());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::NetworkMetricsProvider>(
          content::CreateNetworkConnectionTrackerAsyncGetter()));
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::CPUMetricsProvider>());
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::EntropyStateProvider>(pref_service_));
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::ScreenInfoMetricsProvider>());
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::FormFactorMetricsProvider>());
  metrics_service_->RegisterMetricsProvider(CreateFileMetricsProvider(
      pref_service_, metrics_dir_, old_metrics_dir_,
      metrics_state_manager_->IsMetricsReportingEnabled()));
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::CallStackProfileMetricsProvider>());
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::AndroidMetricsProvider>());
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::DriveMetricsProvider>(
          base::DIR_ANDROID_APP_DATA));
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::GPUMetricsProvider>());
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::SamplingMetricsProvider>(
          GetUnfilteredSampleRatePerMille()));
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::ContentStabilityMetricsProvider>(
          pref_service_, /*extensions_helper=*/nullptr));
  delegate_->RegisterAdditionalMetricsProviders(metrics_service_.get());

  // The file metrics provider performs IO.
  base::ScopedAllowBlocking allow_io;
  metrics_service_->InitializeMetricsRecordingState();
}

void AwMetricsServiceClient::SetHaveMetricsConsent(bool user_consent,
                                                   bool app_consent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  set_consent_finished_ = true;
  user_consent_ = user_consent;
  app_consent_ = app_consent;
  MaybeStartMetrics();
}

void AwMetricsServiceClient::SetFastStartupForTesting(
    bool fast_startup_for_testing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fast_startup_for_testing_ = fast_startup_for_testing;
}

void AwMetricsServiceClient::SetUploadIntervalForTesting(
    const base::TimeDelta& upload_interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  overridden_upload_interval_ = upload_interval;
}

bool AwMetricsServiceClient::IsReadyToStart() const {
  return init_finished_ && set_consent_finished_ && !metrics_dir_.empty();
}

bool AwMetricsServiceClient::IsConsentGiven() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return user_consent_ && app_consent_;
}

bool AwMetricsServiceClient::IsReportingEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!app_consent_) {
    return false;
  }
  return IsMetricsReportingForceEnabled() ||
         EnabledStateProvider::IsReportingEnabled();
}

metrics::MetricsService* AwMetricsServiceClient::GetMetricsServiceIfStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return did_start_metrics_ ? metrics_service_.get() : nullptr;
}

variations::SyntheticTrialRegistry*
AwMetricsServiceClient::GetSyntheticTrialRegistry() {
  return synthetic_trial_registry_.get();
}

metrics::MetricsService* AwMetricsServiceClient::GetMetricsService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This will be null if initialization hasn't finished.
  return metrics_service_.get();
}

// In Chrome, UMA and Crashpad are enabled/disabled together by the same
// checkbox and they share the same client ID (a.k.a. GUID). SetMetricsClientId
// is intended to provide the ID to Breakpad. In AwMetricsServiceClients
// UMA and Crashpad are independent, so this is a no-op.
void AwMetricsServiceClient::SetMetricsClientId(const std::string& client_id) {}

int32_t AwMetricsServiceClient::GetProduct() {
  return metrics::ChromeUserMetricsExtension::ANDROID_WEBVIEW;
}

std::string AwMetricsServiceClient::GetApplicationLocale() {
  return base::i18n::GetConfiguredLocale();
}

const network_time::NetworkTimeTracker*
AwMetricsServiceClient::GetNetworkTimeTracker() {
  return nullptr;
}

bool AwMetricsServiceClient::GetBrand(std::string* brand_code) {
  // AwMetricsServiceClients don't use brand codes.
  return false;
}

metrics::SystemProfileProto::Channel AwMetricsServiceClient::GetChannel() {
  return metrics::AsProtobufChannel(version_info::android::GetChannel());
}

bool AwMetricsServiceClient::IsExtendedStableChannel() {
  return false;  // Not supported on AwMetricsServiceClients.
}

std::string AwMetricsServiceClient::GetVersionString() {
  return metrics::GetVersionString();
}

void AwMetricsServiceClient::MergeSubprocessHistograms() {
  // TODO(crbug.com/40213327): Move this to a shared place to not have to
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

void AwMetricsServiceClient::CollectFinalMetricsForLog(
    base::OnceClosure done_callback) {
  auto barrier_closure =
      base::BarrierClosure(/*num_closures=*/2, std::move(done_callback));

  // Merge histograms from metrics providers into StatisticsRecorder.
  base::StatisticsRecorder::ImportProvidedHistograms(
      /*async=*/true, /*done_callback=*/barrier_closure);

  base::TimeDelta timeout =
      base::Milliseconds(kMaxHistogramGatheringWaitDuration);

  // Set up the callback task to call after we receive histograms from all
  // child processes. |timeout| specifies how long to wait before absolutely
  // calling us back on the task.
  content::FetchHistogramsAsynchronously(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      CreateChainedClosure(barrier_closure,
                           on_final_metrics_collected_listener_),
      timeout);

  if (collect_final_metrics_for_log_closure_) {
    std::move(collect_final_metrics_for_log_closure_).Run();
  }
}

std::unique_ptr<metrics::MetricsLogUploader>
AwMetricsServiceClient::CreateUploader(
    const GURL& server_url,
    const GURL& insecure_server_url,
    std::string_view mime_type,
    metrics::MetricsLogUploader::MetricServiceType service_type,
    const metrics::MetricsLogUploader::UploadCallback& on_upload_complete) {
  CHECK_EQ(service_type, metrics::MetricsLogUploader::UMA);
  // |server_url|, |insecure_server_url|, and |mime_type| are unused because
  // AwMetricsServiceClients send metrics to the platform logging mechanism
  // rather than to Chrome's metrics server.
  return std::make_unique<metrics::AndroidMetricsLogUploader>(
      on_upload_complete);
}

base::TimeDelta AwMetricsServiceClient::GetStandardUploadInterval() {
  // In AwMetricsServiceClients, metrics collection (when we batch up all
  // logged histograms into a ChromeUserMetricsExtension proto) and metrics
  // uploading (when the proto goes to the server) happen separately.
  //
  // This interval controls the metrics collection rate, so we choose the
  // standard upload interval to make sure we're collecting metrics consistently
  // with Chrome for Android. The metrics uploading rate for
  // AwMetricsServiceClients is controlled by the platform logging
  // mechanism. Since this mechanism has its own logic for rate-limiting on
  // cellular connections, we disable the component-layer logic.
  if (!overridden_upload_interval_.is_zero()) {
    return overridden_upload_interval_;
  }
  return metrics::GetUploadInterval(false /* use_cellular_upload_interval */);
}

bool AwMetricsServiceClient::ShouldStartUpFast() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return fast_startup_for_testing_;
}

void AwMetricsServiceClient::OnRenderProcessHostCreated(
    content::RenderProcessHost* host) {
  if (!host_observation_.IsObservingSource(host)) {
    host_observation_.AddObservation(host);
  }
}

void AwMetricsServiceClient::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  host_observation_.RemoveObservation(host);

  if (did_start_metrics_) {
    metrics_service_->OnApplicationNotIdle();
  }
}

void AwMetricsServiceClient::OnWebContentsCreated(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  metrics::MetricsServicesWebContentsObserver::CreateForWebContents(
      web_contents,
      /*OnDidStartLoadingCb=*/
      base::BindRepeating(&AwMetricsServiceClient::OnDidStartLoading,
                          weak_ptr_factory_.GetWeakPtr()),
      /*OnDidStopLoadingCb=*/
      base::BindRepeating(&AwMetricsServiceClient::OnApplicationNotIdle,
                          weak_ptr_factory_.GetWeakPtr()),
      /*OnRendererUnresponsiveCb=*/
      base::BindRepeating(&AwMetricsServiceClient::OnApplicationNotIdle,
                          weak_ptr_factory_.GetWeakPtr()));
}

void AwMetricsServiceClient::SetCollectFinalMetricsForLogClosureForTesting(
    base::OnceClosure closure) {
  collect_final_metrics_for_log_closure_ = std::move(closure);
}

void AwMetricsServiceClient::SetOnFinalMetricsCollectedListenerForTesting(
    base::RepeatingClosure listener) {
  on_final_metrics_collected_listener_ = std::move(listener);
}

int AwMetricsServiceClient::GetSampleBucketValue() const {
  DCHECK(is_client_id_forced_);
  return UintToPerMille(base::PersistentHash(metrics_service_->GetClientId()));
}

InstallerPackageType AwMetricsServiceClient::GetInstallerPackageType() {
  // Check with Java side, to see if it's OK to log the package name for this
  // type of app (see Java side for the specific requirements).
  JNIEnv* env = base::android::AttachCurrentThread();
  int type = Java_AwMetricsServiceClient_getInstallerPackageType(env);
  return static_cast<InstallerPackageType>(type);
}

bool AwMetricsServiceClient::CanRecordPackageNameForAppType() {
  InstallerPackageType installer_type = GetInstallerPackageType();
  // Allow recording the app package name of system apps and apps
  // from the play store.
  return (installer_type == InstallerPackageType::SYSTEM_APP ||
          installer_type == InstallerPackageType::GOOGLE_PLAY_STORE);
}

std::string AwMetricsServiceClient::GetAppPackageNameIfLoggable() {
  if (CanRecordPackageNameForAppType()) {
    return GetAppPackageName();
  }
  return std::string();
}

std::string AwMetricsServiceClient::GetAppPackageName() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_app_name =
      Java_AwMetricsServiceClient_getAppPackageName(env);
  if (j_app_name) {
    return base::android::ConvertJavaStringToUTF8(env, j_app_name);
  }
  return std::string();
}

void AwMetricsServiceClient::SetUpMetricsDir() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // In the past, WebView used the normal data directory to store metrics.
  base::FilePath data_dir;
  if (!base::PathService::Get(base::DIR_ANDROID_APP_DATA, &data_dir)) {
    NOTREACHED();
  }

  base::FilePath no_backup_files_dir = GetNoBackupFilesDir();
  if (no_backup_files_dir.empty()) {
    // This will be empty if the app has configured a specific absolute path as
    // the data directory. The API doesn't have a way to configure the no backup
    // files dir, so for this case we don't migrate at all and just keep using
    // the data directory as before.
    metrics_dir_ = std::move(data_dir);
    MaybeStartMetrics();
    return;
  }

  // To minimize metrics loss if we roll back this experiment, the migration is
  // bidirectional - the feature flag is used to determine the direction, not
  // whether to do it.
  bool use_no_backup_files_dir = base::FeatureList::IsEnabled(
      android_webview::features::kWebViewPersistentMetricsInNoBackupDir);
  base::FilePath old_dir;
  if (use_no_backup_files_dir) {
    // Only try to create the no backup files directory if the flag is enabled,
    // so that we don't unnecessarily create it when not doing the migration.
    base::CreateDirectory(no_backup_files_dir);
    metrics_dir_ = std::move(no_backup_files_dir);
    old_dir = std::move(data_dir);
  } else {
    metrics_dir_ = std::move(data_dir);
    old_dir = std::move(no_backup_files_dir);
  }

  // On Android we can only persist metrics for the current session if a spare
  // file was pre-created by a previous session. If we don't have a spare file
  // in the "current" directory, try to move one from the "old" directory now,
  // before we initialize persistent metrics.
  base::FilePath cur_spare_file =
      GetPersistentHistogramsSpareFilePath(metrics_dir_);
  base::FilePath old_spare_file = GetPersistentHistogramsSpareFilePath(old_dir);
  if (!base::PathExists(cur_spare_file)) {
    // No-op if old file doesn't exist.
    base::Move(old_spare_file, cur_spare_file);
  } else {
    // Already have a new file; if the old one exists, delete it.
    base::DeleteFile(old_spare_file);
  }

  // Try to delete the old subdirectory for metrics awaiting upload. This will
  // only succeed if it's empty as we aren't deleting recursively.
  if (base::DeleteFile(old_dir.AppendASCII(kBrowserMetricsName))) {
    // We either deleted it successfully because it was empty, or it didn't
    // exist in the first place. Nothing to do.
  } else {
    // The directory exists and is non-empty. Rather than trying to move the
    // files and have to deal with failures/collisions/etc, we'll just store the
    // old path as well. Later, we'll configure the FileMetricsProvider to watch
    // both paths; it will handle all the files the same way and eventually
    // delete them, enabling us to remove the subdir on some future startup.
    old_metrics_dir_ = std::move(old_dir);
  }

  MaybeStartMetrics();
}

base::FilePath AwMetricsServiceClient::GetMetricsDir() {
  return metrics_dir_;
}

base::FilePath AwMetricsServiceClient::GetOldMetricsDirForTesting() {
  return old_metrics_dir_;
}

void AwMetricsServiceClient::OnApplicationNotIdle() {
  auto* metrics_service = GetMetricsServiceIfStarted();
  if (!metrics_service) {
    return;
  }

  metrics_service->OnApplicationNotIdle();
}

void AwMetricsServiceClient::OnDidStartLoading() {
  OnApplicationNotIdle();

  auto* metrics_service = GetMetricsService();
  if (!metrics_service) {
    return;
  }

  metrics_service->OnPageLoadStarted();
}

int AwMetricsServiceClient::GetUnfilteredSampleRatePerMille() const {
  // Down-sample unknown channel as a precaution in case it ends up being
  // shipped to Stable users.
  version_info::Channel channel = version_info::android::GetChannel();
  if (channel == version_info::Channel::STABLE ||
      channel == version_info::Channel::UNKNOWN) {
    return kStableUnfilteredSampledInRatePerMille;
  }
  return kBetaDevCanaryUnfilteredSampledInRatePerMille;
}

bool AwMetricsServiceClient::ShouldApplyMetricsFiltering() const {
  bool in_unfiltered_sample =
      GetSampleBucketValue() < GetUnfilteredSampleRatePerMille();
  bool force_enabled = IsMetricsReportingForceEnabled();
  return !(in_unfiltered_sample || force_enabled);
}

void AwMetricsServiceClient::OnAppStateChanged(
    WebViewAppStateObserver::State state) {
  // To match MetricsService's expectation,
  // - does nothing if no WebView has ever been created.
  // - starts notifying MetricsService once a WebView is created and the app
  //   is foreground.
  // - consolidates the other states other than kForeground into background.
  // - avoids the duplicated notification.
  if (state == WebViewAppStateObserver::State::kDestroyed &&
      !delegate_->HasAwContentsEverCreated()) {
    return;
  }

  bool foreground = state == WebViewAppStateObserver::State::kForeground;

  if (foreground == app_in_foreground_)
    return;

  app_in_foreground_ = foreground;
  if (app_in_foreground_) {
    GetMetricsService()->OnAppEnterForeground();
  } else {
    // TODO(crbug.com/40118864): Turn on the background recording.
    // Not recording in background, this matches Chrome's behavior.
    GetMetricsService()->OnAppEnterBackground(
        /* keep_recording_in_background = false */);
  }
}

// static
void AwMetricsServiceClient::RegisterMetricsPrefs(
    PrefRegistrySimple* registry) {
  metrics::MetricsService::RegisterPrefs(registry);
  metrics::FileMetricsProvider::RegisterSourcePrefs(registry,
                                                    kBrowserMetricsName);
  metrics::FileMetricsProvider::RegisterSourcePrefs(
      registry, kCrashpadHistogramAllocatorName);
  metrics::FileMetricsProvider::RegisterPrefs(registry);
  metrics::StabilityMetricsHelper::RegisterPrefs(registry);
  AndroidMetricsProvider::RegisterPrefs(registry);
}

// static
base::FilePath AwMetricsServiceClient::GetNoBackupFilesDir() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::FilePath(
      Java_AwMetricsServiceClient_getNoBackupFilesDirForMetrics(env));
}

// static
static void JNI_AwMetricsServiceClient_SetHaveMetricsConsent(
    JNIEnv* env,
    jboolean user_consent,
    jboolean app_consent) {
  AwMetricsServiceClient::GetInstance()->SetHaveMetricsConsent(user_consent,
                                                               app_consent);
}

// static
static void JNI_AwMetricsServiceClient_SetFastStartupForTesting(
    JNIEnv* env,
    jboolean fast_startup_for_testing) {
  AwMetricsServiceClient::GetInstance()->SetFastStartupForTesting(
      fast_startup_for_testing);
}

// static
static void JNI_AwMetricsServiceClient_SetUploadIntervalForTesting(
    JNIEnv* env,
    jlong upload_interval_ms) {
  AwMetricsServiceClient::GetInstance()->SetUploadIntervalForTesting(
      base::Milliseconds(upload_interval_ms));
}

// static
static void
JNI_AwMetricsServiceClient_SetOnFinalMetricsCollectedListenerForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& listener) {
  AwMetricsServiceClient::GetInstance()
      ->SetOnFinalMetricsCollectedListenerForTesting(base::BindRepeating(
          base::android::RunRunnableAndroid,
          base::android::ScopedJavaGlobalRef<jobject>(listener)));
}

}  // namespace android_webview

DEFINE_JNI(AwMetricsServiceClient)
