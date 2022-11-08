// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_main_parts.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_terminator.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_web_ui_controller_factory.h"
#include "android_webview/browser/metrics/aw_metrics_service_accessor.h"
#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/browser/network_service/aw_network_change_notifier_factory.h"
#include "android_webview/browser/tracing/background_tracing_field_trial.h"
#include "android_webview/common/aw_descriptors.h"
#include "android_webview/common/aw_paths.h"
#include "android_webview/common/aw_resource.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/common/crash_reporter/aw_crash_reporter_client.h"
#include "base/android/apk_assets.h"
#include "base/android/build_info.h"
#include "base/android/bundle_utils.h"
#include "base/android/memory_pressure_listener_android.h"
#include "base/base_paths_android.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/task/current_thread.h"
#include "components/crash/content/browser/child_exit_observer_android.h"
#include "components/crash/core/common/crash_key.h"
#include "components/embedder_support/android/metrics/memory_metrics_logger.h"
#include "components/embedder_support/origin_trials/component_updater_utils.h"
#include "components/heap_profiling/multi_process/supervisor.h"
#include "components/metrics/metrics_service.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/synthetic_trials.h"
#include "components/variations/synthetic_trials_active_group_id_provider.h"
#include "components/variations/variations_crash_keys.h"
#include "components/variations/variations_ids_provider.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/gl/gl_surface.h"

namespace android_webview {

AwBrowserMainParts::AwBrowserMainParts(AwContentBrowserClient* browser_client)
    : browser_client_(browser_client) {
}

AwBrowserMainParts::~AwBrowserMainParts() {
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

  browser_process_ = std::make_unique<AwBrowserProcess>(
      browser_client_->aw_feature_list_creator());

  embedder_support::SetupOriginTrialsCommandLine(
      browser_process_->local_state());

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

  RegisterSyntheticTrials();

  return content::RESULT_CODE_NORMAL_EXIT;
}

void AwBrowserMainParts::RegisterSyntheticTrials() {
  metrics::MetricsService* metrics =
      AwMetricsServiceClient::GetInstance()->GetMetricsService();
  metrics->GetSyntheticTrialRegistry()->AddSyntheticTrialObserver(
      variations::VariationsIdsProvider::GetInstance());
  metrics->GetSyntheticTrialRegistry()->AddSyntheticTrialObserver(
      variations::SyntheticTrialsActiveGroupIdProvider::GetInstance());

  static constexpr char kWebViewApkTypeTrial[] = "WebViewApkType";
  ApkType apk_type = AwBrowserProcess::GetApkType();
  std::string apk_type_string;
  switch (apk_type) {
    case ApkType::TRICHROME:
      apk_type_string = "Trichrome";
      break;
    case ApkType::MONOCHROME:
      apk_type_string = "Monochrome";
      break;
    case ApkType::STANDALONE:
      apk_type_string = "Standalone";
      break;
  }
  AwMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      metrics, kWebViewApkTypeTrial, apk_type_string,
      variations::SyntheticTrialAnnotationMode::kNextLog);
}

int AwBrowserMainParts::PreMainMessageLoopRun() {
  TRACE_EVENT0("startup", "AwBrowserMainParts::PreMainMessageLoopRun");
  AwBrowserProcess::GetInstance()->PreMainMessageLoopRun();
  browser_client_->InitBrowserContext();
  content::WebUIControllerFactory::RegisterFactory(
      AwWebUIControllerFactory::GetInstance());
  content::RenderFrameHost::AllowInjectingJavaScript();
  metrics_logger_ = std::make_unique<metrics::MemoryMetricsLogger>();
  return content::RESULT_CODE_NORMAL_EXIT;
}

void AwBrowserMainParts::WillRunMainMessageLoop(
    std::unique_ptr<base::RunLoop>& run_loop) {
  NOTREACHED();
}

namespace {

void LoadOriginTrialsControllerDelegateOnUiThread() {
  // Requesting the |OriginTrialsControllerDelegate| will initialize
  // it if the feature is enabled.
  //
  // This should be done as soon as possible in the start-up process, in order
  // to load the database from disk.
  AwBrowserContext::GetDefault()->GetOriginTrialsControllerDelegate();
}

}  // namespace

void AwBrowserMainParts::PostCreateThreads() {
  heap_profiling::Mode mode = heap_profiling::GetModeForStartup();
  if (mode != heap_profiling::Mode::kNone)
    heap_profiling::Supervisor::GetInstance()->Start(base::NullCallback());

  MaybeSetupSystemTracing();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&LoadOriginTrialsControllerDelegateOnUiThread));
}

}  // namespace android_webview
