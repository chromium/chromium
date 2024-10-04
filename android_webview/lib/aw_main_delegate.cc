// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/aw_main_delegate.h"

#include <memory>

#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_media_url_interceptor.h"
#include "android_webview/browser/gfx/aw_draw_fn_impl.h"
#include "android_webview/browser/gfx/browser_view_renderer.h"
#include "android_webview/browser/gfx/gpu_service_webview.h"
#include "android_webview/browser/gfx/viz_compositor_thread_runner_webview.h"
#include "android_webview/browser/tracing/aw_trace_event_args_allowlist.h"
#include "android_webview/common/aw_descriptors.h"
#include "android_webview/common/aw_features.h"
#include "android_webview/common/aw_paths.h"
#include "android_webview/common/aw_resource_bundle.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/common/crash_reporter/aw_crash_reporter_client.h"
#include "android_webview/common/crash_reporter/crash_keys.h"
#include "android_webview/gpu/aw_content_gpu_client.h"
#include "android_webview/renderer/aw_content_renderer_client.h"
#include "base/android/apk_assets.h"
#include "base/android/build_info.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/functional/bind.h"
#include "base/i18n/icu_util.h"
#include "base/i18n/rtl.h"
#include "base/posix/global_descriptors.h"
#include "base/scoped_add_feature_flags.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/crash/core/common/crash_key.h"
#include "components/embedder_support/switches.h"
#include "components/memory_system/initializer.h"
#include "components/memory_system/parameters.h"
#include "components/metrics/unsent_log_store_metrics.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"
#include "components/services/heap_profiling/public/cpp/profiling_client.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/variations/variations_ids_provider.h"
#include "components/version_info/android/channel_getter.h"
#include "components/viz/common/features.h"
#include "content/public/app/initialize_mojo_core.h"
#include "content/public/browser/android/media_url_interceptor_register.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/common/content_descriptor_keys.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "device/base/features.h"
#include "gin/public/isolate_holder.h"
#include "gin/v8_initializer.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_finch_features.h"
#include "media/media_buildflags.h"
#include "net/base/features.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "tools/v8_context_snapshot/buildflags.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "components/spellcheck/common/spellcheck_features.h"
#endif  // ENABLE_SPELLCHECK

namespace android_webview {

AwMainDelegate::AwMainDelegate() = default;

AwMainDelegate::~AwMainDelegate() = default;

std::optional<int> AwMainDelegate::BasicStartupComplete() {
  TRACE_EVENT0("startup", "AwMainDelegate::BasicStartupComplete");
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();

  // WebView uses the Android system's scrollbars and overscroll glow.
  cl->AppendSwitch(switches::kDisableOverscrollEdgeEffect);

  // Pull-to-refresh should never be a default WebView action.
  cl->AppendSwitch(switches::kDisablePullToRefreshEffect);

  // Not yet supported in single-process mode.
  cl->AppendSwitch(switches::kDisableSharedWorkers);

  // File system API not supported (requires some new API; internal bug 6930981)
  cl->AppendSwitch(switches::kDisableFileSystem);

  // Web Notification API and the Push API are not supported (crbug.com/434712)
  cl->AppendSwitch(switches::kDisableNotifications);

  // Check damage in OnBeginFrame to prevent unnecessary draws.
  cl->AppendSwitch(cc::switches::kCheckDamageEarly);

  // This is needed for sharing textures across the different GL threads.
  cl->AppendSwitch(switches::kEnableThreadedTextureMailboxes);

  // WebView does not yet support screen orientation locking.
  cl->AppendSwitch(switches::kDisableScreenOrientationLock);

  // WebView does not currently support Web Speech Synthesis API,
  // but it does support Web Speech Recognition API (crbug.com/487255).
  cl->AppendSwitch(switches::kDisableSpeechSynthesisAPI);

  // WebView does not (yet) save Chromium data during shutdown, so add setting
  // for Chrome to aggressively persist DOM Storage to minimize data loss.
  // http://crbug.com/479767
  cl->AppendSwitch(switches::kEnableAggressiveDOMStorageFlushing);

  // Webview does not currently support the Presentation API, see
  // https://crbug.com/521319
  cl->AppendSwitch(switches::kDisablePresentationAPI);

  // WebView doesn't support Remote Playback API for the same reason as the
  // Presentation API, see https://crbug.com/521319.
  cl->AppendSwitch(switches::kDisableRemotePlaybackAPI);

  // WebView does not support MediaSession API since there's no UI for media
  // metadata and controls.
  cl->AppendSwitch(switches::kDisableMediaSessionAPI);

  // We have crash dumps to diagnose regressions in remote font analysis or cc
  // serialization errors but most of their utility is in identifying URLs where
  // the regression occurs. This info is not available for webview so there
  // isn't much point in having the crash dumps there.
  cl->AppendSwitch(switches::kDisableOoprDebugCrashDump);

  // Deemed that performance benefit is not worth the stability cost.
  // See crbug.com/1309151.
  cl->AppendSwitch(switches::kDisableGpuShaderDiskCache);

  // Keep data: URL support in SVGUseElement for webview until deprecation is
  // completed in the Web Platform.
  cl->AppendSwitch(blink::switches::kDataUrlInSvgUseEnabled);

  if (cl->GetSwitchValueASCII(switches::kProcessType).empty()) {
    // Browser process (no type specified).

    content::RegisterMediaUrlInterceptor(new AwMediaUrlInterceptor());
    BrowserViewRenderer::CalculateTileMemoryPolicy();

    if (AwDrawFnImpl::IsUsingVulkan())
      cl->AppendSwitch(switches::kWebViewDrawFunctorUsesVulkan);

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
#if !BUILDFLAG(USE_V8_CONTEXT_SNAPSHOT) || BUILDFLAG(INCLUDE_BOTH_V8_SNAPSHOTS)
    base::android::RegisterApkAssetWithFileDescriptorStore(
        content::kV8Snapshot32DataDescriptor,
        gin::V8Initializer::GetSnapshotFilePath(
            true, gin::V8SnapshotFileType::kDefault));
    base::android::RegisterApkAssetWithFileDescriptorStore(
        content::kV8Snapshot64DataDescriptor,
        gin::V8Initializer::GetSnapshotFilePath(
            false, gin::V8SnapshotFileType::kDefault));
#endif
#if BUILDFLAG(USE_V8_CONTEXT_SNAPSHOT)
    base::android::RegisterApkAssetWithFileDescriptorStore(
        content::kV8ContextSnapshot32DataDescriptor,
        gin::V8Initializer::GetSnapshotFilePath(
            true, gin::V8SnapshotFileType::kWithAdditionalContext));
    base::android::RegisterApkAssetWithFileDescriptorStore(
        content::kV8ContextSnapshot64DataDescriptor,
        gin::V8Initializer::GetSnapshotFilePath(
            false, gin::V8SnapshotFileType::kWithAdditionalContext));
#endif
#endif  // V8_USE_EXTERNAL_STARTUP_DATA
  }

  if (cl->HasSwitch(switches::kWebViewSandboxedRenderer)) {
    cl->AppendSwitch(switches::kInProcessGPU);
  }

  {
    // TODO(crbug.com/40271903): Consider to migrate all the following overrides
    // to the new mechanism in android_webview/browser/aw_field_trials.cc.
    base::ScopedAddFeatureFlags features(cl);

    if (cl->HasSwitch(switches::kWebViewLogJsConsoleMessages)) {
      features.EnableIfNotSet(::features::kLogJsConsoleMessages);
    }

    if (!cl->HasSwitch(switches::kWebViewDrawFunctorUsesVulkan)) {
      // Not use ANGLE's Vulkan backend, if the draw functor is not using
      // Vulkan.
      features.DisableIfNotSet(::features::kDefaultANGLEVulkan);
    }

    if (cl->HasSwitch(switches::kWebViewFencedFrames)) {
      features.EnableIfNotSet(blink::features::kFencedFrames);
      features.EnableIfNotSet(blink::features::kFencedFramesAPIChanges);
      features.EnableIfNotSet(blink::features::kFencedFramesDefaultMode);
      features.EnableIfNotSet(::features::kFencedFramesEnforceFocus);
      features.EnableIfNotSet(::features::kPrivacySandboxAdsAPIsOverride);
    }

    features.EnableIfNotSet(metrics::kRecordLastUnsentLogMetadataMetrics);

    // Enabled by default for webview.
    features.EnableIfNotSet(::features::kWebViewThreadSafeMediaDefault);

    // WebView uses kWebViewFrameRateHints to control this. Not using
    // AwFieldTrials::RegisterFeatureOverrides to avoid misconfiguring
    // experimients accidentally enabling kUseFrameIntervalDecider for WebView.
    features.DisableIfNotSet(::features::kUseFrameIntervalDecider);
  }

  android_webview::RegisterPathProvider();

  // Used only if the argument filter is enabled in tracing config,
  // as is the case by default in aw_tracing_controller.cc
  base::trace_event::TraceLog::GetInstance()->SetArgumentFilterPredicate(
      base::BindRepeating(&IsTraceEventArgsAllowlisted));
  base::trace_event::TraceLog::GetInstance()->SetMetadataFilterPredicate(
      base::BindRepeating(&IsTraceMetadataAllowlisted));
  base::trace_event::TraceLog::GetInstance()->SetRecordHostAppPackageName(true);

  // The TLS slot used by the memlog allocator shim needs to be initialized
  // early to ensure that it gets assigned a low slot number. If it gets
  // initialized too late, the glibc TLS system will require a malloc call in
  // order to allocate storage for a higher slot number. Since malloc is hooked,
  // this causes re-entrancy into the allocator shim, while the TLS object is
  // partially-initialized, which the TLS object is supposed to protect again.
  heap_profiling::InitTLSSlot();

  // Have the network service in the browser process even if we have separate
  // renderer processes. See also: switches::kInProcessGPU above.
  content::ForceInProcessNetworkService();

  return std::nullopt;
}

void AwMainDelegate::PreSandboxStartup() {
  TRACE_EVENT0("startup", "AwMainDelegate::PreSandboxStartup");
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  const bool is_browser_process = process_type.empty();
  if (!is_browser_process) {
    base::i18n::SetICUDefaultLocale(
        command_line.GetSwitchValueASCII(switches::kLang));
  }

  if (process_type == switches::kRendererProcess) {
    InitResourceBundleRendererSide();
  }

  EnableCrashReporter(process_type);

  base::android::BuildInfo* android_build_info =
      base::android::BuildInfo::GetInstance();

  static ::crash_reporter::CrashKeyString<64> app_name_key(
      crash_keys::kAppPackageName);
  app_name_key.Set(android_build_info->host_package_name());

  static ::crash_reporter::CrashKeyString<64> app_version_key(
      crash_keys::kAppPackageVersionCode);
  app_version_key.Set(android_build_info->host_version_code());

  static ::crash_reporter::CrashKeyString<8> sdk_int_key(
      crash_keys::kAndroidSdkInt);
  sdk_int_key.Set(base::NumberToString(android_build_info->sdk_int()));
}

absl::variant<int, content::MainFunctionParams> AwMainDelegate::RunProcess(
    const std::string& process_type,
    content::MainFunctionParams main_function_params) {
  // Defer to the default main method outside the browser process.
  if (!process_type.empty())
    return std::move(main_function_params);

  browser_runner_ = content::BrowserMainRunner::Create();
  int exit_code = browser_runner_->Initialize(std::move(main_function_params));
  // We do not expect Initialize() to ever fail in AndroidWebView. On success
  // it returns a negative value but we do not want to use that on Android.
  DCHECK_LT(exit_code, 0);
  return 0;
}

void AwMainDelegate::ProcessExiting(const std::string& process_type) {
  // TODO(torne): Clean up resources when we handle them.

  logging::CloseLogFile();
}

bool AwMainDelegate::ShouldCreateFeatureList(InvokedIn invoked_in) {
  // In the browser process the FeatureList is created in
  // AwMainDelegate::PostEarlyInitialization().
  return absl::holds_alternative<InvokedInChildProcess>(invoked_in);
}

bool AwMainDelegate::ShouldInitializeMojo(InvokedIn invoked_in) {
  return ShouldCreateFeatureList(invoked_in);
}

variations::VariationsIdsProvider*
AwMainDelegate::CreateVariationsIdsProvider() {
  return variations::VariationsIdsProvider::Create(
      variations::VariationsIdsProvider::Mode::kDontSendSignedInVariations);
}

std::optional<int> AwMainDelegate::PostEarlyInitialization(
    InvokedIn invoked_in) {
  const bool is_browser_process =
      absl::holds_alternative<InvokedInBrowserProcess>(invoked_in);
  if (is_browser_process) {
    InitIcuAndResourceBundleBrowserSide();
    aw_feature_list_creator_->CreateFeatureListAndFieldTrials();
    content::InitializeMojoCore();

    // WebView apps can override WebView#computeScroll to achieve custom
    // scroll/fling. As a result, fling animations may not be ticked,
    // potentially
    // confusing the tap suppression controller. Simply disable it for WebView
    if (!base::FeatureList::IsEnabled(
            ::features::kWebViewSuppressTapDuringFling)) {
      ui::GestureConfiguration::GetInstance()
          ->set_fling_touchscreen_tap_suppression_enabled(false);
    }
  }

  InitializeMemorySystem(is_browser_process);

  return std::nullopt;
}

content::ContentClient* AwMainDelegate::CreateContentClient() {
  return &content_client_;
}

content::ContentBrowserClient* AwMainDelegate::CreateContentBrowserClient() {
  DCHECK(!aw_feature_list_creator_);
  aw_feature_list_creator_ = std::make_unique<AwFeatureListCreator>();
  content_browser_client_ =
      std::make_unique<AwContentBrowserClient>(aw_feature_list_creator_.get());
  return content_browser_client_.get();
}

namespace {
gpu::SyncPointManager* GetSyncPointManager() {
  DCHECK(GpuServiceWebView::GetInstance());
  return GpuServiceWebView::GetInstance()->sync_point_manager();
}

gpu::SharedImageManager* GetSharedImageManager() {
  DCHECK(GpuServiceWebView::GetInstance());
  return GpuServiceWebView::GetInstance()->shared_image_manager();
}

gpu::Scheduler* GetScheduler() {
  DCHECK(GpuServiceWebView::GetInstance());
  return GpuServiceWebView::GetInstance()->scheduler();
}

viz::VizCompositorThreadRunner* GetVizCompositorThreadRunner() {
  return VizCompositorThreadRunnerWebView::GetInstance();
}

}  // namespace

content::ContentGpuClient* AwMainDelegate::CreateContentGpuClient() {
  content_gpu_client_ = std::make_unique<AwContentGpuClient>(
      base::BindRepeating(&GetSyncPointManager),
      base::BindRepeating(&GetSharedImageManager),
      base::BindRepeating(&GetScheduler),
      base::BindRepeating(&GetVizCompositorThreadRunner));
  return content_gpu_client_.get();
}

content::ContentRendererClient* AwMainDelegate::CreateContentRendererClient() {
  content_renderer_client_ = std::make_unique<AwContentRendererClient>();
  return content_renderer_client_.get();
}

void AwMainDelegate::InitializeMemorySystem(const bool is_browser_process) {
  const version_info::Channel channel = version_info::android::GetChannel();
  const bool is_canary_dev = (channel == version_info::Channel::CANARY ||
                              channel == version_info::Channel::DEV);
  const std::string process_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);
  const bool gwp_asan_boost_sampling = is_canary_dev || is_browser_process;

  // Add PoissonAllocationSampler. On Android WebView we do not have obvious
  // observers of PoissonAllocationSampler. Unfortunately, some potential
  // candidates are still linked and may sneak in through hidden paths.
  // Therefore, we include PoissonAllocationSampler unconditionally.
  // TODO(crbug.com/40062835): Which observers of PoissonAllocationSampler are
  // really in use on Android WebView? Can we add the sampler conditionally or
  // remove it completely?
  memory_system::Initializer()
      .SetGwpAsanParameters(gwp_asan_boost_sampling, process_type)
      .SetDispatcherParameters(memory_system::DispatcherParameters::
                                   PoissonAllocationSamplerInclusion::kEnforce,
                               memory_system::DispatcherParameters::
                                   AllocationTraceRecorderInclusion::kIgnore,
                               process_type)
      .Initialize(memory_system_);
}
}  // namespace android_webview
