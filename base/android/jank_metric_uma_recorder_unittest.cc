// Copyright (c) 2021 The Chromium Authors. All rights reserved.
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

namespace base {
namespace android {
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
const size_t kDurationsLen = base::size(kDurations);

// Jank bursts are calculated based on durations.
const int64_t kJankBursts[] = {
    100'000'000,  // 100ms
    20'000'000,   // 20ms
};
const size_t kJankBurstsLen = base::size(kJankBursts);

}  // namespace

TEST(JankMetricUMARecorder, TestUMARecording) {
  HistogramTester histogram_tester;

  JNIEnv* env = AttachCurrentThread();

  jstring java_scenario_name =
      ConvertUTF8ToJavaString(env, "PeriodicReporting").Release();
  jlongArray java_durations =
      GenerateJavaLongArray(env, kDurations, kDurationsLen);
  jlongArray java_jank_bursts =
      GenerateJavaLongArray(env, kJankBursts, kJankBurstsLen);

  RecordJankMetrics(
      env,
      /* java_scenario_name= */
      base::android::JavaParamRef<jstring>(env, java_scenario_name),
      /* java_durations_ns= */
      base::android::JavaParamRef<jlongArray>(env, java_durations),
      /* java_jank_bursts_ns=*/
      base::android::JavaParamRef<jlongArray>(env, java_jank_bursts),
      /* java_missed_frames = */ 2);

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Android.Jank.FrameDuration.PeriodicReporting"),
              ElementsAre(Bucket(1, 3), Bucket(2, 1), Bucket(10, 1),
                          Bucket(20, 1), Bucket(29, 1), Bucket(57, 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Android.Jank.JankBursts.PeriodicReporting"),
              ElementsAre(Bucket(20, 1), Bucket(96, 1)));

  histogram_tester.ExpectUniqueSample(
      "Android.Jank.MissedFrames.PeriodicReporting", 2, 1);
}

}  // namespace android
}  // namespace base