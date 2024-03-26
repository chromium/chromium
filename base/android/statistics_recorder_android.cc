// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/jni_string.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/system/sys_info.h"

// Must come after other headers because it uses JSONVerbosityLevel.
#include "base/base_jni/StatisticsRecorderAndroid_jni.h"

namespace base {
namespace android {

static std::string JNI_StatisticsRecorderAndroid_ToJson(
    JNIEnv* env,
    JSONVerbosityLevel verbosityLevel) {
  return base::StatisticsRecorder::ToJSON(verbosityLevel);
}

}  // namespace android
}  // namespace base
