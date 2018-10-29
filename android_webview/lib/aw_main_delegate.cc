// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/aw_main_delegate.h"

#include <memory>

#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_media_url_interceptor.h"
#include "android_webview/browser/browser_view_renderer.h"
#include "android_webview/browser/command_line_helper.h"
#include "android_webview/browser/deferred_gpu_command_service.h"
#include "android_webview/browser/tracing/aw_trace_event_args_whitelist.h"
#include "android_webview/common/aw_descriptors.h"
#include "android_webview/common/aw_paths.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/common/crash_reporter/aw_crash_reporter_client.h"
#include "android_webview/common/crash_reporter/crash_keys.h"
#include "android_webview/gpu/aw_content_gpu_client.h"
#include "android_webview/renderer/aw_content_renderer_client.h"
#include "android_webview/utility/aw_content_utility_client.h"
#include "base/android/apk_assets.h"
#include "base/android/build_info.h"
#include "base/android/locale_utils.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/i18n/icu_util.h"
#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "cc/base/switches.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/crash/content/app/breakpad_linux.h"
#include "components/crash/core/common/crash_key.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"
#include "components/services/heap_profiling/public/cpp/allocator_shim.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/viz/common/features.h"
#include "content/public/browser/android/browser_media_player_manager_register.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_descriptor_keys.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "gin/public/isolate_holder.h"
#include "gin/v8_initializer.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/ipc/gl_in_process_context.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_bundle_android.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/gesture_detection/gesture_configuration.h"

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "components/spellcheck/common/spellcheck_features.h"
#endif  // ENABLE_SPELLCHECK

namespace android_webview {

AwMainDelegate::AwMainDelegate() {}

AwMainDelegate::~AwMainDelegate() {}

bool AwMainDelegate::BasicStartupComplete(int* exit_code) {
  content::SetContentClient(&content_client_);

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

  // WebRTC hardware decoding is not supported, internal bug 15075307
  cl->AppendSwitch(switches::kDisableWebRtcHWDecoding);

  // Check damage in OnBeginFrame to prevent unnecessary draws.
  cl->AppendSwitch(cc::switches::kCheckDamageEarly);

  // This is needed for sharing textures across the different GL threads.
  cl->AppendSwitch(switches::kEnableThreadedTextureMailboxes);

  // WebView does not yet support screen orientation locking.
  cl->AppendSwitch(switches::kDisableScreenOrientationLock);

  // WebView does not currently support Web Speech API (crbug.com/487255)
  cl->AppendSwitch(switches::kDisableSpeechAPI);

  // WebView does not currently support the Permissions API (crbug.com/490120)
  cl->AppendSwitch(switches::kDisablePermissionsAPI);

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

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  if (cl->GetSwitchValueASCII(switches::kProcessType).empty()) {
    // Browser process (no type specified).

    content::RegisterMediaUrlInterceptor(new AwMediaUrlInterceptor());
    BrowserViewRenderer::CalculateTileMemoryPolicy();
    // WebView apps can override WebView#computeScroll to achieve custom
    // scroll/fling. As a result, fling animations may not be ticked,
    // potentially
    // confusing the tap suppression controller. Simply disable it for WebView
    ui::GestureConfiguration::GetInstance()
        ->set_fling_touchscreen_tap_suppression_enabled(false);

    base::android::RegisterApkAssetWithFileDescriptorStore(
        content::kV8NativesDataDescriptor,
        gin::V8Initializer::GetNativesFilePath());
#if defined(USE_V8_CONTEXT_SNAPSHOT)
    gin::V8Initializer::V8SnapshotFileType file_type =
        gin::V8Initializer::V8SnapshotFileType::kWithAdditionalContext;
#else
    gin::V8Initializer::V8SnapshotFileType file_type =
        gin::V8Initializer::V8SnapshotFileType::kDefault;
#endif
    base::android::RegisterApkAssetWithFileDescriptorStore(
        content::kV8Snapshot32DataDescriptor,
        gin::V8Initializer::GetSnapshotFilePath(true, file_type));
    base::android::RegisterApkAssetWithFileDescriptorStore(
        content::kV8Snapshot64DataDescriptor,
        gin::V8Initializer::GetSnapshotFilePath(false, file_type));
  }
#endif  // V8_USE_EXTERNAL_STARTUP_DATA

  if (cl->HasSwitch(switches::kWebViewSandboxedRenderer)) {
    content::RenderProcessHost::SetMaxRendererProcessCount(1u);
    cl->AppendSwitch(switches::kInProcessGPU);
  }

#if BUILDFLAG(ENABLE_SPELLCHECK)
  CommandLineHelper::AddEnabledFeature(
      *cl, spellcheck::kAndroidSpellCheckerNonLowEnd.name);
#endif  // ENABLE_SPELLCHECK

  CommandLineHelper::AddDisabledFeature(*cl, features::kWebPayments.name);

  // WebView does not and should not support WebAuthN.
  CommandLineHelper::AddDisabledFeature(*cl, features::kWebAuth.name);

  // WebView isn't compatible with OOP-D.
  CommandLineHelper::AddDisabledFeature(*cl,
                                        features::kVizDisplayCompositor.name);

  // WebView does not support AndroidOverlay yet for video overlays.
  CommandLineHelper::AddDisabledFeature(*cl, media::kUseAndroidOverlay.name);

  // WebView doesn't support embedding CompositorFrameSinks which is needed for
  // UseSurfaceLayerForVideo[PIP] feature. https://crbug.com/853832
  CommandLineHelper::AddDisabledFeature(*cl,
                                        media::kUseSurfaceLayerForVideo.name);
  CommandLineHelper::AddDisabledFeature(
      *cl, media::kUseSurfaceLayerForVideoPIP.name);

  // WebView does not support EME persistent license yet, because it's not
  // clear on how user can remove persistent media licenses from UI.
  CommandLineHelper::AddDisabledFeature(*cl,
                                        media::kMediaDrmPersistentLicense.name);

  CommandLineHelper::AddEnabledFeature(
      *cl, autofill::features::kAutofillSkipComparingInferredLabels.name);

  CommandLineHelper::AddDisabledFeature(
      *cl, autofill::features::kAutofillRestrictUnownedFieldsToFormlessCheckout
               .name);

  CommandLineHelper::AddDisabledFeature(*cl, features::kBackgroundFetch.name);

  android_webview::RegisterPathProvider();

  safe_browsing_api_handler_.reset(
      new safe_browsing::SafeBrowsingApiHandlerBridge());
  safe_browsing::SafeBrowsingApiHandler::SetInstance(
      safe_browsing_api_handler_.get());

  // Used only if the argument filter is enabled in tracing config,
  // as is the case by default in aw_tracing_controller.cc
  base::trace_event::TraceLog::GetInstance()->SetArgumentFilterPredicate(
      base::BindRepeating(&IsTraceEventArgsWhitelisted));

  // The TLS slot used by the memlog allocator shim needs to be initialized
  // early to ensure that it gets assigned a low slot number. If it gets
  // initialized too late, the glibc TLS system will require a malloc call in
  // order to allocate storage for a higher slot number. Since malloc is hooked,
  // this causes re-entrancy into the allocator shim, while the TLS object is
  // partially-initialized, which the TLS object is supposed to protect again.
  heap_profiling::InitTLSSlot();

  return false;
}

void AwMainDelegate::PreSandboxStartup() {
#if defined(ARCH_CPU_ARM_FAMILY)
  // Create an instance of the CPU class to parse /proc/cpuinfo and cache
  // cpu_brand info.
  base::CPU cpu_info;
#endif

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  const std::string locale = command_line.GetSwitchValueASCII(switches::kLang);
  base::i18n::SetICUDefaultLocale(locale);

  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  int crash_signal_fd = -1;
  if (process_type == switches::kRendererProcess) {
    auto* global_descriptors = base::GlobalDescriptors::GetInstance();
    int pak_fd = global_descriptors->Get(kAndroidWebViewLocalePakDescriptor);
    base::MemoryMappedFile::Region pak_region =
        global_descriptors->GetRegion(kAndroidWebViewLocalePakDescriptor);
    ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(base::File(pak_fd),
                                                            pak_region);

    std::pair<int, ui::ScaleFactor> extra_paks[] = {
        {kAndroidWebViewMainPakDescriptor, ui::SCALE_FACTOR_NONE},
        {kAndroidWebView100PercentPakDescriptor, ui::SCALE_FACTOR_100P}};

    for (const auto& pak_info : extra_paks) {
      pak_fd = global_descriptors->Get(pak_info.first);
      pak_region = global_descriptors->GetRegion(pak_info.first);
      ui::ResourceBundle::GetSharedInstance().AddDataPackFromFileRegion(
          base::File(pak_fd), pak_region, pak_info.second);
    }

    crash_signal_fd =
        global_descriptors->Get(kAndroidWebViewCrashSignalDescriptor);
  }
  if (process_type.empty()) {
    if (command_line.HasSwitch(switches::kWebViewSandboxedRenderer)) {
      process_type = breakpad::kBrowserProcessType;
    } else {
      process_type = breakpad::kWebViewSingleProcessType;
    }
  }

  crash_reporter::EnableCrashReporter(process_type, crash_signal_fd);

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
  sdk_int_key.Set(base::IntToString(android_build_info->sdk_int()));
}

int AwMainDelegate::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  if (process_type.empty()) {
    browser_runner_.reset(content::BrowserMainRunner::Create());
    int exit_code = browser_runner_->Initialize(main_function_params);
    DCHECK_LT(exit_code, 0);

    // At this point the content client has received the GPU info required
    // to create a GPU fingerpring, and we can pass it to the microdump
    // crash handler on the same thread as the crash handler was initialized.
    crash_reporter::AddGpuFingerprintToMicrodumpCrashHandler(
        content_client_.gpu_fingerprint());

    // Return 0 so that we do NOT trigger the default behavior. On Android, the
    // UI message loop is managed by the Java application.
    return 0;
  }

  return -1;
}

void AwMainDelegate::ProcessExiting(const std::string& process_type) {
  // TODO(torne): Clean up resources when we handle them.

  logging::CloseLogFile();
}

bool AwMainDelegate::ShouldCreateFeatureList() {
  // TODO(https://crbug.com/887468): Move the creation of FeatureList from
  // AwBrowserMainParts::PreCreateThreads() to
  // AwMainDelegate::PostEarlyInitialization().
  return false;
}

void AwMainDelegate::PostEarlyInitialization(bool is_running_tests) {
  ui::SetLocalePaksStoredInApk(true);
  std::string locale = ui::ResourceBundle::InitSharedInstanceWithLocale(
      base::android::GetDefaultLocaleString(), NULL,
      ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  if (locale.empty()) {
    LOG(WARNING) << "Failed to load locale .pak from the apk. "
                    "Bringing up WebView without any locale";
  }
  base::i18n::SetICUDefaultLocale(locale);

  // Try to directly mmap the resources.pak from the apk. Fall back to load
  // from file, using PATH_SERVICE, otherwise.
  base::FilePath pak_file_path;
  base::PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &pak_file_path);
  pak_file_path = pak_file_path.AppendASCII("resources.pak");
  ui::LoadMainAndroidPackFile("assets/resources.pak", pak_file_path);
}

content::ContentBrowserClient* AwMainDelegate::CreateContentBrowserClient() {
  content_browser_client_.reset(new AwContentBrowserClient());
  return content_browser_client_.get();
}

namespace {
gpu::SyncPointManager* GetSyncPointManager() {
  DCHECK(DeferredGpuCommandService::GetInstance());
  return DeferredGpuCommandService::GetInstance()->sync_point_manager();
}
}  // namespace

content::ContentGpuClient* AwMainDelegate::CreateContentGpuClient() {
  content_gpu_client_.reset(
      new AwContentGpuClient(base::BindRepeating(&GetSyncPointManager)));
  return content_gpu_client_.get();
}

content::ContentRendererClient* AwMainDelegate::CreateContentRendererClient() {
  content_renderer_client_.reset(new AwContentRendererClient());
  return content_renderer_client_.get();
}

content::ContentUtilityClient* AwMainDelegate::CreateContentUtilityClient() {
  content_utility_client_.reset(new AwContentUtilityClient());
  return content_utility_client_.get();
}

}  // namespace android_webview
