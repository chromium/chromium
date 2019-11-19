// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/uma_utils.h"

#include <stdint.h>

#include "chrome/android/chrome_jni_headers/UmaUtils_jni.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "components/metrics/metrics_reporting_default_state.h"

using base::android::JavaParamRef;

class PrefService;

namespace chrome {
namespace android {

base::TimeTicks GetMainEntryPointTimeTicks() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::TimeTicks::FromUptimeMillis(
      Java_UmaUtils_getMainEntryPointTicks(env));
}

static jboolean JNI_UmaUtils_IsClientInMetricsReportingSample(JNIEnv* env) {
  return ChromeMetricsServicesManagerClient::IsClientInSample();
}

static void JNI_UmaUtils_RecordMetricsReportingDefaultOptIn(
    JNIEnv* env,
    jboolean opt_in) {
  DCHECK(g_browser_process);
  PrefService* local_state = g_browser_process->local_state();
  metrics::RecordMetricsReportingDefaultState(
      local_state, opt_in ? metrics::EnableMetricsDefault::OPT_IN
                          : metrics::EnableMetricsDefault::OPT_OUT);
}

void SetUsageAndCrashReporting(bool enabled) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_UmaUtils_setUsageAndCrashReportingFromNative(env, enabled);
}

}  // namespace android
}  // namespace chrome
