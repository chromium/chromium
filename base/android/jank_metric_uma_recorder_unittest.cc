// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jank_metric_uma_recorder.h"

#include <jni.h>

#include <cstddef>
#include <cstdint>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/metrics/histogram.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace base::android {
namespace {

jlongArray GenerateJavaLongArray(JNIEnv* env,
                                 const int64_t long_array[],
                                 const size_t array_length) {
  ScopedJavaLocalRef<jlongArray> java_long_array =
      ToJavaLongArray(env, long_array, array_length);

  return java_long_array.Release();
}

// Durations are received in nanoseconds, but are recorded to UMA in
// milliseconds.
const int64_t kDurations[] = {
    1'000'000,   // 1ms
    2'000'000,   // 2ms
    30'000'000,  // 30ms
    10'000'000,  // 10ms
    60'000'000,  // 60ms
    1'000'000,   // 1ms
    1'000'000,   // 1ms
    20'000'000,  // 20ms
};
const size_t kDurationsLen = std::size(kDurations);

jbooleanArray GenerateJavaBooleanArray(JNIEnv* env,
                                       const bool bool_array[],
                                       const size_t array_length) {
  ScopedJavaLocalRef<jbooleanArray> java_bool_array =
      ToJavaBooleanArray(env, bool_array, array_length);

  return java_bool_array.Release();
}

const bool kJankStatus[] = {
    false, false, true, false, true, false, false, false,
};

const size_t kJankStatusLen = kDurationsLen;

}  // namespace

TEST(JankMetricUMARecorder, TestUMARecording) {
  HistogramTester histogram_tester;

  JNIEnv* env = AttachCurrentThread();

  jlongArray java_durations =
      GenerateJavaLongArray(env, kDurations, kDurationsLen);

  jbooleanArray java_jank_status =
      GenerateJavaBooleanArray(env, kJankStatus, kJankStatusLen);

  RecordJankMetrics(
      env,
      /* java_durations_ns= */
      base::android::JavaParamRef<jlongArray>(env, java_durations),
      /* java_jank_status = */
      base::android::JavaParamRef<jbooleanArray>(env, java_jank_status),
      /* java_reporting_interval_start_time = */ 0,
      /* java_reporting_interval_duration = */ 1000);

  EXPECT_THAT(histogram_tester.GetAllSamples("Android.Jank.FrameDuration"),
              ElementsAre(Bucket(1, 3), Bucket(2, 1), Bucket(10, 1),
                          Bucket(20, 1), Bucket(29, 1), Bucket(57, 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples("Android.Jank.FrameJankStatus"),
              ElementsAre(Bucket(0, 2), Bucket(1, 6)));
}

}  // namespace base::android
