// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_process_singleton.h"

#include <utility>

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/compiler_specific.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/common/chrome_features.h"
#include "components/app_launch_prefetch/app_launch_prefetch.h"
#endif

namespace {

ChromeProcessSingleton* g_chrome_process_singleton_ = nullptr;

}  // namespace

ChromeProcessSingleton::ChromeProcessSingleton(
    const base::FilePath& user_data_dir)
    : startup_lock_(
          base::BindRepeating(&ChromeProcessSingleton::NotificationCallback,
                              base::Unretained(this))),
      process_singleton_(user_data_dir,
                         startup_lock_.AsNotificationCallback()) {}

ChromeProcessSingleton::~ChromeProcessSingleton() = default;

ProcessSingleton::NotifyResult
    ChromeProcessSingleton::NotifyOtherProcessOrCreate() {
  CHECK(!is_singleton_instance_);
  ProcessSingleton::NotifyResult result =
      process_singleton_.NotifyOtherProcessOrCreate();
  if (result == ProcessSingleton::PROCESS_NONE) {
    is_singleton_instance_ = true;
  }
  return result;
}

void ChromeProcessSingleton::StartWatching() {
  process_singleton_.StartWatching();
}

void ChromeProcessSingleton::Cleanup() {
  if (is_singleton_instance_) {
    process_singleton_.Cleanup();
  }
}

void ChromeProcessSingleton::Unlock(
    const ProcessSingleton::NotificationCallback& notification_callback) {
  notification_callback_ = notification_callback;
  startup_lock_.Unlock();
}

#if BUILDFLAG(IS_WIN)
void ChromeProcessSingleton::InitializeFeatures() {
  // On Windows, App Launch Prefetch (ALPF) will monitor the disk accesses done
  // by processes launched, and load the resources used into memory before the
  // process needs them, if possible. Different Chrome process types use
  // different resources, and this is signaled to ALPF via the command line:
  // passing "/prefetch:N" on the command line with different numbers causes
  // ALPF to treat two launches placed in different buckets as separarate
  // applications for its purposes.
  //
  // Short lived browser processes occur on notification and rendezvous, both
  // cases where nearly nothing will be used from disk, and which are not
  // launched with `/prefetch`, and are thus considered "browser" processes
  // according to ALPF. This may be polluting the ALPF cache.
  //
  // The `::ProcessOverrideSubsequentPrefetchParameter` process information
  // attribute will change the behavior of ALPF to explicitly consider
  // subsequent launches while this singleton process is running (rendezvous,
  // toast) of Chrome which do not specify a prefetch bucket as if they had
  // specified the `kCatchAll` bucket.
  //
  // It is expected that this will overall improve the behavior of ALPF on
  // Windows, which should decrease startup time for ordinary browser processes.
  //
  // SAFETY: `::GetCommandLineW()` returns a pointer that is guaranteed to be
  // non-null and NUL-terminated.
  if (is_singleton_instance_ &&
      (UNSAFE_BUFFERS(wcsstr(::GetCommandLineW(), L"/prefetch:")) == nullptr) &&
      base::FeatureList::IsEnabled(features::kOverridePrefetchOnSingleton)) {
    OVERRIDE_PREFETCH_PARAMETER prefetch_parameter = {};
    prefetch_parameter.Value = app_launch_prefetch::GetPrefetchBucket(
        app_launch_prefetch::SubprocessType::kCatchAll);
    // This is not fatal because it is an optimization and has no bearing on the
    // functionality of the browser. See crbug.com/380088804 for details. It has
    // been seen that occasionally (in CQ), this call fails with
    // ERROR_INTERNAL_ERROR.
    base::UmaHistogramSparse(
        "Startup.PrefetchOverrideErrorCode",
        ::SetProcessInformation(::GetCurrentProcess(),
                                ::ProcessOverrideSubsequentPrefetchParameter,
                                &prefetch_parameter, sizeof(prefetch_parameter))
            ? ERROR_SUCCESS
            : ::GetLastError());
  }
}
#endif

// static
void ChromeProcessSingleton::CreateInstance(
    const base::FilePath& user_data_dir) {
  DCHECK(!g_chrome_process_singleton_);
  DCHECK(!user_data_dir.empty());
  g_chrome_process_singleton_ = new ChromeProcessSingleton(user_data_dir);
}

// static
void ChromeProcessSingleton::DeleteInstance() {
  if (g_chrome_process_singleton_) {
    delete g_chrome_process_singleton_;
    g_chrome_process_singleton_ = nullptr;
  }
}

// static
ChromeProcessSingleton* ChromeProcessSingleton::GetInstance() {
  CHECK(g_chrome_process_singleton_);
  return g_chrome_process_singleton_;
}

// static
bool ChromeProcessSingleton::IsSingletonInstance() {
  return g_chrome_process_singleton_ &&
         g_chrome_process_singleton_->is_singleton_instance_;
}

bool ChromeProcessSingleton::NotificationCallback(
    base::CommandLine command_line,
    const base::FilePath& current_directory) {
  DCHECK(notification_callback_);
  return notification_callback_.Run(std::move(command_line), current_directory);
}
