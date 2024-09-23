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
#include "android_webview/browser/metrics/system_state_util.h"
#include "android_webview/browser/network_service/aw_network_change_notifier_factory.h"
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
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/task/current_thread.h"
#include "base/trace_event/named_trigger.h"
#include "components/crash/content/browser/child_exit_observer_android.h"
#include "components/crash/core/common/crash_key.h"
#include "components/embedder_support/android/metrics/memory_metrics_logger.h"
#include "components/embedder_support/origin_trials/component_updater_utils.h"
#include "components/embedder_support/origin_trials/origin_trials_settings_storage.h"
#include "components/heap_profiling/multi_process/supervisor.h"
#include "components/metrics/android_metrics_helper.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "components/tracing/common/background_tracing_utils.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/synthetic_trials.h"
#include "components/variations/synthetic_trials_active_group_id_provider.h"
#include "components/variations/variations_crash_keys.h"
#include "components/variations/variations_ids_provider.h"
#include "components/version_info/version_info_values.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/synthetic_trial_syncer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#include "third_party/blink/public/common/origin_trials/origin_trials_settings_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gl/gl_surface.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwBrowserMainParts_jni.h"
#include "android_webview/browser_jni_headers/AwInterfaceRegistrar_jni.h"

namespace android_webview {

namespace {

// Return true if the version code indicates the bundle is primarily 64-bit
// (even if it may have 32-bit bits).
bool Is64bitAccordingToVersionCode(const std::string& version_code) {
  // Primary bitness of the bundle is encoded in the last digit of the version
  // code.
  //
  // From build/util/android_chrome_version.py:
  //       'arm': {
  //          '32': 0,
  //          '32_64': 1,
  //          '64_32': 2,
  //          '64_32_high': 3,
  //          '64': 4,
  //      },
  //      'intel': {
  //          '32': 6,
  //          '32_64': 7,
  //          '64_32': 8,
  //          '64': 9,
  //      },
  std::set<char> arch_codes_64bit = {'2', '3', '4', '8', '9'};
  char arch_code = version_code.back();
  return arch_codes_64bit.count(arch_code) > 0;
}

bool IsBundleInterestingAccordingToVersionCode(
    const std::string& version_code) {
  // Primary bitness of the bundle is encoded in the last digit of the version
  // code. And the variant (package name) is encoded in the second to last.
  //
  // From build/util/android_chrome_version.py:
  //       'arm': {
  //          '32': 0,
  //          '32_64': 1,
  //          '64_32': 2,
  //          '64_32_high': 3,
  //          '64': 4,
  //      },
  //      'intel': {
  //          '32': 6,
  //          '32_64': 7,
  //          '64_32': 8,
  //          '64': 9,
  //      },
  //
  //      _PACKAGE_NAMES = {
  //          'CHROME': 0,
  //          'CHROME_MODERN': 10,
  //          'MONOCHROME': 20,
  //          'TRICHROME': 30,
  //          [...]

  if (version_code.length() != 9) {  // Our scheme has exactly 9 digits.
    return false;
  }

  // '32' and '64' bundles go on 32bit-only and 64bit-only devices, so exclude
  // them.
  std::set<char> arch_codes_mixed = {'1', '2', '3', '7', '8'};
  char arch_code = version_code.back();

  // Only 'TRICHROME' supports 64-bit.
  constexpr char kTriChromeVariant = '3';
  char variant = version_code[version_code.length() - 2];

  return arch_codes_mixed.count(arch_code) > 0 && variant == kTriChromeVariant;
}

}  // namespace

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

  auto* origin_trials_settings_storage =
      browser_process_->GetOriginTrialsSettingsStorage();
  embedder_support::SetupOriginTrialsCommandLineAndSettings(
      browser_process_->local_state(), origin_trials_settings_storage);
  blink::OriginTrialsSettingsProvider::Get()->SetSettings(
      origin_trials_settings_storage->GetSettings());

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

  // Set up experiment for 64-bit WebView.
  //
  // We are specifically interested in devices that meet all of these criteria:
  // 1) Devices with 4&6GB RAM, as we're launching the feature only for those
  //    (using (3.2;6.5) range to match RAM targeting in Play).
  // 2) Devices with only one Android profile (work versus personal), as having
  //    multiple profiles is a source of a population bias (so is having
  //    multiple users, but that bias is known to be small, and they're hard to
  //    filter out).
  // 3) Mixed 32-/64-bit devices, as non-mixed devices are forced to use
  //    a particular bitness, thus don't participate in the experiment.
  // 4) Version code ends with 31/32/33/37/38. 3x represents Trichrome and the
  //    last digit represents mixed device (which streghtens the filter #3). In
  //    reality Stable is mostly represented by 31 and 33.
  //    (TMI, but Beta is a tad more complicated. What UMA sees as Beta
  //    includes, as expected, Chrome Beta channel and, less expected, Chrome
  //    Stable channel distributed via the Play Beta track. The latter is
  //    represented mainly by version codes ending with 41 and 42, which
  //    dominate, but we want to filter them out nonetheless because it's harder
  //    to set up experiment for them.)
  std::string version_code =
      base::android::BuildInfo::GetInstance()->package_version_code();
  size_t ram_mb = base::SysInfo::AmountOfPhysicalMemoryMB();
  auto cpu_abi_bitness_support =
      metrics::AndroidMetricsHelper::GetInstance()->cpu_abi_bitness_support();
  bool is_device_of_interest =
      (3.2 * 1024 < ram_mb && ram_mb < 6.5 * 1024) &&
      (GetMultipleUserProfilesState() ==
       MultipleUserProfilesState::kSingleProfile) &&
      (cpu_abi_bitness_support == metrics::CpuAbiBitnessSupport::k32And64bit) &&
      IsBundleInterestingAccordingToVersionCode(version_code);
  if (is_device_of_interest) {
    std::string trial_group;
    // We can't use defined(ARCH_CPU_64_BITS) on WebView, because bitness of
    // Browser doesn't have to match the bitness of the bundle. Browser always
    // follows bitness of the app, whereas Renderer follows bitness of the
    // bundle.
    if (Is64bitAccordingToVersionCode(version_code)) {
      trial_group = "64bit";
    } else {
      trial_group = "32bit";
    }
    AwMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        metrics, "BitnessForMidRangeRAM", trial_group,
        variations::SyntheticTrialAnnotationMode::kCurrentLog);
    AwMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        metrics, "BitnessForMidRangeRAM_wVersion",
        std::string(PRODUCT_VERSION) + "_" + trial_group,
        variations::SyntheticTrialAnnotationMode::kCurrentLog);
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  bool use_webview_context = Java_AwBrowserMainParts_getUseWebViewContext(env);
  AwMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      metrics, "WebViewSeparateResourceContextMetrics",
      use_webview_context ? "Enabled" : "Control",
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}

int AwBrowserMainParts::PreMainMessageLoopRun() {
  TRACE_EVENT0("startup", "AwBrowserMainParts::PreMainMessageLoopRun");
  AwBrowserProcess::GetInstance()->PreMainMessageLoopRun();
  browser_client_->InitBrowserContext();
  content::WebUIControllerFactory::RegisterFactory(
      AwWebUIControllerFactory::GetInstance());
  content::RenderFrameHost::AllowInjectingJavaScript();
  metrics_logger_ = std::make_unique<metrics::MemoryMetricsLogger>();

  // Requesting the |OriginTrialsControllerDelegate| will initialize
  // it if the feature is enabled.
  //
  // This should be done as soon as possible in the start-up process, in order
  // to load the database from disk.
  AwBrowserContext::GetDefault()->GetOriginTrialsControllerDelegate();

  Java_AwInterfaceRegistrar_registerMojoInterfaces(
      base::android::AttachCurrentThread());

  return content::RESULT_CODE_NORMAL_EXIT;
}

void AwBrowserMainParts::WillRunMainMessageLoop(
    std::unique_ptr<base::RunLoop>& run_loop) {
  NOTREACHED();
}

void AwBrowserMainParts::PostCreateThreads() {
  heap_profiling::Mode mode = heap_profiling::GetModeForStartup();
  if (mode != heap_profiling::Mode::kNone)
    heap_profiling::Supervisor::GetInstance()->Start(base::NullCallback());

  tracing::SetupSystemTracingFromFieldTrial();
  tracing::SetupBackgroundTracingFromCommandLine();
  tracing::SetupPresetTracingFromFieldTrial();
  base::trace_event::EmitNamedTrigger(
      base::trace_event::kStartupTracingTriggerName);
}

}  // namespace android_webview
