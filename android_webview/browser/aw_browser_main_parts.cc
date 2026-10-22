// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_main_parts.h"

#include <memory>
#include <string>
#include <utility>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_terminator.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_web_ui_controller_factory.h"
#include "android_webview/browser/metrics/aw_metrics_service_accessor.h"
#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/browser/metrics/memory_metrics_logger.h"
#include "android_webview/browser/network_service/aw_network_change_notifier_factory.h"
#include "android_webview/common/aw_cached_flags.h"
#include "android_webview/common/aw_descriptors.h"
#include "android_webview/common/aw_features.h"
#include "android_webview/common/aw_paths.h"
#include "android_webview/common/aw_resource.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/common/crash_reporter/aw_crash_reporter_client.h"
#include "base/android/apk_assets.h"
#include "base/android/bundle_utils.h"
#include "base/android/memory_pressure_listener_android.h"
#include "base/android/path_utils.h"
#include "base/base_paths_android.h"
#include "base/byte_size.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/task/current_thread.h"
#include "base/task/thread_pool.h"
#include "components/crash/content/browser/child_exit_observer_android.h"
#include "components/crash/core/common/crash_key.h"
#include "components/embedder_support/origin_trials/component_updater_utils.h"
#include "components/embedder_support/origin_trials/origin_trials_settings_storage.h"
#include "components/heap_profiling/in_process/browser_process_snapshot_controller.h"
#include "components/heap_profiling/in_process/heap_profiler_controller.h"
#include "components/heap_profiling/in_process/mojom/snapshot_controller.mojom.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/performance_manager/embedder/graph_features.h"
#include "components/performance_manager/embedder/performance_manager_lifetime.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/synthetic_trials.h"
#include "components/variations/synthetic_trials_active_group_id_provider.h"
#include "components/variations/variations_crash_keys.h"
#include "components/variations/variations_ids_provider.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/synthetic_trial_syncer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#include "third_party/blink/public/common/origin_trials/origin_trials_settings_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gl/gl_surface.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwInterfaceRegistrar_jni.h"

namespace {

void BindHeapSnapshotControllerToProcessHost(
    int child_process_id,
    mojo::PendingReceiver<heap_profiling::mojom::SnapshotController> receiver) {
  if (auto* bcph = content::BrowserChildProcessHost::FromID(child_process_id)) {
    bcph->GetHost()->BindReceiver(std::move(receiver));
  } else if (auto* rph = content::RenderProcessHost::FromID(child_process_id)) {
    if (!rph->GetBrowserContext()->IsOffTheRecord()) {
      rph->BindReceiver(std::move(receiver));
    }
  }
}

}  // namespace

namespace android_webview {

AwBrowserMainParts::AwBrowserMainParts(AwContentBrowserClient* browser_client)
    : browser_client_(browser_client) {
}

AwBrowserMainParts::~AwBrowserMainParts() {
}

void AwBrowserMainParts::PreCreateMainMessageLoop() {
  // WebView should not override the main thread name.
  // This switch is set here instead of
  // `android_webview/lib/aw_main_delegate.cc` because it must be available in
  // `BrowserMainLoop::CreateMainMessageLoop`.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableMainThreadNameOverride);
}

int AwBrowserMainParts::PreEarlyInitialization() {
  // Network change notifier factory must be singleton, only set factory
  // instance while it is not been created.
  // In most cases, this check is not necessary because SetFactory should be
  // called only once, but both webview and native cronet calls this function,
  // in case of building both webview and cronet to one app, it is required to
  // avoid crashing the app.
  if (!net::NetworkChangeNotifier::GetFactory()) {
    net::NetworkChangeNotifier::SetFactory(
        new AwNetworkChangeNotifierFactory());
  }

  // Creates a SingleThreadTaskExecutor for Android WebView if doesn't exist.
  DCHECK(!main_task_executor_.get());
  if (!base::CurrentThread::IsSet()) {
    main_task_executor_ = std::make_unique<base::SingleThreadTaskExecutor>(
        base::MessagePumpType::UI);
  }

  browser_process_ = std::make_unique<AwBrowserProcess>(browser_client_);

  auto* origin_trials_settings_storage =
      browser_process_->GetOriginTrialsSettingsStorage();
  embedder_support::SetupOriginTrialsCommandLineAndSettings(
      browser_process_->local_state(), origin_trials_settings_storage);
  blink::OriginTrialsSettingsProvider::Get()->SetSettings(
      origin_trials_settings_storage->GetSettings());

  if (base::FeatureList::IsEnabled(
          features::kWebViewCacheSizeLimitDerivedFromAppCacheQuota)) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
        base::BindOnce(&AwBrowserProcess::FetchHostAppCacheQuota,
                       base::Unretained(browser_process_.get())));
  }

  return content::RESULT_CODE_NORMAL_EXIT;
}

int AwBrowserMainParts::PreCreateThreads() {
  base::android::MemoryPressureListenerAndroid::Initialize(
      base::android::AttachCurrentThread());
  child_exit_observer_ =
      std::make_unique<::crash_reporter::ChildExitObserver>();

  // We need to create the safe browsing specific directory even if the
  // AwSafeBrowsingConfigHelper::GetSafeBrowsingEnabled() is false
  // initially, because safe browsing can be enabled later at runtime
  // on a per-webview basis.
  base::FilePath safe_browsing_dir;
  if (base::PathService::Get(android_webview::DIR_SAFE_BROWSING,
                             &safe_browsing_dir)) {
    if (!base::PathExists(safe_browsing_dir))
      base::CreateDirectory(safe_browsing_dir);
  }

  base::FilePath crash_dir;
  if (base::PathService::Get(android_webview::DIR_CRASH_DUMPS, &crash_dir)) {
    if (!base::PathExists(crash_dir)) {
      base::CreateDirectory(crash_dir);
    }
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebViewSandboxedRenderer)) {
    // Create the renderers crash manager on the UI thread.
    child_exit_observer_->RegisterClient(
        std::make_unique<AwBrowserTerminator>());
  }

  crash_reporter::InitializeCrashKeys();
  variations::InitCrashKeys();
  CHECK(metrics::SubprocessMetricsProvider::CreateInstance());

  RegisterSyntheticTrials();

  return content::RESULT_CODE_NORMAL_EXIT;
}

void AwBrowserMainParts::RegisterSyntheticTrials() {
  metrics::MetricsService* metrics =
      AwMetricsServiceClient::GetInstance()->GetMetricsService();
  metrics->GetSyntheticTrialRegistry()->AddObserver(
      variations::VariationsIdsProvider::GetInstance());
  metrics->GetSyntheticTrialRegistry()->AddObserver(
      variations::SyntheticTrialsActiveGroupIdProvider::GetInstance());

  synthetic_trial_syncer_ = content::SyntheticTrialSyncer::Create(
      metrics->GetSyntheticTrialRegistry());

  // We use 3393823 as an id reported for all WebView traffic to help analyse data on the
  // server-side for WebView embedders.
  std::vector<std::string> forced_variation_ids = {"3393823"};

  // Configure experiment to measure impact of using a native renderer zygote.
  std::string native_zygote_group;
  if (AwBrowserProcess::IsNativeWebViewZygoteEnabled()) {
    native_zygote_group = "Enabled";
    forced_variation_ids.emplace_back("101000092");
  } else {
    native_zygote_group = "Disabled";
    forced_variation_ids.emplace_back("101000091");
  }
  AwMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      metrics, "WebViewNativeZygote", native_zygote_group,
      variations::SyntheticTrialAnnotationMode::kCurrentLog);

  variations::VariationsIdsProvider::GetInstance()->ForceVariationIds(
      base::PassKey<AwBrowserMainParts>(), forced_variation_ids,
      /*command_line_variation_ids=*/"");

  AwMetricsServiceClient::GetInstance()->FlushPendingSyntheticTrialsFromJava();

  bool in_seed_experiment =
      android_webview::CachedFlags::IsCachedFeatureOverridden(
          features::kWebViewReducedSeedExpiration) ||
      android_webview::CachedFlags::IsCachedFeatureOverridden(
          features::kWebViewReducedSeedRequestPeriod);
  bool reduced_seed_expiration = android_webview::CachedFlags::IsEnabled(
      features::kWebViewReducedSeedExpiration);
  bool reduced_seed_request_period = android_webview::CachedFlags::IsEnabled(
      features::kWebViewReducedSeedRequestPeriod);

  std::string group = "Default";
  if (in_seed_experiment) {
    if (reduced_seed_expiration && reduced_seed_request_period) {
      group = "BothEnabled";
    } else if (reduced_seed_expiration) {
      group = "ReducedSeedExpiration";
    } else if (reduced_seed_request_period) {
      group = "ReducedSeedRequestPeriod";
    } else {
      group = "Control";
    }
  }
  AwMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      metrics, "WebViewFasterFinchSeed", group,
      variations::SyntheticTrialAnnotationMode::kCurrentLog);

  std::string trial_name, group_name;
  if (const auto* heap_profiler_controller =
          heap_profiling::HeapProfilerController::GetInstance();
      heap_profiler_controller &&
      heap_profiler_controller->GetSyntheticFieldTrial(trial_name,
                                                       group_name)) {
    AwMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        metrics, trial_name, group_name,
        variations::SyntheticTrialAnnotationMode::kCurrentLog);
  }
}

int AwBrowserMainParts::PreMainMessageLoopRun() {
  TRACE_EVENT0("startup", "AwBrowserMainParts::PreMainMessageLoopRun");
  AwBrowserProcess::GetInstance()->PreMainMessageLoopRun();
  browser_client_->InitBrowserContextStore();
  content::WebUIControllerFactory::RegisterFactory(
      AwWebUIControllerFactory::GetInstance());
  content::RenderFrameHost::AllowInjectingJavaScript();
  metrics_logger_ = std::make_unique<metrics::MemoryMetricsLogger>();

  Java_AwInterfaceRegistrar_registerMojoInterfaces(
      base::android::AttachCurrentThread());

  // The global NetworkService instance (network::NetworkService) is profile
  // agnostic and operates asynchronously on a dedicated IO/Network thread.
  // Unlike initializing a full StoragePartition or NetworkContext, invoking
  // content::GetNetworkService() does not touch profile storage paths, SQLite
  // cookie databases, or app quota limits. Eagerly warming up the service here
  // during native browser process startup spins up the background thread and
  // IPC pipelines ahead of time, eliminating thread spin-up latency on the
  // critical path of the first navigation or socket preconnect API call.
  if (base::FeatureList::IsEnabled(features::kWebViewWarmupNetworkService)) {
    TRACE_EVENT0("startup", "AwBrowserMainParts::WarmupNetworkService");
    content::GetNetworkService();
  }

  return content::RESULT_CODE_NORMAL_EXIT;
}

void AwBrowserMainParts::WillRunMainMessageLoop(
    std::unique_ptr<base::RunLoop>& run_loop) {
  NOTREACHED();
}

int AwBrowserMainParts::PostCreateThreads() {
  if (base::FeatureList::IsEnabled(features::kWebViewMemoryProfilingClient)) {
    if (auto* snapshot_controller =
            heap_profiling::BrowserProcessSnapshotController::GetInstance()) {
      snapshot_controller->SetBindRemoteForChildProcessCallback(
          base::BindRepeating(&BindHeapSnapshotControllerToProcessHost));
    }
  }

  // TODO(crbug.com/524981399): Enable standard graph features.
  performance_manager_lifetime_ =
      std::make_unique<performance_manager::PerformanceManagerLifetime>(
          performance_manager::GraphFeatures::WithNone(), base::DoNothing());
  return content::RESULT_CODE_NORMAL_EXIT;
}

}  // namespace android_webview

DEFINE_JNI(AwInterfaceRegistrar)
