// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "data_saver.h"

#include <optional>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/android/chrome_jni_headers/DataSaverOSSetting_jni.h"
#endif

namespace {
std::optional<bool> g_override_data_saver_for_testing;
#if BUILDFLAG(IS_ANDROID)
// Whether the Data Saver Android setting was set last time we checked it.
// This can be a global variable because this is an OS setting that does not
// vary based on Chrome profiles.
std::optional<bool> g_cached_data_saver_setting;
base::TimeTicks g_last_setting_check_time;

bool IsDataSaverEnabledBlockingCall() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return datareduction::android::Java_DataSaverOSSetting_isDataSaverEnabled(
      env);
}
#endif

}  // namespace

namespace data_saver {

void OverrideIsDataSaverEnabledForTesting(bool flag) {
  g_override_data_saver_for_testing = flag;
}

void ResetIsDataSaverEnabledForTesting() {
  g_override_data_saver_for_testing = std::nullopt;
}

void FetchDataSaverOSSettingAsynchronously() {
#if BUILDFLAG(IS_ANDROID)
  g_last_setting_check_time = base::TimeTicks::Now();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(IsDataSaverEnabledBlockingCall),
      base::BindOnce([](bool data_saver_enabled) {
        g_cached_data_saver_setting = data_saver_enabled;
      }));
#endif
}

bool IsDataSaverEnabled() {
  if (g_override_data_saver_for_testing.has_value()) {
    return g_override_data_saver_for_testing.value();
  }
#if BUILDFLAG(IS_ANDROID)
  if (!g_cached_data_saver_setting) {
    // No cached value, so block until we find the result. Note that
    // FetchDataSaverOSSettingAsynchronously is called on startup, so we
    // expect this case to rarely happen.
    g_cached_data_saver_setting = IsDataSaverEnabledBlockingCall();
    return g_cached_data_saver_setting.value();
  }

  // There is a cached value.
  if (base::TimeTicks::Now() - g_last_setting_check_time > base::Seconds(1)) {
    // It's been more than one second since we checked the OS setting. Update
    // it asynchronously and return the cached value immediately.
    FetchDataSaverOSSettingAsynchronously();
  }
  DCHECK(g_cached_data_saver_setting);
  return g_cached_data_saver_setting.value();
#else
  return false;
#endif
}

}  // namespace data_saver
