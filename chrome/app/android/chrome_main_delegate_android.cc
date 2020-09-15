// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/android/chrome_main_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/base_paths_android.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/android/chrome_startup_flags.h"
#include "chrome/browser/android/metrics/uma_utils.h"
#include "chrome/common/profiler/main_thread_stack_sampling_profiler.h"
#include "components/policy/core/common/android/android_combined_policy_provider.h"
#include "components/safe_browsing/buildflags.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/browser/browser_main_runner.h"

#if BUILDFLAG(SAFE_BROWSING_DB_REMOTE)
#include "components/safe_browsing/android/safe_browsing_api_handler.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"
#endif

namespace {
using safe_browsing::SafeBrowsingApiHandler;

// Whether to use the process start time for startup metrics.
const base::Feature kUseProcessStartTimeForMetrics{
    "UseProcessStartTimeForMetrics", base::FEATURE_DISABLED_BY_DEFAULT};
}  // namespace

// ChromeMainDelegateAndroid is created when the library is loaded. It is always
// done in the process' main Java thread. But for a non-browser process, e.g.
// renderer process, it is not the native Chrome's main thread.
ChromeMainDelegateAndroid::ChromeMainDelegateAndroid() = default;
ChromeMainDelegateAndroid::~ChromeMainDelegateAndroid() = default;

bool ChromeMainDelegateAndroid::BasicStartupComplete(int* exit_code) {
#if BUILDFLAG(SAFE_BROWSING_DB_REMOTE)
  safe_browsing_api_handler_.reset(
      new safe_browsing::SafeBrowsingApiHandlerBridge());
  SafeBrowsingApiHandler::SetInstance(safe_browsing_api_handler_.get());
#endif

  policy::android::AndroidCombinedPolicyProvider::SetShouldWaitForPolicy(true);
  SetChromeSpecificCommandLineFlags();

  return ChromeMainDelegate::BasicStartupComplete(exit_code);
}

void ChromeMainDelegateAndroid::PreSandboxStartup() {
  ChromeMainDelegate::PreSandboxStartup();

  // Start the sampling profiler after crashpad initialization.
  sampling_profiler_ = std::make_unique<MainThreadStackSamplingProfiler>();
}

void ChromeMainDelegateAndroid::SecureDataDirectory() {
  // By default, Android creates the directory accessible by others.
  // We'd like to tighten security and make it accessible only by
  // the browser process.
  // TODO(crbug.com/832388): Remove this once minsdk >= 21,
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

int ChromeMainDelegateAndroid::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  TRACE_EVENT0("startup", "ChromeMainDelegateAndroid::RunProcess");
  // Defer to the default main method outside the browser process.
  if (!process_type.empty())
    return -1;

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
      startup_metric_utils::RecordStartupProcessCreationTime(
          process_start_time);
      // TODO(crbug.com/1127482): Perf bots should add support for measuring
      // Startup.LoadTime.ProcessCreateToApplicationStart, then the
      // kUseProcessStartTimeForMetrics feature can be removed.
      if (base::FeatureList::IsEnabled(kUseProcessStartTimeForMetrics))
        application_start_time = process_start_time;
    }
    startup_metric_utils::RecordApplicationStartTime(application_start_time);
    browser_runner_ = content::BrowserMainRunner::Create();
  }

  int exit_code = browser_runner_->Initialize(main_function_params);
  // On Android we do not run BrowserMain(), so the above initialization of a
  // BrowserMainRunner is all we want to occur. Return >= 0 to avoid running
  // BrowserMain, while preserving any error codes > 0.
  if (exit_code > 0)
    return exit_code;
  return 0;
}

void ChromeMainDelegateAndroid::ProcessExiting(
    const std::string& process_type) {
#if BUILDFLAG(SAFE_BROWSING_DB_REMOTE)
  SafeBrowsingApiHandler::SetInstance(nullptr);
#endif
}
