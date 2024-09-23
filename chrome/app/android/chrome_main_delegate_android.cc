// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/android/chrome_main_delegate_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/base_paths_android.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/android/chrome_startup_flags.h"
#include "chrome/browser/android/metrics/uma_utils.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/common/profiler/main_thread_stack_sampling_profiler.h"
#include "components/policy/core/common/android/android_combined_policy_provider.h"
#include "components/startup_metric_utils/common/startup_metric_utils.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"

namespace {
// Whether to use the process start time for startup metrics.
BASE_FEATURE(kUseProcessStartTimeForMetrics,
             "UseProcessStartTimeForMetrics",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace

// ChromeMainDelegateAndroid is created when the library is loaded. It is always
// done in the process' main Java thread. But for a non-browser process, e.g.
// renderer process, it is not the native Chrome's main thread.
ChromeMainDelegateAndroid::ChromeMainDelegateAndroid() = default;
ChromeMainDelegateAndroid::~ChromeMainDelegateAndroid() = default;

std::optional<int> ChromeMainDelegateAndroid::BasicStartupComplete() {
  TRACE_EVENT0("startup", "ChromeMainDelegateAndroid::BasicStartupComplete");
  policy::android::AndroidCombinedPolicyProvider::SetShouldWaitForPolicy(true);
  SetChromeSpecificCommandLineFlags();

  return ChromeMainDelegate::BasicStartupComplete();
}

void ChromeMainDelegateAndroid::PreSandboxStartup() {
  ChromeMainDelegate::PreSandboxStartup();

  // PoissonAllocationSampler's TLS slots need to be set up before
  // MainThreadStackSamplingProfiler, which can allocate TLS slots of its own.
  // On some platforms pthreads can malloc internally to access higher-numbered
  // TLS slots, which can cause reentry in the heap profiler. (See the comment
  // on ReentryGuard::InitTLSSlot().)
  // TODO(crbug.com/40062835): Clean up other paths that call this Init()
  // function, which are now redundant.
  base::PoissonAllocationSampler::Init();

  // We only create a MainThreadStackSamplingProfiler for the browser process.
  // `ChromeContentGpuClient` and `ChromeContentRendererClient` create their own
  // `ThreadProfiler` for the child processes.
  auto type = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kProcessType);
  if (type.empty()) {
    CHECK(chrome_content_browser_client_);
    // Start the sampling profiler after crashpad initialization.
    chrome_content_browser_client_->SetSamplingProfiler(
        std::make_unique<MainThreadStackSamplingProfiler>());
  }
}

void ChromeMainDelegateAndroid::SecureDataDirectory() {
  // By default, Android creates the directory accessible by others.
  // We'd like to tighten security and make it accessible only by
  // the browser process.
  // TODO(crbug.com/41382891): Remove this once minsdk >= 21,
  // at which point this will be handled by PathUtils.java.
  base::FilePath data_path;
  bool ok = base::PathService::Get(base::DIR_ANDROID_APP_DATA, &data_path);
  if (ok) {
    ok = base::SetPosixFilePermissions(data_path,
                                       base::FILE_PERMISSION_USER_MASK);
  }
  if (!ok) {
    LOG(ERROR) << "Failed to set data directory permissions";
  }
}

absl::variant<int, content::MainFunctionParams>
ChromeMainDelegateAndroid::RunProcess(
    const std::string& process_type,
    content::MainFunctionParams main_function_params) {
  TRACE_EVENT0("startup", "ChromeMainDelegateAndroid::RunProcess");
  // Defer to the default main method outside the browser process.
  if (!process_type.empty())
    return std::move(main_function_params);

  SecureDataDirectory();

  // Because the browser process can be started asynchronously as a series of
  // UI thread tasks a second request to start it can come in while the
  // first request is still being processed. Chrome must keep the same
  // browser runner for the second request.
  // Also only record the start time the first time round, since this is the
  // start time of the application, and will be same for all requests.
  if (!browser_runner_) {
    base::TimeTicks process_start_time = chrome::android::GetProcessStartTime();
    base::TimeTicks application_start_time =
        chrome::android::GetApplicationStartTime();
    if (!process_start_time.is_null()) {
      startup_metric_utils::GetCommon().RecordStartupProcessCreationTime(
          process_start_time);
      // TODO(crbug.com/40719075): Perf bots should add support for measuring
      // Startup.LoadTime.ProcessCreateToApplicationStart, then the
      // kUseProcessStartTimeForMetrics feature can be removed.
      if (base::FeatureList::IsEnabled(kUseProcessStartTimeForMetrics))
        application_start_time = process_start_time;
    }
    startup_metric_utils::GetCommon().RecordApplicationStartTime(
        application_start_time);
    browser_runner_ = content::BrowserMainRunner::Create();
  }

  int exit_code = browser_runner_->Initialize(std::move(main_function_params));
  // On Android we do not run BrowserMain(), so the above initialization of a
  // BrowserMainRunner is all we want to occur. Preserve any error codes > 0.
  if (exit_code > 0)
    return exit_code;
  return 0;
}
