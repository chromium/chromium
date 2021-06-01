// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_JANK_METRIC_UMA_RECORDER_H_
#define BASE_ANDROID_JANK_METRIC_UMA_RECORDER_H_

#include "base/android/jni_android.h"
#include "base/base_export.h"

namespace base {
namespace android {

BASE_EXPORT void RecordJankMetrics(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& java_scenario_name,
    const base::android::JavaParamRef<jlongArray>& java_durations_ns,
    const base::android::JavaParamRef<jlongArray>& java_jank_bursts_ns,
    jint java_missed_frames);

}  // namespace android
}  // namespace base
#endif  // BASE_ANDROID_JANK_METRIC_UMA_RECORDER_H_
