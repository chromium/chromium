// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_JANK_METRIC_UMA_RECORDER_H_
#define BASE_ANDROID_JANK_METRIC_UMA_RECORDER_H_

#include "base/android/jni_android.h"
#include "base/base_export.h"

namespace base::android {

BASE_EXPORT void RecordJankMetrics(
    JNIEnv* env,
    const base::android::JavaParamRef<jlongArray>& java_durations_ns,
    const base::android::JavaParamRef<jbooleanArray>& java_jank_status,
    jlong java_reporting_interval_start_time,
    jlong java_reporting_interval_duration);
}  // namespace base::android
#endif  // BASE_ANDROID_JANK_METRIC_UMA_RECORDER_H_
